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

#include "pcx-server.h"

#include <json_object.h>
#include <json_tokener.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-config.h"
#include "pcx-buffer.h"

struct pcx_error_domain
pcx_server_error;

struct pcx_server_response {
        struct pcx_list link;
        struct json_object *object;
};

struct pcx_server_connection {
        struct pcx_list link;

        struct pcx_server *server;

        int sock;
        struct pcx_main_context_source *source;

        /* This becomes true when we've received something from the
         * client that we don't understand and we're ignoring any
         * further input
         */
        bool had_bad_input;
        /* This becomes true when the client has closed its end of the
         * connection
         */
        bool read_finished;
        /* This becomes true when we've stopped writing data. This
         * will only happen after the client closes its connection or
         * we've had bad input and we're ignoring further data
         */
        bool write_finished;
        /* Set to true if we’ve sent some data to the tokener without
         * receiving a complete object. In that case it is invalid for
         * the client to close their writing end.
         */
        bool partial_object;

        /* Queue of pcx_server_response to write to the connection */
        struct pcx_list response_queue;
        /* How much data is in write_buf */
        size_t output_length;
        /* How much of the first response has already been copied into
         * write_buf
         */
        size_t partial_response_written;

        uint8_t write_buf[1024];

        struct json_tokener *tokener;
};

struct pcx_server {
        const struct pcx_config *config;

        char *address;

        struct pcx_main_context_source *server_source;

        int sock;
        bool socket_bound;

        struct pcx_list connections;

        /* Temporary buffer for reading data to feed into the tokener */
        char read_buf[1024];
};

static bool
set_nonblock(int sock,
             struct pcx_error **error)
{
        int flags;

        flags = fcntl(sock, F_GETFL, 0);

        if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error setting non-blocking mode: %s",
                              strerror(errno));
                return false;
        }

        return true;
}

static void
remove_response(struct pcx_server_response *response)
{
        json_object_put(response->object);
        pcx_list_remove(&response->link);
        pcx_free(response);
}

static void
remove_responses(struct pcx_server_connection *connection)
{
        struct pcx_server_response *response, *tmp;

        pcx_list_for_each_safe(response, tmp,
                               &connection->response_queue,
                               link) {
                remove_response(response);
        }
}

static void
remove_connection(struct pcx_server_connection *connection)
{
        remove_responses(connection);

        if (connection->tokener)
                json_tokener_free(connection->tokener);
        if (connection->source)
                pcx_main_context_remove_source(connection->source);
        if (connection->sock != -1)
                close(connection->sock);

        pcx_list_remove(&connection->link);

        pcx_free(connection);
}

static void
update_poll(struct pcx_server_connection *connection)
{
        enum pcx_main_context_poll_flags flags = 0;

        if (!connection->read_finished)
                flags |= PCX_MAIN_CONTEXT_POLL_IN;

        /* Shutdown the socket if we've finished writing */
        if (!connection->write_finished &&
            (connection->read_finished || connection->had_bad_input) &&
            pcx_list_empty(&connection->response_queue) &&
            connection->output_length == 0) {
                if (shutdown(connection->sock, SHUT_WR) == -1) {
                        remove_connection(connection);
                        return;
                }

                connection->write_finished = true;
        }

        /* If both ends of the connection are closed then we can
         * abandon this connection
         */
        if (connection->read_finished && connection->write_finished) {
                remove_connection(connection);
                return;
        }

        if (!connection->write_finished) {
                if (connection->output_length > 0 ||
                    !pcx_list_empty(&connection->response_queue))
                        flags |= PCX_MAIN_CONTEXT_POLL_OUT;
        }

        pcx_main_context_modify_poll(connection->source, flags);
}

static void
queue_response(struct pcx_server_connection *connection,
               struct json_object *obj)
{
        struct pcx_server_response *response = pcx_alloc(sizeof *response);

        response->object = json_object_get(obj);

        pcx_list_insert(connection->response_queue.prev, &response->link);
}

