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

#ifndef PCX_COUP_CHARACTER_H
#define PCX_COUP_CHARACTER_H

#include "pcx-text.h"

enum pcx_coup_character {
        PCX_COUP_CHARACTER_DUKE,
        PCX_COUP_CHARACTER_ASSASSIN,
        PCX_COUP_CHARACTER_CONTESSA,
        PCX_COUP_CHARACTER_CAPTAIN,
        PCX_COUP_CHARACTER_AMBASSADOR
};

#define PCX_COUP_CHARACTER_COUNT 5

enum pcx_text_string
pcx_coup_character_get_name(enum pcx_coup_character character);

enum pcx_text_string
pcx_coup_character_get_object_name(enum pcx_coup_character character);

#endif /* PCX_COUP_CHARACTER_H */
