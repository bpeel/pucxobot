/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2011, 2015, 2020  Neil Roberts
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

#include "pcx-ws-parser.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "pcx-util.h"

#define PCX_WS_PARSER_MAX_LINE_LENGTH 512

struct pcx_ws_parser {
        unsigned int buf_len;

        uint8_t buf[PCX_WS_PARSER_MAX_LINE_LENGTH];

        enum {
                PCX_WS_PARSER_READING_REQUEST_LINE,
                PCX_WS_PARSER_TERMINATING_REQUEST_LINE,
                PCX_WS_PARSER_READING_HEADER,
                PCX_WS_PARSER_TERMINATING_HEADER,
                PCX_WS_PARSER_CHECKING_HEADER_CONTINUATION,
                PCX_WS_PARSER_DONE
        } state;

        const struct pcx_ws_parser_vtable *vtable;
        void *user_data;
};

struct pcx_error_domain
pcx_ws_parser_error;

struct pcx_ws_parser *
pcx_ws_parser_new(const struct pcx_ws_parser_vtable *vtable,
                  void *user_data)
{
        struct pcx_ws_parser *parser = pcx_alloc(sizeof *parser);

        parser->buf_len = 0;
        parser->state = PCX_WS_PARSER_READING_REQUEST_LINE;
        parser->vtable = vtable;
        parser->user_data = user_data;

        return parser;
}

static bool
check_http_version(const uint8_t *data,
                   unsigned int length,
                   struct pcx_error **error)
{
        static const char prefix[] = "HTTP/1.";

        /* This accepts any 1.x version */

        if (length < sizeof(prefix) ||
            memcmp(data, prefix, sizeof(prefix) - 1))
                goto bad;

        data += sizeof(prefix) - 1;
        length -= sizeof(prefix) - 1;

        /* The remaining characters should all be digits */
        while (length > 0) {
                if (!pcx_ascii_isdigit(data[--length]))
                        goto bad;
        }

        return true;

bad:
        pcx_set_error(error,
                      &pcx_ws_parser_error,
                      PCX_WS_PARSER_ERROR_UNSUPPORTED,
                      "Unsupported HTTP version");
        return false;
}

static void
set_cancelled_error(struct pcx_error **error)
{
        pcx_set_error(error,
                      &pcx_ws_parser_error,
                      PCX_WS_PARSER_ERROR_CANCELLED,
                      "Application cancelled parsing");
}

static bool
add_bytes_to_buffer(struct pcx_ws_parser *parser,
                    const uint8_t *data,
                    unsigned int length,
                    struct pcx_error **error)
{
        if (parser->buf_len + length > PCX_WS_PARSER_MAX_LINE_LENGTH) {
                pcx_set_error(error,
                              &pcx_ws_parser_error,
                              PCX_WS_PARSER_ERROR_UNSUPPORTED,
                              "Unsupported line length in HTTP request");
                return false;
        } else {
                memcpy(parser->buf + parser->buf_len, data, length);
                parser->buf_len += length;

                return true;
        }
}

static bool
process_request_line(struct pcx_ws_parser *parser,
                     uint8_t *data,
                     unsigned int length,
                     struct pcx_error **error)
{
        uint8_t *method_end;
        uint8_t *uri_end;
        const char *method = (char *) data;
        const char *uri;

        method_end = memchr(data, ' ', length);

        if (method_end == NULL) {
                pcx_set_error(error,
                              &pcx_ws_parser_error,
                              PCX_WS_PARSER_ERROR_INVALID,
                              "Invalid HTTP request received");
                return false;
        }

        /* Replace the space with a zero terminator so we can reuse
         * the buffer to pass to the callback
         */
        *method_end = '\0';

        length -= method_end - data + 1;
        data = method_end + 1;

        uri = (const char *) data;
        uri_end = memchr(data, ' ', length);

        if (uri_end == NULL) {
                pcx_set_error(error,
                              &pcx_ws_parser_error,
                              PCX_WS_PARSER_ERROR_INVALID,
                              "Invalid HTTP request received");
                return false;
        }

        *uri_end = '\0';

        length -= uri_end - data + 1;
        data = uri_end + 1;

