/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "pcx-server.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>
#include <openssl/ssl.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-config.h"
#include "pcx-buffer.h"
#include "pcx-socket.h"
#include "pcx-file-error.h"
#include "pcx-netaddress.h"
#include "pcx-connection.h"
#include "pcx-signal.h"
#include "pcx-log.h"
#include "pcx-conversation.h"
#include "pcx-playerbase.h"
#include "pcx-proto.h"
#include "pcx-generate-id.h"
#include "pcx-ssl-error.h"

#define DEFAULT_PORT 3648
#define DEFAULT_SSL_PORT (DEFAULT_PORT + 1)

/* Number of microseconds of inactivity before a client will be
 * considered for garbage collection.
 */
#define MAX_CLIENT_AGE ((uint64_t) 2 * 60 * 1000000)

struct pcx_error_domain
pcx_server_error;

struct pcx_server {
        const struct pcx_config *config;
        struct pcx_class_store *class_store;
        struct pcx_main_context_source *gc_source;
        struct pcx_list sockets;
        struct pcx_list clients;

        struct pcx_playerbase *playerbase;

        /* If there is a game that hasn’t started yet then it will be
         * stored here so that people can join it.
         */
        struct pcx_list pending_conversations;
};

struct pcx_server_client {
        struct pcx_list link;
        struct pcx_connection *connection;
        struct pcx_listener event_listener;
        struct pcx_server *server;
};

struct pcx_server_pending_conversation {
        struct pcx_list link;
        struct pcx_conversation *conversation;
        struct pcx_listener listener;
};

struct pcx_server_socket {
        struct pcx_list link;
        int listen_sock;
        struct pcx_main_context_source *listen_source;
        SSL_CTX *ssl_ctx;
        struct pcx_server *server;
};

static void
queue_gc_source(struct pcx_server *server);

static void
remove_client(struct pcx_server *server,
              struct pcx_server_client *client)
{
        pcx_connection_free(client->connection);

        pcx_list_remove(&client->link);
        pcx_free(client);

        /* If we remove a connection then any previous disabled accept
         * might start working again.
         */
        struct pcx_server_socket *ssocket;

        pcx_list_for_each(ssocket, &server->sockets, link) {
                if (ssocket->listen_source == NULL)
                        continue;

                pcx_main_context_modify_poll(ssocket->listen_source,
                                             PCX_MAIN_CONTEXT_POLL_IN |
                                             PCX_MAIN_CONTEXT_POLL_ERROR);
        }
}

static void
gc_cb(struct pcx_main_context_source *source,
      void *user_data)
{
        struct pcx_server *server = user_data;
        uint64_t now = pcx_main_context_get_monotonic_clock(NULL);
        struct pcx_server_client *client, *tmp;
        bool client_remaining = false;

        server->gc_source = NULL;

        pcx_list_for_each_safe(client, tmp, &server->clients, link) {
                struct pcx_connection *conn = client->connection;
                uint64_t update_time =
                        pcx_connection_get_last_update_time(conn);

                if (now - update_time >= MAX_CLIENT_AGE) {
                        pcx_log("Removing connection from %s which has been "
                                "idle for %i seconds",
                                pcx_connection_get_remote_address_string(conn),
                                (int) ((now - update_time) / 1000000));
                        remove_client(server, client);
                } else {
                        client_remaining = true;
                }
        }

        if (client_remaining)
                queue_gc_source(server);
}

static void
queue_gc_source(struct pcx_server *server)
{
        if (server->gc_source)
                return;

        server->gc_source =
                pcx_main_context_add_timeout(NULL,
                                             MAX_CLIENT_AGE / 1000 + 1,
                                             gc_cb,
                                             server);
}

static void
remove_pending_conversation(struct pcx_server_pending_conversation *pc)
{
        pcx_list_remove(&pc->listener.link);
        pcx_conversation_unref(pc->conversation);
        pcx_list_remove(&pc->link);
        pcx_free(pc);
}

static bool
pending_conversation_event_cb(struct pcx_listener *listener,
                              void *data)
{
        struct pcx_server_pending_conversation *pc =
               pcx_container_of(listener,
                                struct pcx_server_pending_conversation,
                                listener);
        const struct pcx_conversation_event *event = data;

        switch (event->type) {
        case PCX_CONVERSATION_EVENT_STARTED:
                remove_pending_conversation(pc);
                break;
        case PCX_CONVERSATION_EVENT_PLAYER_ADDED:
                /* If the game is full then it will start
                 * automatically and the conversation will be removed
                 * from the pending conversation so we don’t need to
                 * handle it here.
                 */
                break;
        case PCX_CONVERSATION_EVENT_PLAYER_REMOVED:
                /* If a player has been removed then the game will
                 * probably go wrong so let’s not let any random
                 * people discover it.
                 */
                remove_pending_conversation(pc);
                break;
        case PCX_CONVERSATION_EVENT_NEW_MESSAGE:
        case PCX_CONVERSATION_EVENT_SIDEBAND_DATA_MODIFIED:
                break;
        }

        return true;
}

