/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2021  Neil Roberts
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

#include "pcx-ssl-error.h"

#include <openssl/err.h>

#include "pcx-buffer.h"

struct pcx_error_domain
pcx_ssl_error;

enum pcx_ssl_error
pcx_ssl_error_from_errno(unsigned long errnum)
{
        return PCX_SSL_ERROR_OTHER;
}

void
pcx_ssl_error_set(struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        unsigned long errnum = ERR_get_error();

        pcx_buffer_append_string(&buf, "SSL error: ");
        pcx_buffer_ensure_size(&buf, buf.length + 200);
        ERR_error_string(errnum, (char *) buf.data + buf.length);

        pcx_set_error(error,
                      &pcx_ssl_error,
                      pcx_ssl_error_from_errno(errnum),
                      "%s",
                      (char *) buf.data);

        pcx_buffer_destroy(&buf);
}
