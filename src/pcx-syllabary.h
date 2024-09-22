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

#ifndef PCX_SYLLABARY_H
#define PCX_SYLLABARY_H

#include <stdbool.h>

#include "pcx-error.h"
#include "pcx-utf8.h"

/* Maxmium length in bytes of a syllable */
#define PCX_SYLLABARY_MAX_SYLLABLE_LENGTH (3 * PCX_UTF8_MAX_CHAR_LENGTH)

/* Maximum (exclusive) value of the difficulty for a syllable */
#define PCX_SYLLABARY_MAX_DIFFICULTY 100

struct pcx_syllabary *
pcx_syllabary_new(const char *filename,
                  struct pcx_error **error);

/* Gets a random syllable from the syllabary and stores the letters in
 * syllable. The easier syllables are more likely. The difficulty
 * parameter is a number in the range 0,MAX_DIFFICULTY. It is based on
 * how many words the syllable appears in.
 */
bool
pcx_syllabary_get_random(struct pcx_syllabary *syllabary,
                         char syllable[PCX_SYLLABARY_MAX_SYLLABLE_LENGTH + 1],
                         int *difficulty);

void
pcx_syllabary_free(struct pcx_syllabary *syllabary);

#endif /* PCX_SYLLABARY_H */
