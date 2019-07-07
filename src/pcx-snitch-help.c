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

#include "pcx-snitch-help.h"

#include "pcx-text.h"

const char *
pcx_snitch_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "Perfidulo estas kartludo de duonkunlaboro kaj blufado.\n"
        "\n"
        "Vi ĉiuj estas rabistoj kiuj kunlaboras por organizi rabadojn de "
        "bankoj. Estos 8 raŭndoj kaj en ĉiu raŭndo estos unu rabestro.\n"
        "\n"
        "<b>La kontrakto</b>\n"
        "\n"
        "La rabestro elektas kiom da roluloj oni bezonos por la rabado, kaj "
        "tiel elektas la mafacilecon. Tiam oni prenas kartojn el la kartaro "
        "kiuj indikas kiu estas ĉiu el tiuj roluloj. Tiuj kartoj formas la "
        "kontrakton kaj por sukcesi la rabadon oni devas dungi ĉiujn tiujn "
        "rolulojn.\n"
        "\n"
        "<b>La diskuto kaj rabado</b>\n"
        "\n"
        "Nun estas la diskuta fazo. Ĉiu havas en sia mano 10 kartojn. 7 el ili "
        "reprezentas rolulojn. Vi devas kunlabori por provi havi la bezonatajn "
        "rolulojn. Fine ĉiu sekrete elektas sian karton kaj kiam ĉiu estos "
        "preta la kartoj estos malkaŝitaj samtempe. Se vi sukcesas montri la "
        "bezonatajn rolulojn ĉiu gajnas la saman kvanton da moneroj kiel la "
        "grandeco de la kontrakto.\n"
        "\n"
        "<b>La perfiduloj</b>\n"
        "\n"
        "Tamen en ĉies mano estas ankaŭ 3 perfiduloj. Se la rabado malsukcesas "
        "kaj iuj elektis la perfidulon, anstataŭ ŝteli monon de la banko la "
        "perfiduloj prenas monon de la aliaj ludantoj! Ĉiu kiu ne elektis "
        "perfidulon perdas 1 moneron kaj la kolekto de mono estas dividita "
        "inter la perfiduloj. Se la sumo ne estas egale dividebla, oni prenas "
        "pliajn monerojn el la banko por egaligi ĝin.\n"
        "\n"
        "Post 8 raŭndoj, tiu kiu havas la plej multon da mono gajnas la "
        "partion.\n"
};