static struct pcx_server_pending_conversation *
add_pending_conversation(struct pcx_server *server,
                         const struct pcx_game *game_type,
                         enum pcx_text_language language)
{
        struct pcx_conversation *conv =
                pcx_conversation_new(server->config,
                                     server->class_store,
                                     game_type,
                                     language);
        struct pcx_server_pending_conversation *pc = pcx_alloc(sizeof *pc);

        pc->conversation = conv;
        pc->listener.notify = pending_conversation_event_cb;
        pcx_signal_add(&conv->event_signal,
                       &pc->listener);
        pcx_list_insert(&server->pending_conversations, &pc->link);

        return pc;
}

static struct pcx_conversation *
get_pending_conversation(struct pcx_server *server,
                         const struct pcx_game *game_type,
                         enum pcx_text_language language)
{
        struct pcx_server_pending_conversation *pc;

        pcx_list_for_each(pc, &server->pending_conversations, link) {
                if (!pc->conversation->is_private &&
                    pc->conversation->game_type == game_type &&
                    pc->conversation->language == language)
                        goto found_conversation;
        }

        pc = add_pending_conversation(server, game_type, language);

found_conversation:
        return pc->conversation;
}

static struct pcx_server_pending_conversation *
find_private_conversation(struct pcx_server *server,
                          uint64_t id)
{
        struct pcx_server_pending_conversation *pc;

        pcx_list_for_each(pc, &server->pending_conversations, link) {
                if (pc->conversation->is_private &&
                    pc->conversation->private_game_id == id)
                        return pc;
        }

        return NULL;
}

static struct pcx_conversation *
add_private_conversation(struct pcx_server *server,
                         const struct pcx_game *game_type,
                         enum pcx_text_language language,
                         const struct pcx_netaddress *remote_address)
{
        struct pcx_server_pending_conversation *pc =
                add_pending_conversation(server, game_type, language);

        pc->conversation->is_private = true;

        uint64_t id;

        do {
                id = pcx_generate_id(remote_address);
        } while (find_private_conversation(server, id));

        pc->conversation->private_game_id = id;

        return pc->conversation;
}

static char *
normalise_string(const char *name)
{
        size_t name_len;

        /* Skip spaces at the beginning */
        while (*name == ' ')
                name++;

        name_len = strlen(name);

        while (name_len > 0 && name[name_len - 1] == ' ')
                name_len--;

        if (name_len == 0)
                return NULL;

        /* Check if the name has any suspicious characters */
        for (unsigned i = 0; i < name_len; i++) {
                if (name[i] >= 0 && name[i] < ' ')
                        return NULL;
        }

        return pcx_strndup(name, name_len);
}

static void
watch_conversation(struct pcx_server *server,
                   struct pcx_server_client *client,
                   const char *name,
                   struct pcx_conversation *conversation)
{
        const struct pcx_netaddress *remote_address =
                pcx_connection_get_remote_address(client->connection);

        uint64_t id;

        do {
                id = pcx_generate_id(remote_address);
        } while (pcx_playerbase_get_player_by_id(server->playerbase, id));

        struct pcx_player *player =
                pcx_playerbase_add_player(server->playerbase,
                                          conversation,
                                          name,
                                          id);

        pcx_connection_set_player(client->connection,
                                  player,
                                  0 /* n_messages_received */);
}