        if (!check_http_version(data, length, error))
                return false;

        if (!parser->vtable->request_line_received(method,
                                                   uri,
                                                   parser->user_data)) {
                set_cancelled_error(error);
                return false;
        }

        return true;
}

static bool
process_header(struct pcx_ws_parser *parser,
               struct pcx_error **error)
{
        uint8_t *data = parser->buf;
        unsigned int length = parser->buf_len;
        const char *field_name = (char *)data;
        const char *value;
        uint8_t zero = '\0';
        uint8_t *field_name_end;

        field_name_end = memchr(data, ':', length);

        if (field_name_end == NULL) {
                pcx_set_error(error,
                              &pcx_ws_parser_error,
                              PCX_WS_PARSER_ERROR_INVALID,
                              "Invalid HTTP request received");
                return false;
        }

        /* Replace the colon with a zero terminator so we can reuse
         * the buffer to pass to the callback
         */
        *field_name_end = '\0';

        length -= field_name_end - data + 1;
        data = field_name_end + 1;

        /* Skip leading spaces */
        while (length > 0 && *data == ' ') {
                length--;
                data++;
        }

        value = (char *) data;

        /* Add a terminator so we can pass it to the callback */
        if (!add_bytes_to_buffer(parser, &zero, 1, error))
                return false;

        if (!parser->vtable->header_received(field_name,
                                             value,
                                             parser->user_data)) {
                set_cancelled_error(error);
                return false;
        }

        return true;
}

struct pcx_ws_parser_closure {
        const uint8_t *data;
        unsigned int length;
};

static bool
handle_reading_request_line(struct pcx_ws_parser *parser,
                            struct pcx_ws_parser_closure *c,
                            struct pcx_error **error)
{
        const uint8_t *terminator;

        /* Could the data contain a terminator? */
        if ((terminator = memchr(c->data, '\r', c->length))) {
                /* Add the data up to the potential terminator */
                if (!add_bytes_to_buffer(parser,
                                         c->data,
                                         terminator - c->data,
                                         error))
                        return false;

                /* Consume those bytes */
                c->length -= terminator - c->data + 1;
                c->data = terminator + 1;

                parser->state = PCX_WS_PARSER_TERMINATING_REQUEST_LINE;
        } else {
                /* Add and consume all of the data */
                if (!add_bytes_to_buffer(parser,
                                         c->data,
                                         c->length,
                                         error))
                        return false;

                c->length = 0;
        }

        return true;
}

static bool
handle_terminating_request_line(struct pcx_ws_parser *parser,
                                struct pcx_ws_parser_closure *c,
                                struct pcx_error **error)
{
        /* Do we have the \n needed to complete the terminator? */
        if (c->data[0] == '\n') {
                /* Apparently some clients send a '\r\n' after sending
                 * the request body. We can handle this by just
                 * ignoring empty lines before the request line
                 */
                if (parser->buf_len == 0)
                        parser->state =
                                PCX_WS_PARSER_READING_REQUEST_LINE;
                else {
                        if (!process_request_line(parser,
                                                  parser->buf,
                                                  parser->
                                                  buf_len,
                                                  error))
                                return false;

                        parser->buf_len = 0;
                        /* Start processing headers */
                        parser->state = PCX_WS_PARSER_READING_HEADER;
                }

                /* Consume the \n */
                c->data++;
                c->length--;
        } else {
                uint8_t r = '\r';
                /* Add the \r that we ignored when switching to this
                 * state and then switch back to reading the request
                 * line without consuming the char
                 */
                if (!add_bytes_to_buffer(parser, &r, 1, error))
                        return false;
                parser->state =
                        PCX_WS_PARSER_READING_REQUEST_LINE;
        }

        return true;
}

static bool
handle_reading_header(struct pcx_ws_parser *parser,
                      struct pcx_ws_parser_closure *c,
                      struct pcx_error **error)
{
        const uint8_t *terminator;