static void
set_bad_input(struct pcx_server_connection *connection)
{
        struct pcx_server_response *partial_response = NULL;

        /* If we are part way through sending a response then keep it */
        if (connection->partial_response_written) {
                partial_response =
                        pcx_container_of(connection->response_queue.next,
                                         struct pcx_server_response,
                                         link);
                pcx_list_remove(&partial_response->link);
        }

        /* Remove all other responses */
        remove_responses(connection);

        /* Add the first one back */
        if (partial_response) {
                pcx_list_insert(&connection->response_queue,
                                &partial_response->link);
        }

        json_object *str = json_object_new_string("Invalid request");
        json_object *obj = json_object_new_object();
        json_object_object_add(obj, "error", str);

        queue_response(connection, obj);

        json_object_put(obj);

        connection->had_bad_input = true;
}

static void
handle_request(struct pcx_server_connection *connection,
               struct json_object *object,
               bool *need_update)
{
        /* Stub handler just queues the request as a response */
        queue_response(connection, object);

        *need_update = true;
}

static void
parse_json_objects(struct pcx_server_connection *connection,
                   size_t buf_size,
                   const char *buf,
                   bool *need_update)
{
        while (true) {
                struct json_object *object =
                        json_tokener_parse_ex(connection->tokener,
                                              buf,
                                              buf_size);

                if (object == NULL) {
                        enum json_tokener_error error =
                                json_tokener_get_error(connection->tokener);

                        if (error == json_tokener_continue) {
                                connection->partial_object = true;
                        } else {
                                set_bad_input(connection);
                                *need_update = true;
                        }
                        break;
                }

                buf_size -= connection->tokener->char_offset;
                buf += connection->tokener->char_offset;

                handle_request(connection, object, need_update);

                json_object_put(object);

                json_tokener_reset(connection->tokener);

                if (buf_size <= 0) {
                        connection->partial_object = false;
                        break;
                }
        }
}

static void
fill_write_buf(struct pcx_server_connection *connection)
{
        while (connection->output_length < sizeof connection->write_buf &&
               !pcx_list_empty(&connection->response_queue)) {
                struct pcx_server_response *response =
                        pcx_container_of(connection->response_queue.next,
                                         struct pcx_server_response,
                                         link);

                const char *str = json_object_to_json_string(response->object);
                size_t len = strlen(str);
                size_t to_write =
                        MIN((sizeof connection->write_buf) -
                            connection->output_length,
                            len - connection->partial_response_written);

                memcpy(connection->write_buf +
                       connection->output_length,
                       str + connection->partial_response_written,
                       to_write);

                if (to_write + connection->partial_response_written >= len) {
                        connection->partial_response_written = 0;
                        remove_response(response);
                } else {
                        connection->partial_response_written += to_write;
                }

                connection->output_length += to_write;
        }
}

static void
connection_cb(struct pcx_main_context_source *source,
              int fd,
              enum pcx_main_context_poll_flags flags,
              void *user_data)
{
        struct pcx_server_connection *connection = user_data;
        struct pcx_server *server = connection->server;
        bool need_update = false;

        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR)) {
                remove_connection (connection);
                return;
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_IN)) {
                int got = read(fd,
                               server->read_buf,
                               sizeof server->read_buf);

                if (got == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                remove_connection(connection);
                                return;
                        }
                } else if (got == 0) {
                        connection->read_finished = true;
                        if (!connection->had_bad_input &&
                            connection->partial_object)
                                set_bad_input(connection);
                        need_update = true;
                } else {
                        if (!connection->had_bad_input) {
                                parse_json_objects(connection,
                                                   got,
                                                   server->read_buf,
                                                   &need_update);
                        }
                }
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_OUT)) {
                fill_write_buf(connection);

                int wrote = write(fd,
                                  connection->write_buf,
                                  connection->output_length);

                if (wrote == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                remove_connection(connection);
                                return;
                        }
                } else {
                        memmove(connection->write_buf,
                                connection->write_buf + wrote,
                                connection->output_length - wrote);
                        connection->output_length -= wrote;

                        need_update = true;
                }
        }

        if (need_update)
                update_poll(connection);
}