static bool
handle_new_player(struct pcx_server *server,
                  struct pcx_server_client *client,
                  const struct pcx_connection_new_player_event *event)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player != NULL) {
                pcx_log("Client %s sent multiple hello messages",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        if (pcx_text_get(event->language,
                         PCX_TEXT_STRING_START_BUTTON) == NULL ||
            pcx_text_get(event->language,
                         event->game_type->start_command) == NULL) {
                pcx_log("Client %s tried to start a game in a language "
                        "that it hasn’t been translated to",
                        remote_address_string);
                remove_client(server, client);
                return false;
        }

        char *normalised_name = normalise_string(event->name);

        if (normalised_name == NULL) {
                pcx_log("Client %s sent an invalid name",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        struct pcx_conversation *conversation;

        if (event->is_private) {
                const struct pcx_netaddress *remote_address =
                        pcx_connection_get_remote_address(client->connection);
                conversation = add_private_conversation(server,
                                                        event->game_type,
                                                        event->language,
                                                        remote_address);
        } else {
                conversation = get_pending_conversation(server,
                                                        event->game_type,
                                                        event->language);
        }

        watch_conversation(server, client, normalised_name, conversation);

        pcx_free(normalised_name);

        return true;
}

static bool
handle_join_private_game(struct pcx_server *server,
                         struct pcx_server_client *client,
                         const struct pcx_connection_join_private_game_event *e)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);

        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player != NULL) {
                pcx_log("Client %s sent multiple hello messages",
                        remote_address_string);
                remove_client(server, client);
                return false;
        }

        struct pcx_server_pending_conversation *pc =
                find_private_conversation(server, e->game_id);

        if (pc == NULL) {
                int msg = PCX_PROTO_PRIVATE_GAME_NOT_FOUND;
                if (!pcx_connection_send_message(client->connection, msg)) {
                        pcx_log("Couldn’t send game not found message to %s",
                                remote_address_string);
                        remove_client(server, client);
                        return false;
                }
                return true;
        }

        char *normalised_name = normalise_string(e->name);

        if (normalised_name == NULL) {
                pcx_log("Client %s sent an invalid name",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        watch_conversation(server, client, normalised_name, pc->conversation);

        pcx_free(normalised_name);

        return true;
}

static bool
handle_reconnect(struct pcx_server *server,
                 struct pcx_server_client *client,
                 struct pcx_connection_reconnect_event *event)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player != NULL) {
                pcx_log("Client %s sent multiple hello messages",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        player = pcx_playerbase_get_player_by_id(server->playerbase,
                                                 event->player_id);

        if (player == NULL) {
                pcx_log("Client %s tried to reconnect to a non-existent player",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        pcx_connection_set_player(client->connection,
                                  player,
                                  event->n_messages_received);

        return true;
}

static bool
handle_leave(struct pcx_server *server,
             struct pcx_server_client *client)
{
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player == NULL)
                return true;

        if (player->has_left)
                return true;

        player->has_left = true;

        pcx_conversation_remove_player(player->conversation,
                                       player->player_num);

        return true;
}

static bool
handle_button(struct pcx_server *server,
              struct pcx_server_client *client,
              struct pcx_connection_button_event *event)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player == NULL) {
                pcx_log("Client %s sent a button command before a hello",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        if (player->has_left) {
                pcx_log("Client %s tried to push a button after leaving",
                        remote_address_string);
                remove_client(server, client);
                return false;
        }

        pcx_conversation_push_button(player->conversation,
                                     player->player_num,
                                     event->button_data);

        return true;
}

static bool
handle_send_message(struct pcx_server *server,
                    struct pcx_server_client *client,
                    const struct pcx_connection_send_message_event *event)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player == NULL) {
                pcx_log("Client %s sent a chat message before sending "
                        "a hello message",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        if (player->has_left) {
                pcx_log("Client %s tried to sent a chat message after leaving",
                        remote_address_string);
                remove_client(server, client);
                return false;
        }

        char *normalised_text = normalise_string(event->text);

        if (normalised_text == NULL) {
                pcx_log("Client %s sent an invalid chat message",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        pcx_conversation_add_chat_message(player->conversation,
                                          player->player_num,
                                          normalised_text);

        pcx_free(normalised_text);

        return true;
}

static bool
connection_event_cb(struct pcx_listener *listener,
                    void *data)
{
        struct pcx_connection_event *event = data;
        struct pcx_server_client *client =
                pcx_container_of(listener,
                                 struct pcx_server_client,
                                 event_listener);
        struct pcx_server *server = client->server;

        switch (event->type) {
        case PCX_CONNECTION_EVENT_ERROR:
                remove_client(server, client);
                return false;

        case PCX_CONNECTION_EVENT_NEW_PLAYER: {
                struct pcx_connection_new_player_event *de = (void *) event;
                return handle_new_player(server, client, de);
        }

        case PCX_CONNECTION_EVENT_JOIN_PRIVATE_GAME: {
                struct pcx_connection_join_private_game_event *de =
                        (void *) event;
                return handle_join_private_game(server, client, de);
        }

        case PCX_CONNECTION_EVENT_RECONNECT: {
                struct pcx_connection_reconnect_event *de = (void *) event;
                return handle_reconnect(server, client, de);
        }

        case PCX_CONNECTION_EVENT_LEAVE:
                return handle_leave(server, client);

        case PCX_CONNECTION_EVENT_BUTTON: {
                struct pcx_connection_button_event *de = (void *) event;
                return handle_button(server, client, de);
        }

        case PCX_CONNECTION_EVENT_SEND_MESSAGE: {
                struct pcx_connection_send_message_event *de = (void *) event;
                return handle_send_message(server, client, de);
        }

        }

        return true;
}

static int
create_socket_for_netaddress(const struct pcx_netaddress *netaddress,
                             struct pcx_error **error)
{
        struct pcx_netaddress_native native_address;
        const int true_value = true;
        int sock;

        pcx_netaddress_to_native(netaddress, &native_address);

        sock = socket(native_address.sockaddr.sa_family == AF_INET6 ?
                      PF_INET6 : PF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to create socket: %s",
                                   strerror(errno));
                return -1;
        }

        setsockopt(sock,
                   SOL_SOCKET, SO_REUSEADDR,
                   &true_value, sizeof true_value);

        if (!pcx_socket_set_nonblock(sock, error))
                goto error;

        if (bind(sock, &native_address.sockaddr, native_address.length) == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to bind socket: %s",
                                   strerror(errno));
                goto error;
        }

        if (listen(sock, 10) == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to make socket listen: %s",
                                   strerror(errno));
                goto error;
        }

        return sock;

error:
        pcx_close(sock);
        return false;
}

static int
create_socket_for_port(int port,
                       struct pcx_error **error)
{
        struct pcx_netaddress netaddress;

        memset(&netaddress, 0, sizeof netaddress);

        /* First try binding it with an IPv6 address */
        netaddress.port = port;
        netaddress.family = AF_INET6;

        struct pcx_error *local_error = NULL;

        int sock = create_socket_for_netaddress(&netaddress, &local_error);

        if (sock != -1)
                return sock;

        if (local_error->domain == &pcx_file_error &&
            (local_error->code == PCX_FILE_ERROR_PFNOSUPPORT ||
             local_error->code == PCX_FILE_ERROR_AFNOSUPPORT)) {
                pcx_error_free(local_error);
        } else {
                pcx_error_propagate(error, local_error);
                return -1;
        }

        /* Some servers disable IPv6 so try IPv4 */
        netaddress.family = AF_INET;

        return create_socket_for_netaddress(&netaddress, error);
}

static int
create_socket_for_address(const char *address,
                          int default_port,
                          struct pcx_error **error)
{
        unsigned long port;
        char *tail;

        errno = 0;
        port = strtoul(address, &tail, 0);
        if (errno == 0 && port <= UINT16_MAX && *tail == '\0')
                return create_socket_for_port(port, error);

        struct pcx_netaddress netaddress;

        if (!pcx_netaddress_from_string(&netaddress,
                                        address,
                                        default_port)) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_INVALID_ADDRESS,
                              "The listen address %s is invalid",
                              address);
                return -1;
        }

        return create_socket_for_netaddress(&netaddress, error);
}

