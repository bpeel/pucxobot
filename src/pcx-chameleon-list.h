/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2022  Neil Roberts
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

#ifndef PCX_CHAMELEON_LIST_H
#define PCX_CHAMELEON_LIST_H

#include <stdbool.h>
#include <stdint.h>

#include "pcx-error.h"

struct pcx_chameleon_list {
        int stub;
};

struct pcx_chameleon_list *
pcx_chameleon_list_new(const char *filename,
                       struct pcx_error **error);

void
pcx_chameleon_list_free(struct pcx_chameleon_list *word_list);

#endif /* PCX_CHAMELEON_LIST_H */