        /* Could the data contain a terminator? */
        if ((terminator = memchr(c->data, '\r', c->length))) {
                /* Add the data up to the potential terminator */
                if (!add_bytes_to_buffer(parser,
                                         c->data,
                                         terminator - c->data,
                                         error))
                        return false;

                /* Consume those bytes */
                c->length -= terminator - c->data + 1;
                c->data = terminator + 1;

                parser->state = PCX_WS_PARSER_TERMINATING_HEADER;
        } else {
                /* Add and consume all of the data */
                if (!add_bytes_to_buffer(parser,
                                         c->data,
                                         c->length,
                                         error))
                        return false;

                c->length = 0;
        }

        return true;
}

static bool
handle_terminating_header(struct pcx_ws_parser *parser,
                          struct pcx_ws_parser_closure *c,
                          struct pcx_error **error)
{
        /* Do we have the \n needed to complete the terminator? */
        if (c->data[0] == '\n') {
                /* If the header is empty then this marks the end of the
                 * headers
                 */
                if (parser->buf_len == 0) {
                        parser->state = PCX_WS_PARSER_DONE;
                } else {
                        /* Start checking for a continuation */
                        parser->state =
                                PCX_WS_PARSER_CHECKING_HEADER_CONTINUATION;
                }

                /* Consume the \n */
                c->data++;
                c->length--;
        } else {
                uint8_t r = '\r';
                /* Add the \r that we ignored when switching to this
                 * state and then switch back to reading the header
                 * without consuming the char
                 */
                if (!add_bytes_to_buffer(parser, &r, 1, error))
                        return false;
                parser->state = PCX_WS_PARSER_READING_HEADER;
        }

        return true;
}

static bool
handle_checking_header_continuation(struct pcx_ws_parser *parser,
                                    struct pcx_ws_parser_closure *c,
                                    struct pcx_error **error)
{
        /* Do we have a continuation character? */
        if (c->data[0] == ' ') {
                /* Yes, continue processing headers */
                parser->state = PCX_WS_PARSER_READING_HEADER;
                /* We don't consume the character so that the space
                 * will be added to the buffer
                 */
        } else {
                /* We have a complete header */
                if (!process_header(parser, error))
                        return false;

                parser->buf_len = 0;
                parser->state = PCX_WS_PARSER_READING_HEADER;
        }

        return true;
}

enum pcx_ws_parser_result
pcx_ws_parser_parse_data(struct pcx_ws_parser *parser,
                         const uint8_t *data,
                         size_t length,
                         size_t *consumed,
                         struct pcx_error **error)
{
        struct pcx_ws_parser_closure closure;

        closure.data = data;
        closure.length = length;

        while (closure.length > 0) {
                switch (parser->state) {
                case PCX_WS_PARSER_READING_REQUEST_LINE:
                        if (!handle_reading_request_line(parser,
                                                         &closure,
                                                         error))
                                return PCX_WS_PARSER_RESULT_ERROR;
                        break;

                case PCX_WS_PARSER_TERMINATING_REQUEST_LINE:
                        if (!handle_terminating_request_line(parser,
                                                             &closure,
                                                             error))
                                return PCX_WS_PARSER_RESULT_ERROR;
                        break;

                case PCX_WS_PARSER_READING_HEADER:
                        if (!handle_reading_header(parser,
                                                   &closure,
                                                   error))
                                return PCX_WS_PARSER_RESULT_ERROR;
                        break;

                case PCX_WS_PARSER_TERMINATING_HEADER:
                        if (!handle_terminating_header(parser,
                                                       &closure,
                                                       error))
                                return PCX_WS_PARSER_RESULT_ERROR;
                        break;

                case PCX_WS_PARSER_CHECKING_HEADER_CONTINUATION:
                        if (!handle_checking_header_continuation(parser,
                                                                 &closure,
                                                                 error))
                                return PCX_WS_PARSER_RESULT_ERROR;
                        break;

                case PCX_WS_PARSER_DONE:
                        *consumed = closure.data - data;
                        return PCX_WS_PARSER_RESULT_FINISHED;
                }
        }

        if (parser->state == PCX_WS_PARSER_DONE) {
                *consumed = length;
                return PCX_WS_PARSER_RESULT_FINISHED;
        }

        return PCX_WS_PARSER_RESULT_NEED_MORE_DATA;
}

void
pcx_ws_parser_free(struct pcx_ws_parser *parser)
{
        pcx_free(parser);
}