static struct pcx_server_client *
add_client(struct pcx_server *server,
           struct pcx_connection *conn)
{
        struct pcx_server_client *client = pcx_calloc(sizeof *client);

        client->server = server;
        client->connection = conn;

        struct pcx_signal *
                command_signal = pcx_connection_get_event_signal(conn);
        pcx_signal_add(command_signal, &client->event_listener);
        client->event_listener.notify = connection_event_cb;

        pcx_list_insert(&server->clients, &client->link);

        queue_gc_source(server);

        return client;
}

static void
free_server_socket(struct pcx_server_socket *ssocket)
{
        if (ssocket->ssl_ctx)
                SSL_CTX_free(ssocket->ssl_ctx);
        if (ssocket->listen_source)
                pcx_main_context_remove_source(ssocket->listen_source);
        if (ssocket->listen_sock != -1)
                pcx_close(ssocket->listen_sock);

        pcx_list_remove(&ssocket->link);

        pcx_free(ssocket);
}

static void
listen_sock_cb(struct pcx_main_context_source *source,
               int fd,
               enum pcx_main_context_poll_flags flags,
               void *user_data)
{
        struct pcx_server_socket *ssocket = user_data;
        struct pcx_server *server = ssocket->server;
        struct pcx_connection *conn;
        struct pcx_error *error = NULL;

        conn = pcx_connection_accept(ssocket->ssl_ctx, fd, &error);

        if (conn == NULL) {
                if (error->domain == &pcx_file_error &&
                    error->code == PCX_FILE_ERROR_MFILE) {
                        pcx_log("Accept failed due to too many open fds. "
                                "Waiting for a client to disconnect.");
                        /* Run out of file descriptors. Stop listening
                         * until someone disconnects.
                         */
                        pcx_main_context_modify_poll(source, 0);
                } else if (error->domain != &pcx_file_error ||
                           (error->code != PCX_FILE_ERROR_AGAIN &&
                            error->code != PCX_FILE_ERROR_INTR)) {
                        pcx_log("%s", error->message);
                        free_server_socket(ssocket);
                }
                pcx_error_free(error);
                return;
        }

        pcx_log("Accepted connection from %s",
                pcx_connection_get_remote_address_string(conn));

        add_client(server, conn);
}

