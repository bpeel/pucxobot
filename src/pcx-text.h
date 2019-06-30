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

#ifndef PCX_TEXT_H
#define PCX_TEXT_H

#include <stdarg.h>

#include "pcx-buffer.h"

enum pcx_text_language {
        PCX_TEXT_LANGUAGE_ESPERANTO,
};

enum pcx_text_string {
        PCX_TEXT_STRING_TIMEOUT_START,
        PCX_TEXT_STRING_TIMEOUT_ABANDON,
        PCX_TEXT_STRING_NEED_PUBLIC_GROUP,
        PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE,
        PCX_TEXT_STRING_ALREADY_IN_GAME,
        PCX_TEXT_STRING_GAME_FULL,
        PCX_TEXT_STRING_GAME_ALREADY_STARTED,
        PCX_TEXT_STRING_NAME_FROM_ID,
        PCX_TEXT_STRING_FINAL_CONJUNCTION,
        PCX_TEXT_STRING_WELCOME,
        PCX_TEXT_STRING_JOIN_BEFORE_START,
        PCX_TEXT_STRING_NEED_TWO_PLAYERS,
        PCX_TEXT_STRING_JOIN_COMMAND,
        PCX_TEXT_STRING_START_COMMAND,
        PCX_TEXT_STRING_HELP_COMMAND,
        PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE,
};

const char *
pcx_text_get(enum pcx_text_language lang,
             enum pcx_text_string string);

#endif /* PCX_TEXT_H */
