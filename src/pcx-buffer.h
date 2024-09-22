/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2013  Neil Roberts
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

#ifndef PCX_BUFFER_H
#define PCX_BUFFER_H

#include <stdint.h>
#include <stdarg.h>

#include "pcx-util.h"

struct pcx_buffer {
        uint8_t *data;
        size_t length;
        size_t size;
};

#define PCX_BUFFER_STATIC_INIT { .data = NULL, .length = 0, .size = 0 }

void
pcx_buffer_init(struct pcx_buffer *buffer);

void
pcx_buffer_ensure_size(struct pcx_buffer *buffer,
                       size_t size);

void
pcx_buffer_set_length(struct pcx_buffer *buffer,
                      size_t length);

PCX_PRINTF_FORMAT(2, 3) void
pcx_buffer_append_printf(struct pcx_buffer *buffer,
                         const char *format,
                         ...);

void
pcx_buffer_append_vprintf(struct pcx_buffer *buffer,
                          const char *format,
                          va_list ap);

void
pcx_buffer_append(struct pcx_buffer *buffer,
                  const void *data,
                  size_t length);

static inline void
pcx_buffer_append_c(struct pcx_buffer *buffer,
                    char c)
{
        if (buffer->size > buffer->length)
                buffer->data[buffer->length++] = c;
        else
                pcx_buffer_append(buffer, &c, 1);
}

void
pcx_buffer_append_string(struct pcx_buffer *buffer,
                         const char *str);

void
pcx_buffer_destroy(struct pcx_buffer *buffer);

#endif /* PCX_BUFFER_H */
