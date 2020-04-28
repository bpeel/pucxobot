/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2013, 2015, 2020  Neil Roberts
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

#include "pcx-connection.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <stdarg.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-buffer.h"
#include "pcx-log.h"
#include "pcx-file-error.h"
#include "pcx-socket.h"
#include "pcx-main-context.h"

struct pcx_connection {
        struct pcx_netaddress remote_address;
        char *remote_address_string;
        struct pcx_main_context_source *socket_source;
        int sock;

        uint8_t read_buf[1024];
        size_t read_buf_pos;

        uint8_t write_buf[1024];
        size_t write_buf_pos;

        struct pcx_signal event_signal;

        /* Last monotonic clock time when data was received on this
         * connection. This is used for garbage collection.
         */
        uint64_t last_update_time;
};

static bool
emit_event(struct pcx_connection *conn,
           enum pcx_connection_event_type type,
           struct pcx_connection_event *event)
{
        event->type = type;
        event->connection = conn;
        return pcx_signal_emit(&conn->event_signal, event);
}

static void
remove_sources(struct pcx_connection *conn)
{
        if (conn->socket_source) {
                pcx_main_context_remove_source(conn->socket_source);
                conn->socket_source = NULL;
        }
}

static void
set_error_state(struct pcx_connection *conn)
{
        struct pcx_connection_event event;

        /* Stop polling for further events */
        remove_sources(conn);

        emit_event(conn,
                   PCX_CONNECTION_EVENT_ERROR,
                   &event);
}

static void
handle_error(struct pcx_connection *conn)
{
        int value;
        unsigned int value_len = sizeof(value);

        if (getsockopt(conn->sock,
                       SOL_SOCKET,
                       SO_ERROR,
                       &value,
                       &value_len) == -1 ||
            value_len != sizeof(value) ||
            value == 0) {
                pcx_log("Unknown error on socket for %s",
                        conn->remote_address_string);
        } else {
                pcx_log("Error on socket for %s: %s",
                        conn->remote_address_string,
                        strerror(value));
        }

        set_error_state(conn);
}

static bool
connection_is_ready_to_write(struct pcx_connection *conn)
{
        if (conn->write_buf_pos > 0)
                return true;

        return false;
}

static void
update_poll_flags(struct pcx_connection *conn)
{
        enum pcx_main_context_poll_flags flags = PCX_MAIN_CONTEXT_POLL_IN;

        if (connection_is_ready_to_write(conn))
                flags |= PCX_MAIN_CONTEXT_POLL_OUT;

        pcx_main_context_modify_poll(conn->socket_source, flags);
}

static void
fill_write_buf(struct pcx_connection *conn)
{
}

static void
handle_read_error(struct pcx_connection *conn,
                  size_t got)
{
        if (got == 0) {
                pcx_log("Connection closed for %s",
                        conn->remote_address_string);
                set_error_state(conn);
        } else {
                enum pcx_file_error e = pcx_file_error_from_errno(errno);

                if (e != PCX_FILE_ERROR_AGAIN &&
                    e != PCX_FILE_ERROR_INTR) {
                        pcx_log("Error reading from socket for %s: %s",
                                conn->remote_address_string,
                                strerror(errno));
                        set_error_state(conn);
                }
        }
}

static void
handle_read(struct pcx_connection *conn)
{
        uint64_t now;
        ssize_t got;

        got = read(conn->sock,
                   conn->read_buf + conn->read_buf_pos,
                   sizeof conn->read_buf - conn->read_buf_pos);

        if (got <= 0) {
                handle_read_error(conn, got);
        } else {
                now = pcx_main_context_get_monotonic_clock(NULL);

                conn->last_update_time = now;
        }
}

static void
handle_write(struct pcx_connection *conn)
{
        int wrote;

        fill_write_buf(conn);

        wrote = write(conn->sock,
                      conn->write_buf,
                      conn->write_buf_pos);

        if (wrote == -1) {
                enum pcx_file_error e = pcx_file_error_from_errno(errno);

                if (e != PCX_FILE_ERROR_AGAIN &&
                    e != PCX_FILE_ERROR_INTR) {
                        pcx_log("Error writing to socket for %s: %s",
                                conn->remote_address_string,
                                strerror(errno));
                        set_error_state(conn);
                }
        } else {
                memmove(conn->write_buf,
                        conn->write_buf + wrote,
                        conn->write_buf_pos - wrote);
                conn->write_buf_pos -= wrote;

                update_poll_flags(conn);
        }
}

static void
connection_poll_cb(struct pcx_main_context_source *source,
                   int fd,
                   enum pcx_main_context_poll_flags flags,
                   void *user_data)
{
        struct pcx_connection *conn = user_data;

        if (flags & PCX_MAIN_CONTEXT_POLL_ERROR)
                handle_error(conn);
        else if (flags & PCX_MAIN_CONTEXT_POLL_IN)
                handle_read(conn);
        else if (flags & PCX_MAIN_CONTEXT_POLL_OUT)
                handle_write(conn);
}

void
pcx_connection_free(struct pcx_connection *conn)
{
        remove_sources(conn);

        pcx_free(conn->remote_address_string);
        pcx_close(conn->sock);

        pcx_free(conn);
}

static struct pcx_connection *
new_for_socket(int sock,
               const struct pcx_netaddress *remote_address)
{
        struct pcx_connection *conn;

        conn = pcx_calloc(sizeof *conn);

        conn->sock = sock;
        conn->remote_address = *remote_address;
        conn->remote_address_string = pcx_netaddress_to_string(remote_address);

        pcx_signal_init(&conn->event_signal);

        conn->socket_source =
                pcx_main_context_add_poll(NULL, /* context */
                                          sock,
                                          PCX_MAIN_CONTEXT_POLL_IN,
                                          connection_poll_cb,
                                          conn);

        conn->last_update_time = pcx_main_context_get_monotonic_clock(NULL);

        return conn;
}

struct pcx_signal *
pcx_connection_get_event_signal(struct pcx_connection *conn)
{
        return &conn->event_signal;
}

const char *
pcx_connection_get_remote_address_string(struct pcx_connection *conn)
{
        return conn->remote_address_string;
}

const struct pcx_netaddress *
pcx_connection_get_remote_address(struct pcx_connection *conn)
{
        return &conn->remote_address;
}

struct pcx_connection *
pcx_connection_accept(int server_sock,
                      struct pcx_error **error)
{
        struct pcx_netaddress address;
        struct pcx_netaddress_native native_address;
        int sock;

        native_address.length = sizeof native_address.sockaddr_in6;

        sock = accept(server_sock,
                      &native_address.sockaddr,
                      &native_address.length);

        if (sock == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Error accepting connection: %s",
                                   strerror(errno));
                return NULL;
        }

        if (!pcx_socket_set_nonblock(sock, error)) {
                pcx_close(sock);
                return NULL;
        }

        pcx_netaddress_from_native(&address, &native_address);

        return new_for_socket(sock, &address);
}

uint64_t
pcx_connection_get_last_update_time(struct pcx_connection *conn)
{
        return conn->last_update_time;
}
