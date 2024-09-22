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

#include "config.h"

#include "pcx-wordparty-help.h"

#include "pcx-text.h"

const char *
pcx_wordparty_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>Vortofesto</b>\n"
        "\n"
        "Dum via vico, la roboto donos al vi kombinon de 2 aŭ 3 literoj. Vi "
        "devas simple rapide respondi per vorto kiu enhavas tiujn literojn. "
        "Se vi ne respondas ene de la tempolimo vi perdos unu vivon. Se vi "
        "perdas ĉiujn vivojn vi perdas la partion. La lasta vivanta ludanto "
        "venkas.\n"
        "\n"
        "La vorto devas esti valida vorto laŭ la vortaro de la ludo. Vi ne "
        "povas reuzi la saman vorton en unu partio.\n"
        "\n"
        "Se vi uzas ĉiujn literojn de la alfabeto vi povas regajni vivon. Do "
        "indas provi elpensi longajn vortojn kun maloftaj literoj.\n"
        "\n"
        "La longeco de la tempolimo dependas de la malfacileco de la literoj "
        "kaj la tuta daŭro de la partio.\n",
};
