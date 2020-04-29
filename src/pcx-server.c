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

#include <json_object.h>
#include <json_tokener.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>

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

#define DEFAULT_PORT 3648

/* Number of microseconds of inactivity before a client will be
 * considered for garbage collection.
 */
#define MAX_CLIENT_AGE ((uint64_t) 2 * 60 * 1000000)

struct pcx_error_domain
pcx_server_error;

struct pcx_server {
        const struct pcx_config *config;
        int listen_sock;
        struct pcx_main_context_source *listen_source;
        struct pcx_main_context_source *gc_source;
        struct pcx_list clients;

        struct pcx_playerbase *playerbase;

        /* If there is a game that hasn’t started yet then it will be
         * stored here so that people can join it.
         */
        struct pcx_conversation *pending_conversation;
        struct pcx_listener pending_conversation_listener;
};

struct pcx_server_client {
        struct pcx_list link;
        struct pcx_connection *connection;
        struct pcx_listener event_listener;
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
        if (server->listen_source) {
                pcx_main_context_modify_poll(server->listen_source,
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
remove_pending_conversation(struct pcx_server *server)
{
        if (server->pending_conversation == NULL)
                return;

        pcx_list_remove(&server->pending_conversation_listener.link);
        pcx_conversation_unref(server->pending_conversation);
        server->pending_conversation = NULL;
}

static bool
pending_conversation_event_cb(struct pcx_listener *listener,
                              void *data)
{
        struct pcx_server *server =
               pcx_container_of(listener,
                                struct pcx_server,
                                pending_conversation_listener);
        const struct pcx_conversation_event *event = data;

        switch (event->type) {
        case PCX_CONVERSATION_EVENT_STARTED:
                remove_pending_conversation(server);
                break;
        case PCX_CONVERSATION_EVENT_PLAYER_ADDED:
                if (event->conversation->n_players >=
                    event->conversation->game_type->max_players)
                        remove_pending_conversation(server);
                break;
        case PCX_CONVERSATION_EVENT_PLAYER_REMOVED:
                /* If a player has been removed then the game will
                 * probably go wrong so let’s not let any random
                 * people discover it.
                 */
                remove_pending_conversation(server);
                break;
        case PCX_CONVERSATION_EVENT_NEW_MESSAGE:
                break;
        }

        return true;
}

static struct pcx_conversation *
ref_pending_conversation(struct pcx_server *server)
{
        if (server->pending_conversation == NULL) {
                struct pcx_conversation *conv =
                        pcx_conversation_new(server->config);
                server->pending_conversation = conv;
                pcx_signal_add(&conv->event_signal,
                               &server->pending_conversation_listener);
        }

        pcx_conversation_ref(server->pending_conversation);

        return server->pending_conversation;
}

static void
xor_bytes(uint64_t *id,
          const uint8_t *data,
          size_t data_length)
{
        uint8_t *id_bytes = (uint8_t *) id;
        int data_pos = 0;
        int i;

        for (i = 0; i < sizeof (*id); i++) {
                id_bytes[i] ^= data[data_pos];
                data_pos = (data_pos + 1) % data_length;
        }
}

static uint64_t
generate_id(const struct pcx_netaddress *remote_address)
{
        uint16_t random_data;
        uint64_t id = 0;
        int i;

        for (i = 0; i < sizeof id / sizeof random_data; i++) {
                random_data = rand();
                memcpy((uint8_t *) &id + i * sizeof random_data,
                       &random_data,
                       sizeof random_data);
        }

        /* XOR in the bytes of the client's address so that even if
         * the client can predict the random number sequence it'll
         * still be hard to guess a number of another client
         */
        xor_bytes(&id,
                  (uint8_t *) &remote_address->port,
                  sizeof remote_address->port);
        if (remote_address->family == AF_INET6)
                xor_bytes(&id,
                          (uint8_t *) &remote_address->ipv6,
                          sizeof remote_address->ipv6);
        else
                xor_bytes(&id,
                          (uint8_t *) &remote_address->ipv4,
                          sizeof remote_address->ipv4);

        return id;
}

static bool
handle_new_player(struct pcx_server *server,
                  struct pcx_server_client *client)
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

        const struct pcx_netaddress *remote_address =
                pcx_connection_get_remote_address(client->connection);
        uint64_t id;

        do {
                id = generate_id(remote_address);
        } while (pcx_playerbase_get_player_by_id(server->playerbase, id));

        struct pcx_conversation *conversation =
                ref_pending_conversation(server);

        player = pcx_playerbase_add_player(server->playerbase,
                                           conversation,
                                           id);

        pcx_connection_set_player(client->connection,
                                  player,
                                  false /* from_reconnect */);

        pcx_conversation_unref(conversation);

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

        /* If the client requested a player that doesn't exist then
         * divert it to a new player instead.
         */
        if (player == NULL)
                return handle_new_player(server, client);

        pcx_connection_set_player(client->connection,
                                  player,
                                  true /* from_reconnect */);

        return true;
}

static bool
handle_start(struct pcx_server *server,
             struct pcx_server_client *client)
{
        const char *remote_address_string =
                pcx_connection_get_remote_address_string(client->connection);
        struct pcx_player *player =
                pcx_connection_get_player(client->connection);

        if (player == NULL) {
                pcx_log("Client at %s sent a start message without sending "
                        "a hello",
                       remote_address_string);
                remove_client(server, client);
                return false;
        }

        pcx_conversation_start(player->conversation);

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

        pcx_conversation_push_button(player->conversation,
                                     player->player_num,
                                     event->button_data);

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

        case PCX_CONNECTION_EVENT_NEW_PLAYER:
                return handle_new_player(server, client);

        case PCX_CONNECTION_EVENT_RECONNECT: {
                struct pcx_connection_reconnect_event *de = (void *) event;
                return handle_reconnect(server, client, de);
        }

        case PCX_CONNECTION_EVENT_START:
                return handle_start(server, client);

        case PCX_CONNECTION_EVENT_LEAVE:
                return handle_leave(server, client);

        case PCX_CONNECTION_EVENT_BUTTON: {
                struct pcx_connection_button_event *de = (void *) event;
                return handle_button(server, client, de);
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
                                        DEFAULT_PORT)) {
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
listen_sock_cb(struct pcx_main_context_source *source,
               int fd,
               enum pcx_main_context_poll_flags flags,
               void *user_data)
{
        struct pcx_server *server = user_data;
        struct pcx_connection *conn;
        struct pcx_error *error = NULL;

        conn = pcx_connection_accept(fd, &error);

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
                        pcx_main_context_remove_source(source);
                        server->listen_source = NULL;
                }
                pcx_error_free(error);
                return;
        }

        pcx_log("Accepted connection from %s",
                pcx_connection_get_remote_address_string(conn));

        add_client(server, conn);
}

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               const struct pcx_config_server *server_config,
               struct pcx_error **error)
{
        int sock;

