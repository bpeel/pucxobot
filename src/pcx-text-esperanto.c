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

#include "pcx-text-esperanto.h"

#include "pcx-text.h"

const char *
pcx_text_esperanto[] = {
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Neniu aliĝis dum pli ol %i minutoj. La "
        "ludo tuj komenciĝos.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "La ludo estas senaktiva dum pli ol "
        "%i minutoj kaj estos forlasita.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "Bonvolu aliĝi al ludo en publika grupo.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Bonvolu sendi privatan mesaĝon al @%s "
        "por ke mi povu sendi al vi viajn kartojn "
        "private.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "Vi jam estas en ludo",
        [PCX_TEXT_STRING_GAME_FULL] =
        "La ludo jam estas plena",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "La ludo jam komenciĝis",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "Sr.%i",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " kaj ",
        [PCX_TEXT_STRING_WELCOME] =
        "Bonvenon. Aliaj ludantoj tajpu "
        "/aligxi por aliĝi al la ludo aŭ tajpu /komenci "
        "por komenci la ludon. La aktualaj ludantoj "
        "estas:\n"
        "%s",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Aliĝu al la ludo per /aligxi antaŭ ol "
        "komenci ĝin",
        [PCX_TEXT_STRING_NEED_TWO_PLAYERS] =
        "Necesas almenaŭ 2 ludantoj por ludi.",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/aligxi",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/komenci",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/helpo",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Dankon pro la mesaĝo. Vi povas nun aliĝi "
        "al ludo en la ĉefa grupo.",
};