static void
server_socket_cb(struct pcx_main_context_source *source,
                 int fd,
                 enum pcx_main_context_poll_flags flags,
                 void *user_data)
{
        struct pcx_server *server = user_data;

        assert(server->server_source == source);
        assert(server->sock == fd);

        int client_fd = accept(fd, NULL, NULL);

        if (client_fd == -1) {
                switch (errno) {
                case EAGAIN:
#if EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                        break;
                case EMFILE:
                        /* Run out of file descriptors. Stop listening
                         * until someone disconnects.
                         */
                        pcx_main_context_modify_poll(source, 0);
                        break;
                default:
                        /* Some other error that is probably going to
                         * mean the poll is broken. Let’s give up
                         * listening on the socket.
                         */
                        pcx_main_context_remove_source(source);
                        server->server_source = NULL;
                        break;
                }

                return;
        }

        struct pcx_error *error = NULL;

        if (!set_nonblock(client_fd, &error)) {
                pcx_error_free(error);
                close(client_fd);
                return;
        }

        struct pcx_server_connection *connection =
                pcx_calloc(sizeof *connection);

        connection->server = server;
        connection->sock = client_fd;
        connection->tokener = json_tokener_new();
        pcx_list_init(&connection->response_queue);
        connection->source =
                pcx_main_context_add_poll(NULL,
                                          client_fd,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          connection_cb,
                                          connection);

        pcx_list_insert(&server->connections, &connection->link);
}

static void
delete_existing_socket(const char *address)
{
        struct stat statbuf;

        if (stat(address, &statbuf) == -1)
                return;

        if (!S_ISSOCK(statbuf.st_mode))
                return;

        unlink(address);
}

static bool
create_socket(struct pcx_server *server,
              struct pcx_error **error)
{
        server->sock = socket(AF_LOCAL, SOCK_STREAM, 0);

        if (server->sock == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error creating socket: %s",
                              strerror(errno));
                return false;
        }

        if (!set_nonblock(server->sock, error))
                return false;

        size_t address_size = (offsetof(struct sockaddr_un, sun_path) +
                               strlen(server->address));

        struct sockaddr_un *addr = pcx_alloc(address_size + 1);
        addr->sun_family = AF_LOCAL;
        strcpy(addr->sun_path, server->address);

        int ret;

        ret = bind(server->sock, (struct sockaddr *) addr, address_size);

        pcx_free(addr);

        if (ret == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error binding socket: %s",
                              strerror(errno));
                return false;
        }

        server->socket_bound = true;

        ret = listen(server->sock, 10);

        if (ret == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error making socket listen: %s",
                              strerror(errno));
                return false;
        }

        return true;
}

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               const struct pcx_config_server *server_config,
               struct pcx_error **error)
{
        struct pcx_server *server = pcx_calloc(sizeof *server);

        server->config = config;
        server->sock = -1;
        server->address = pcx_strdup(server_config->address);
        pcx_list_init(&server->connections);

        delete_existing_socket(server_config->address);

        if (!create_socket(server, error))
                goto error;

        server->server_source =
                pcx_main_context_add_poll(NULL,
                                          server->sock,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          server_socket_cb,
                                          server);

        return server;

error:
        pcx_server_free(server);
        return NULL;
}

static void
remove_connections(struct pcx_server *server)
{
        struct pcx_server_connection *connection, *tmp;

        pcx_list_for_each_safe(connection, tmp, &server->connections, link) {
                remove_connection(connection);
        }
}

void
pcx_server_free(struct pcx_server *server)
{
        remove_connections(server);

        if (server->server_source)
                pcx_main_context_remove_source(server->server_source);

        if (server->sock != -1)
                close(server->sock);

        if (server->socket_bound) {
                /* Try to clean up the local socket file. It doesn’t
                 * really matter if this fails.
                 */
                unlink(server->address);
        }

        pcx_free(server->address);

        pcx_free(server);
}
