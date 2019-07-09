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

#include "pcx-love-help.h"

#include "pcx-text.h"

const char *
pcx_love_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Vi ĉiuj estas amindumantoj de la princino kaj volas sendi al ŝi "
        "amleteron. En via mano vi havas unu karton kiu reprezentas la homon "
        "kiu aktuale portas vian leteron. Ju pli alta ĝia numero, des pli "
        "proksima la homo estas al la princino. Je la fino de la raŭndo tiu "
        "kiu havas la plej valoran karton sendas sian leteron al la princino "
        "kaj gajnas unu poenton de korinklino.\n"
        "\n"
        "Je ĉiu vico, ludanto prenas karton de la kartaro kaj devas elekti unu "
        "el la du kartoj en sia mano por forĵeti. Ĉiu karto havas efikon kiam "
        "ĝi estas forĵetita:\n"
        "\n"
        "@CARDS@\n"
        "\n"
        "Kiam iu gajnas sufiĉe da korinklino de la princino ri gajnas la "
        "partion.\n",
        [PCX_TEXT_LANGUAGE_FRENCH] =
        "<b>RÉSUMÉ DES RÈGLES :</b>\n"
        "\n"
        "Vous êtes tous des prétendants de l’amour de la princesse et vous "
        "voulez lui envoyer une lettre d’amour. Dans votre main vous avez une "
        "carte qui représente la personne qui porte votre lettre en ce moment. "
        "Plus ce numéro est élevé, plus la personne est proche de la "
        "princesse. À la fin de la manche, ce qui a la carte avec le plus de "
        "valeur envoie sa lettre à la princesse et gagne un point "
        "d’affection.\n"
        "\n"
        "À chaque tour, le joueur actif pioche une carte et doit choisir entre "
        "les deux cartes qu’il aura dans sa main pour la défausser. Chaque "
        "carte a un effet différent lorsque on la défausse :\n"
        "\n"
        "@CARDS@\n"
        "\n"
        "Quand quelqu’un gagne assez d’affection de la princesse, il remporte "
        "la partie.",
};
