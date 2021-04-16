/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2021  Neil Roberts
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

#include "pcx-fox-help.h"

#include "pcx-text.h"

const char *
pcx_fox_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Vulpo en la Arbaro estas kartludo pri prenvicoj por 2 ludantoj.\n"
        "\n"
        "La kartaro havas la kartojn 1-11 kun la tri emblemoj 🔔, 🗝 kaj 🌜. "
        "Dum prenvico unu ludanto ludas la unuan karton kaj la alia ludanto "
        "devas sekvi. Se la sekvanto havas almenaŭ unu karton kun la sama "
        "emblemo ri devas ludi tiun emblemon. Aliokaze ri rajtas ludi iun ajn "
        "karton. La ludanto kiu ludas la plej bonan karton gajnas la "
        "prenvicon kaj komencas la sekvan.\n"
        "\n"
        "Estas unu aparta karto kiu nomiĝas la dekreta karto. Por decidi kiu "
        "karto plej bonas, la ordo de la emblemoj estas: la emblemo de la "
        "dekreta karto, la emblemo de la komenca karto kaj la alia emblemo. Se "
        "ambaŭ kartoj havas la saman emblemon la plej alta numero venkas.\n"
        "\n"
        "Kiam oni ludas malparan karton estas speciala efiko:\n"
        "\n"
        "1🔼: Se la malvenkinto ludis ĉi tion, ri tamen komencas la sekvan "
        "prenvicon.\n"
        "3🔄: La ludanto rajtas interŝanĝi karton kun la dekreta karto.\n"
        "5📤: La ludanto prenas novan karton kaj forĵetas unu.\n"
        "7💎: La venkinto de la prenvico tuj gajnas poenton.\n"
        "9🎩: Se oni ludis nur unu 9-karton, ĝia emblemo iĝas la dekreta "
        "emblemo.\n"
        "11↕️: Se ĉi tiu estas la unua karto kaj la sekvanto havas ĝian "
        "emblemon, ri devas ludi aŭ sian plej altan karton, aŭ la karton 1.\n"
        "\n"
        "Je la fino de la raŭndo, la ludanto kiu gajnis pli da prenoj gajnas "
        "6 poentojn kaj la alia gajnas malpli. Tamen se la venkinto estis tro "
        "avida kaj gajnis almenaŭ 10 prenojn ri ricevas neniun poenton kaj la "
        "alia gajnas la 6. La ludo finiĝas kiam iu havas almenaŭ 21 poentojn.",
};
