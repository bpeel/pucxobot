/*
 * Pucxobot - A bot and website to play some card games
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

#ifndef PCX_DICTIONARY_H
#define PCX_DICTIONARY_H

#include <stdbool.h>
#include <stdint.h>

#include "pcx-error.h"

typedef void
(* pcx_dictionary_iterate_cb)(const char *word,
                              void *user_data);

struct pcx_dictionary *
pcx_dictionary_new(const char *filename,
                   struct pcx_error **error);

bool
pcx_dictionary_contains_word(struct pcx_dictionary *dict,
                             const char *word);

void
pcx_dictionary_iterate(struct pcx_dictionary *dict,
                       pcx_dictionary_iterate_cb cb,
                       void *user_data);

void
pcx_dictionary_free(struct pcx_dictionary *dict);

#endif /* PCX_DICTIONARY_H */
