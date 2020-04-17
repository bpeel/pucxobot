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

        struct pcx_buffer buf;
};

struct pcx_server {
        const struct pcx_config *config;

        char *address;

        struct pcx_main_context_source *server_source;

        int sock;
        bool socket_bound;

        struct pcx_list connections;
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
remove_connection(struct pcx_server_connection *connection)
{
        if (connection->source)
                pcx_main_context_remove_source(connection->source);
        if (connection->sock != -1)
                close(connection->sock);

        pcx_buffer_destroy(&connection->buf);

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
            connection->buf.length == 0) {
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
                if (connection->buf.length > 0)
                        flags |= PCX_MAIN_CONTEXT_POLL_OUT;
        }

        pcx_main_context_modify_poll(connection->source, flags);
}

static void
connection_cb(struct pcx_main_context_source *source,
              int fd,
              enum pcx_main_context_poll_flags flags,
              void *user_data)
{
        struct pcx_server_connection *connection = user_data;
        bool need_update = false;

        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR)) {
                remove_connection (connection);
                return;
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_IN)) {
                pcx_buffer_ensure_size(&connection->buf,
                                       connection->buf.length + 512);

                int got = read(fd,
                               connection->buf.data,
                               connection->buf.size - connection->buf.length);

                if (got == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                remove_connection(connection);
                                return;
                        }
                } else if (got == 0) {
                        connection->read_finished = true;
                        need_update = true;
                } else {
                        if (connection->buf.length == 0)
                                need_update = true;
                        connection->buf.length += got;
                }
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_OUT)) {
                int wrote = write(fd,
                                  connection->buf.data,
                                  connection->buf.length);

                if (wrote == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                remove_connection(connection);
                                return;
                        }
                } else {
                        memmove(connection->buf.data,
                                connection->buf.data + wrote,
                                connection->buf.length - wrote);
                        connection->buf.length -= wrote;

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
        pcx_buffer_init(&connection->buf);
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
