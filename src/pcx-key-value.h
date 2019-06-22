/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2013, 2014, 2019  Neil Roberts
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef PCX_KEY_VALUE_H
#define PCX_KEY_VALUE_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

enum pcx_key_value_event {
        PCX_KEY_VALUE_EVENT_HEADER,
        PCX_KEY_VALUE_EVENT_PROPERTY
};

typedef void
(* pcx_key_value_func)(enum pcx_key_value_event event,
                       int line_number,
                       const char *key,
                       const char *value,
                       void *user_data);

typedef void
(* pcx_key_value_error_func)(const char *message,
                             void *user_data);

void
pcx_key_value_load(FILE *file,
                   pcx_key_value_func func,
                   pcx_key_value_error_func error_func,
                   void *user_data);

bool
pcx_key_value_parse_bool_value(int line_number,
                               const char *value,
                               bool *result);

bool
pcx_key_value_parse_int_value(int line_number,
                              const char *value_string,
                              int64_t max,
                              int64_t *result);

#endif /* PCX_KEY_VALUE_H */