        if (server_config->address)
                sock = create_socket_for_address(server_config->address, error);
        else
                sock = create_socket_for_port(DEFAULT_PORT, error);

        if (sock == -1)
                return NULL;

        struct pcx_server *server = pcx_calloc(sizeof *server);

        pcx_list_init(&server->clients);

        server->playerbase = pcx_playerbase_new();

        server->config = config;
        server->listen_sock = sock;
        server->listen_source =
                pcx_main_context_add_poll(NULL,
                                          sock,
                                          PCX_MAIN_CONTEXT_POLL_IN,
                                          listen_sock_cb,
                                          server);

        server->pending_conversation_listener.notify =
                pending_conversation_event_cb;

        return server;
}

static void
free_clients(struct pcx_server *server)
{
        struct pcx_server_client *client, *tmp;

        pcx_list_for_each_safe(client, tmp, &server->clients, link)
                remove_client(server, client);
}

void
pcx_server_free(struct pcx_server *server)
{
        free_clients(server);

        remove_pending_conversation(server);

        pcx_playerbase_free(server->playerbase);

        if (server->gc_source)
                pcx_main_context_remove_source(server->gc_source);

        if (server->listen_source)
                pcx_main_context_remove_source(server->listen_source);
        if (server->listen_sock != -1)
                pcx_close(server->listen_sock);

        pcx_free(server);
}
