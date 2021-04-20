/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

#ifndef PCX_SSL_ERROR_H
#define PCX_SSL_ERROR_H

#include "pcx-error.h"

extern struct pcx_error_domain
pcx_ssl_error;

enum pcx_ssl_error {
        PCX_SSL_ERROR_OTHER
};

enum pcx_ssl_error
pcx_ssl_error_from_errno(unsigned long errnum);

void
pcx_ssl_error_set(struct pcx_error **error);

#endif /* PCX_SSL_ERROR_H */
