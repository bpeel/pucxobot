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

#ifndef PCX_SERVER_H
#define PCX_SERVER_H

#include "pcx-config.h"

struct pcx_server;

extern struct pcx_error_domain
pcx_server_error;

enum pcx_server_error {
        PCX_SERVER_ERROR_INVALID_ADDRESS
};

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               const struct pcx_config_server *server_config,
               struct pcx_error **error);

int
pcx_server_get_n_players(struct pcx_server *server);

void
pcx_server_free(struct pcx_server *server);

#endif /* PCX_SERVER_H */
