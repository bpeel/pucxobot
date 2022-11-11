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

#include "config.h"

#include "pcx-chameleon-help.h"

#include "pcx-text.h"

const char *
pcx_chameleon_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>Kameleono</b>\n"
        "\n"
        "En ĉiu vico unu ludanto estas hazarde nomita la kameleono. "
        "Neniu scias kiu tiu estas, krom la kameleono mem. Sekve "
        "estas listo de vortoj kun iu temo. Unu el la vortoj estas la "
        "sekreta vorto. Ĉiu ludanto krom la kameleono scias la "
        "sekretan vorton. Ĉiu, inkluzive de la kameleono, laŭvice "
        "donos vorton kiel indikon al la sekreta vorto. Oni volas "
        "fari indikon kiu estas sufiĉe klara por pruvi ke oni ne "
        "estas la kameleono, sed ne tiel specifa ke la kameleono "
        "povus uzi ĝin por diveni la sekretan vorton. Kiam ĉiu donis "
        "indikon, estas tempo por debato por provi trovi la "
        "kameleonon.\n"
        "\n"
        "Kiam ĉiu pretas, oni povas voĉdoni. Se oni elektas la "
        "malĝustan ludanton, la kameleono gajnas 2 poentojn. Tamen se "
        "oni ja trovas la kameleonon, ri ankoraŭ havas ŝancon por "
        "gajni se ri povas diveni la sekretan vorton sciante la "
        "indikojn. Se ri sukcesas, ri gajnas unu poenton. Aliokaze "
        "ĉiu krom ri gajnas 2 poentojn.\n"
        "\n"
        "La ludo finiĝas kiam iu havas 5 poentojn.",
};
