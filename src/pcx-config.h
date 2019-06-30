/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2019  Neil Roberts
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

#ifndef PCX_CONFIG_H
#define PCX_CONFIG_H

#include "pcx-error.h"
#include "pcx-list.h"

extern struct pcx_error_domain
pcx_config_error;

enum pcx_config_error {
        PCX_CONFIG_ERROR_IO
};

struct pcx_config_bot {
        struct pcx_list link;
        char *apikey;
        char *botname;
        char *announce_channel;
};

struct pcx_config {
        struct pcx_list bots;
};

struct pcx_config *
pcx_config_load(struct pcx_error **error);

void
pcx_config_free(struct pcx_config *config);

#endif /* PCX_CONFIG_H */
