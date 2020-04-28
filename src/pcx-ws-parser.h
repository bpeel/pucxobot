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

#ifndef PCX_WS_PARSER_H
#define PCX_WS_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "pcx-error.h"

extern struct pcx_error_domain
pcx_ws_parser_error;

enum pcx_ws_parser_error {
        PCX_WS_PARSER_ERROR_INVALID,
        PCX_WS_PARSER_ERROR_UNSUPPORTED,
        PCX_WS_PARSER_ERROR_CANCELLED
};

struct pcx_ws_parser_vtable {
        bool (* request_line_received)(const char *method,
                                       const char *uri,
                                       void *user_data);
        bool (* header_received)(const char *field_name,
                                 const char *value,
                                 void *user_data);
};

enum pcx_ws_parser_result {
        PCX_WS_PARSER_RESULT_NEED_MORE_DATA,
        PCX_WS_PARSER_RESULT_FINISHED,
        PCX_WS_PARSER_RESULT_ERROR
};

struct pcx_ws_parser *
pcx_ws_parser_new(const struct pcx_ws_parser_vtable *vtable,
                  void *user_data);

enum pcx_ws_parser_result
pcx_ws_parser_parse_data(struct pcx_ws_parser *parser,
                         const uint8_t *data,
                         size_t length,
                         size_t *consumed,
                         struct pcx_error **error);

void
pcx_ws_parser_free(struct pcx_ws_parser *parser);

#endif /* PCX_WS_PARSER_H */
