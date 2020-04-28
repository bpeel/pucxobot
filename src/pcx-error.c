/*
 * Notbit - A Bitmessage client
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

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "pcx-error.h"
#include "pcx-util.h"

static struct pcx_error *
pcx_error_new(int buf_size)
{
        return pcx_alloc(offsetof(struct pcx_error, message) +
                         buf_size);
}

void
pcx_set_error_va_list(struct pcx_error **error_out,
                      struct pcx_error_domain *domain,
                      int code,
                      const char *format,
                      va_list ap)
{
        struct pcx_error *error;
        va_list apcopy;
        size_t buf_size, required_size;

        if (error_out == NULL)
                /* Error is being ignored */
                return;

        if (*error_out) {
                pcx_warning("Multiple exceptions occured without "
                            "being handled");
                return;
        }

        buf_size = 64;

        error = pcx_error_new(buf_size);

        va_copy(apcopy, ap);
        required_size = vsnprintf(error->message, buf_size, format, ap);

        if (required_size >= buf_size) {
                pcx_free(error);
                buf_size = required_size + 1;
                error = pcx_error_new(buf_size);
                vsnprintf(error->message, buf_size, format, apcopy);
        }

        va_end(apcopy);

        error->domain = domain;
        error->code = code;
        *error_out = error;
}

void
pcx_set_error(struct pcx_error **error_out,
              struct pcx_error_domain *domain,
              int code,
              const char *format,
              ...)
{
        va_list ap;

        va_start(ap, format);
        pcx_set_error_va_list(error_out, domain, code, format, ap);
        va_end(ap);
}

void
pcx_error_free(struct pcx_error *error)
{
        pcx_free(error);
}

void
pcx_error_clear(struct pcx_error **error)
{
        pcx_error_free(*error);
        *error = NULL;
}

void
pcx_error_propagate(struct pcx_error **error,
                    struct pcx_error *other)
{
        assert(other != NULL);

        if (error)
                *error = other;
        else
                pcx_error_free(other);
}
