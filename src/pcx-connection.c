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

#include "config.h"

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
#include "pcx-ws-parser.h"
#include "pcx-base64.h"
#include "sha1.h"

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

        /* This is freed and becomes NULL once the headers have all
         * been parsed.
         */
        struct pcx_ws_parser *ws_parser;
        /* This is allocated temporarily between getting the WebSocket
         * key header and finishing all of the headers.
         */
        SHA1_CTX *sha1_ctx;
};

static const char
ws_sec_key_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static const char
ws_header_prefix[] =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: ";

static const char
ws_header_postfix[] = "\r\n\r\n";

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
process_frames(struct pcx_connection *conn)
{
        conn->read_buf_pos = 0;
}

static bool
ws_request_line_received_cb(const char *method,
                            const char *uri,
                            void *user_data)
{
        return true;
}

static bool
ws_header_received_cb(const char *field_name,
                      const char *value,
                      void *user_data)
{
        struct pcx_connection *conn = user_data;

        if (!pcx_ascii_string_case_equal(field_name, "sec-websocket-key"))
                return true;

        if (conn->sha1_ctx != NULL) {
                pcx_log("Client at %s sent a WebSocket header with multiple "
                        "Sec-WebSocket-Key headers",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        conn->sha1_ctx = pcx_alloc(sizeof *conn->sha1_ctx);
        SHA1Init(conn->sha1_ctx);
        SHA1Update(conn->sha1_ctx, (const uint8_t *) value, strlen(value));

        return true;
}

static bool
ws_headers_finished(struct pcx_connection *conn)
{
        uint8_t sha1_hash[SHA1_DIGEST_LENGTH];
        size_t encoded_size;

        if (conn->sha1_ctx == NULL) {
                pcx_log("Client at %s sent a WebSocket header without a "
                        "Sec-WebSocket-Key header",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        SHA1Update(conn->sha1_ctx,
                   (const uint8_t *) ws_sec_key_guid,
                   sizeof ws_sec_key_guid - 1);
        SHA1Final(sha1_hash, conn->sha1_ctx);
        pcx_free(conn->sha1_ctx);
        conn->sha1_ctx = NULL;

        /* Send the WebSocket protocol response. This is the first
         * thing we'll send to the client so there should always be
         * enough space in the write buffer.
         */

        {
                _Static_assert(PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH) +
                               sizeof ws_header_prefix - 1 +
                               sizeof ws_header_postfix - 1 <=
                               sizeof conn->write_buf,
                               "The write buffer is too small to contain the "
                               "WebSocket protocol reply");
        }

        memcpy(conn->write_buf,
               ws_header_prefix,
               sizeof ws_header_prefix - 1);
        encoded_size = pcx_base64_encode(sha1_hash,
                                         sizeof sha1_hash,
                                         (char *) conn->write_buf +
                                         sizeof ws_header_prefix - 1);
        assert(encoded_size == PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH));
        memcpy(conn->write_buf + sizeof ws_header_prefix - 1 + encoded_size,
               ws_header_postfix,
               sizeof ws_header_postfix - 1);

        conn->write_buf_pos = (PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH) +
                               sizeof ws_header_prefix - 1 +
                               sizeof ws_header_postfix - 1);

        update_poll_flags(conn);

        return true;
}

static const struct pcx_ws_parser_vtable
ws_parser_vtable = {
        .request_line_received = ws_request_line_received_cb,
        .header_received = ws_header_received_cb
};

static void
handle_ws_data(struct pcx_connection *conn,
               size_t got)
{
        struct pcx_error *error = NULL;
        enum pcx_ws_parser_result result;
        size_t consumed;

        result = pcx_ws_parser_parse_data(conn->ws_parser,
                                          conn->read_buf,
                                          got,
                                          &consumed,
                                          &error);

        switch (result) {
        case PCX_WS_PARSER_RESULT_NEED_MORE_DATA:
                break;
        case PCX_WS_PARSER_RESULT_FINISHED:
                pcx_ws_parser_free(conn->ws_parser);
                conn->ws_parser = NULL;
                memmove(conn->read_buf,
                        conn->read_buf + consumed,
                        got - consumed);
                conn->read_buf_pos = got - consumed;

                if (ws_headers_finished(conn))
                        process_frames(conn);
                break;
        case PCX_WS_PARSER_RESULT_ERROR:
                if (error->domain != &pcx_ws_parser_error ||
                    error->code != PCX_WS_PARSER_ERROR_CANCELLED) {
                        pcx_log("WebSocket protocol error from %s: %s",
                                conn->remote_address_string,
                                error->message);
                        set_error_state(conn);
                }
                pcx_error_free(error);
                break;
        }
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

                if (conn->ws_parser) {
                        handle_ws_data(conn, got);
                } else {
                        conn->read_buf_pos += got;

                        process_frames(conn);
                }
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

        if (conn->ws_parser)
                pcx_free(conn->ws_parser);

        if (conn->sha1_ctx)
                pcx_free(conn->sha1_ctx);

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
        conn->ws_parser = pcx_ws_parser_new(&ws_parser_vtable, conn);

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
