/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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
#include <stdlib.h>

#include "pcx-error.h"
#include "pcx-list.h"

extern struct pcx_error_domain
pcx_chameleon_list_error;

enum pcx_chameleon_list_error {
        PCX_CHAMELEON_LIST_ERROR_EMPTY,
};

struct pcx_chameleon_list;

struct pcx_chameleon_list_group {
        char *topic;
        struct pcx_list words;
};

struct pcx_chameleon_list_word {
        struct pcx_list link;
        /* over-allocated */
        char word[1];
};

struct pcx_chameleon_list *
pcx_chameleon_list_new(const char *filename,
                       struct pcx_error **error);

size_t
pcx_chameleon_list_get_n_groups(struct pcx_chameleon_list *word_list);

const struct pcx_chameleon_list_group *
pcx_chameleon_list_get_group(struct pcx_chameleon_list *word_list,
                             int group);

void
pcx_chameleon_list_free(struct pcx_chameleon_list *word_list);

#endif /* PCX_CHAMELEON_LIST_H */