int
pcx_server_get_n_players(struct pcx_server *server)
{
        return pcx_playerbase_get_n_players(server->playerbase);
}

static int
ssl_password_cb(char *buf, int size, int rwflag, void *user_data)
{
        const struct pcx_config_server *server_config = user_data;

        if (server_config->private_key_password == NULL)
                return -1;

        size_t length = strlen(server_config->private_key_password);

        if (length > size)
                return -1;

        memcpy(buf, server_config->private_key_password, length);

        return length;
}

static bool
init_ssl(struct pcx_server_socket *ssocket,
         const struct pcx_config_server *server_config,
         struct pcx_error **error)
{
        ssocket->ssl_ctx = SSL_CTX_new(TLS_server_method());
        if (ssocket->ssl_ctx == NULL)
                goto error;

        SSL_CTX_set_default_passwd_cb(ssocket->ssl_ctx, ssl_password_cb);
        SSL_CTX_set_default_passwd_cb_userdata(ssocket->ssl_ctx,
                                               (void *) server_config);

        if (SSL_CTX_use_certificate_file(ssocket->ssl_ctx,
                                         server_config->certificate,
                                         SSL_FILETYPE_PEM) <= 0)
                goto error;

        if (SSL_CTX_use_PrivateKey_file(ssocket->ssl_ctx,
                                        server_config->private_key,
                                        SSL_FILETYPE_PEM) <= 0)
                goto error;

        SSL_CTX_set_mode(ssocket->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

        return true;

error:
        pcx_ssl_error_set(error);
        return false;
}

bool
pcx_server_add_config(struct pcx_server *server,
                      const struct pcx_config_server *server_config,
                      struct pcx_error **error)
{
        int default_port = (server_config->certificate ?
                            DEFAULT_SSL_PORT :
                            DEFAULT_PORT);

        int sock;

        if (server_config->address) {
                sock = create_socket_for_address(server_config->address,
                                                 default_port,
                                                 error);
        } else {
                sock = create_socket_for_port(default_port, error);
        }

        if (sock == -1)
                return false;

        struct pcx_server_socket *ssocket = pcx_calloc(sizeof *ssocket);

        pcx_list_insert(&server->sockets, &ssocket->link);
        ssocket->server = server;
        ssocket->listen_sock = sock;
        ssocket->listen_source =
                pcx_main_context_add_poll(NULL,
                                          sock,
                                          PCX_MAIN_CONTEXT_POLL_IN,
                                          listen_sock_cb,
                                          ssocket);

        if (server_config->certificate &&
            !init_ssl(ssocket, server_config, error)) {
                free_server_socket(ssocket);
                return false;
        }

        return server;
}

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               struct pcx_class_store *class_store)
{
        struct pcx_server *server = pcx_calloc(sizeof *server);

        pcx_list_init(&server->clients);
        pcx_list_init(&server->sockets);

        server->playerbase = pcx_playerbase_new();

        server->config = config;
        server->class_store = class_store;

        pcx_list_init(&server->pending_conversations);

        return server;
}

static void
free_clients(struct pcx_server *server)
{
        struct pcx_server_client *client, *tmp;

        pcx_list_for_each_safe(client, tmp, &server->clients, link)
                remove_client(server, client);
}

static void
free_sockets(struct pcx_server *server)
{
        struct pcx_server_socket *ssocket, *tmp;

        pcx_list_for_each_safe(ssocket, tmp, &server->sockets, link)
                free_server_socket(ssocket);
}

static void
remove_pending_conversations(struct pcx_server *server)
{
        struct pcx_server_pending_conversation *pc, *tmp;

        pcx_list_for_each_safe(pc, tmp, &server->pending_conversations, link) {
                remove_pending_conversation(pc);
        }
}

void
pcx_server_free(struct pcx_server *server)
{
        free_clients(server);
        free_sockets(server);

        remove_pending_conversations(server);

        pcx_playerbase_free(server->playerbase);

        if (server->gc_source)
                pcx_main_context_remove_source(server->gc_source);


        pcx_free(server);
}
