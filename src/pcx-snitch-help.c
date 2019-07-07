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
        "partion.\n",
        [PCX_TEXT_LANGUAGE_FRENCH] =
        "Balance est un jeu de cartes de mi-collaboration et de bluff.\n"
        "\n"
        "Vous êtes tous des cambrioleurs qui travaillent ensembles pour "
        "organiser un braquage. Il y a 8 tours et pour chaque tour il y a "
        "un chef de braquage.\n"
        "\n"
        "<b>Le contrat</b>\n"
        "\n"
        "Le chef de braquage choisit combien de personnages il faudra pour "
        "le braquage, et ainsi il choisit la difficulté. Puis il pioche "
        "des cartes qui indiquent lesquels sont ces personnages. Ces "
        "cartes-là forment le contrat et pour réussir le braquage il faut "
        "embaucher tous les personnages indiqués.\n"
        "\n"
        "<b>La discussion et le braquage</b>\n"
        "\n"
        "Maintenant c’est la phase de discussion. Tout le monde a dans sa "
        "main 10 cartes. 7 d’entre eux représentent des personnages. Il "
        "faut collaborer pour essayer d’avoir les cartes nécessaires. "
        "Finalement, chacun choisit sa carte en secret et quand tout le "
        "monde sera prêt les cartes seront révélées au même temps. Si vous "
        "réussissez à montrer toutes les cartes du contrat, tout le monde "
        "gagne la même quantité de pièces d’or que la taille du contrat.\n"
        "\n"
        "<b>Les balances</b>\n"
        "\n"
        "Pourtant, dans la main de chacun, il y a aussi 3 balances. Si le "
        "braquage est un échec et au moins une personne choisit la "
        "balance, à la place de voler de l’argent de la banque les "
        "balances prennent de l’argent des autres joueurs ! Tous ce qui "
        "n’ont pas choisi la balance perdent une pièce d’or et la "
        "collection de pièces est partagée entre les balances. Si la somme "
        "d’or n’est pas divisible également, des pièces de la banque sont "
        "ajoutées pour le faire égale.\n"
        "\n"
        "Après les cinq tours, ce qui a le plus d’or remporte la partie.",
};
