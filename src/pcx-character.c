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

#include "pcx-character.h"

#include <assert.h>

#include "pcx-util.h"

enum pcx_text_string
pcx_character_get_name(enum pcx_character character)
{
        static const enum pcx_text_string names[] = {
                [PCX_CHARACTER_DUKE] =
                PCX_TEXT_STRING_CHARACTER_NAME_DUKE,
                [PCX_CHARACTER_ASSASSIN] =
                PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN,
                [PCX_CHARACTER_CONTESSA] =
                PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA,
                [PCX_CHARACTER_CAPTAIN] =
                PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN,
                [PCX_CHARACTER_AMBASSADOR] =
                PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR,
        };

        assert(character >= 0 && character < PCX_N_ELEMENTS(names));

        return names[character];
}

enum pcx_text_string
pcx_character_get_object_name(enum pcx_character character)
{
        static const enum pcx_text_string names[] = {
                [PCX_CHARACTER_DUKE] =
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE,
                [PCX_CHARACTER_ASSASSIN] =
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN,
                [PCX_CHARACTER_CONTESSA] =
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA,
                [PCX_CHARACTER_CAPTAIN] =
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN,
                [PCX_CHARACTER_AMBASSADOR] =
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR,
        };

        assert(character >= 0 && character < PCX_N_ELEMENTS(names));

        return names[character];
}
