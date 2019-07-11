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

#include "pcx-coup-help.h"

#include "pcx-text.h"

const char *
pcx_coup_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Puĉo estas kartludo de trompado kaj blufado.\n"
        "\n"
        "Ĉiu komence havas 2 kartojn (fackaŝitajn) kaj 2 monerojn. "
        "Se oni perdas vivon oni devas malkaŝi karton kaj oni ne plu povas uzi "
        "ĝin.\n"
        "\n"
        "Dum sia vico oni povas fari unu el la sekvaj agoj:\n"
        "\n"
        "<b>Enspezi</b>: gajni unu moneron kaj neniu povas malhelpi ĝin.\n"
        "\n"
        "<b>Eksterlanda helpo</b>: gajni du monerojn, sed se iu pretendas havi "
        "la dukon ri povas bloki ĝin.\n"
        "\n"
        "<b>Puĉo</b>: pagi 7 monerojn por mortigi iun. Neniu povas malhelpi "
        "tion. Se iu havas 10 monerojn ri nepre devas fari puĉon.\n"
        "\n"
        "Se oni havas unu el la sekvaj kartoj, aŭ pretendas havi ĝin, "
        "oni povas:\n"
        "\n"
        "<b>Imposto (Duko)</b>: Preni 3 monerojn.\n"
        "\n"
        "<b>Murdi (Murdisto)</b>: Pagi 3 monerojn kaj murdi iun. Se la viktimo "
        "pretendas havi la grafinon ri povas bloki ĝin.\n"
        "\n"
        "<b>Interŝanĝi (Ambasadoro)</b>: Interŝanĝi siajn kartojn por du novaj "
        "kartoj.\n"
        "\n"
        "<b>Ŝteli (Kapitano)</b>: Ŝteli 2 monerojn de alia ludanto. Se la "
        "viktimo pretendas havi la ambasadoron aŭ la kapitanon ri povas bloki "
        "ĝin.\n"
        "\n"
        "Ekzistas nur po 3 kartoj de ĉiu rolulo.\n"
        "\n"
        "Ĉiu pretendo de karto povas esti defiita de iu ajn alia ludanto. Se "
        "la defio estis prava, la defiito perdas karton, alikaze la defianto "
        "perdas karton.\n"
        "\n"
        "Tiu kiu restas vivanta venkas.\n",

        [PCX_TEXT_LANGUAGE_FRENCH] =
        "<b>RESUMÉ DES RÈGLES :</b>\n"
        "\n"
        "Complot est un jeu de cartes au tour du bluff.\n"
        "\n"
        "Tout le monde commence avec 2 cartes (face visible) et 2 or. "
        "Si quelqu’un perd une vie, il doit dévoiler une carte et il ne peut "
        "plus l’utiliser.\n"
        "\n"
        "Pendant votre tour, vous pouvez faire une des actions suivantes :\n"
        "\n"
        "<b>Revenu</b> : prendre 1 or et personne ne peut l’empecher.\n"
        "\n"
        "<b>Aide étrangère</b> : prendre 2 or, mais si quelqu’un prétend avoir "
        "la duchesse il peut le bloquer.\n"
        "\n"
        "<b>Assassinat</b> : payer 7 or pour assassiner quelqu’un. Personne "
        "ne peut l’empecher. Si quelqu’un a 10 or il doit forcement faire un "
        "assassinat.\n"
        "\n"
        "Si vous avez une des cartes suivantes, ou vouz prétendez l’avoir, "
        "vous pouvez :\n"
        "\n"
        "<b>Taxer (Duchesse)</b> : Prendre 3 or.\n"
        "\n"
        "<b>Assassiner (Assassin)</b> : Payer 3 or et assassiner quelqu’un. "
        "Si la victime prétend avoir la comtesse elle peut le bloquer.\n"
        "\n"
        "<b>Échanger (Ambassadeur)</b> : Échanger vos cartes pour de nouvelles "
        "cartes.\n"
        "\n"
        "<b>Voler (Capitaine)</b> : Voler 2 or à un autre joueur. "
        "Si la victime prétend avoir l’ambassadeur ou le capitaine elle peut "
        "le bloquer.\n"
        "\n"
        "Il n’y a que trois exemplaires de chaque personnage.\n"
        "\n"
        "Chaque déclaration de carte peut être mise en doute par n’importe "
        "quel autre joueur. Si la mise en doute a raison, celui qui a été "
        "mis en doute perd une carte, sinon le metteur en doute en perd.\n"
        "\n"
        "Celui qui reste en vie remporte la partie.\n",
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>SUMMARY OF THE RULES:</b>\n"
        "\n"
        "Coup is a bluffing card game.\n"
        "\n"
        "Everybody starts with two cards (face down) and 2 coins. If someone "
        "loses a life, they have to reveal the card and they can no longer "
        "use it.\n"
        "\n"
        "On your turn, you can do one of the following actions:\n"
        "\n"
        "<b>Income</b>: Take one coin from the bank and nobody can stop you.\n"
        "\n"
        "<b>Foreign aid</b>: Take two coins, but if someone claims to have "
        "the duke they can stop you.\n"
        "\n"
        "<b>Coup</b>: Pay 7 coins to perform a coup and kill a player’s card. "
        "Nobody can prevent it. If you have 10 coins you have to choose this "
        "action.\n"
        "\n"
        "If you have one of the following cards, or just claim to have it, "
        "you can do one of these actions:\n"
        "\n"
        "<b>Tax (Duke)</b>: Take 3 coins.\n"
        "\n"
        "<b>Assassinate (Assassin)</b>: Pay 3 coins and kill someone’s card. "
        "If the target claims to have the contessa they can block it.\n"
        "\n"
        "<b>Exchange (Ambassador)</b>: Exchange your cards for new ones.\n"
        "\n"
        "<b>Steal (Captain)</b>: Take 2 coins from another player. If they "
        "claim to have the captain or the ambassador they can block it.\n"
        "\n"
        "There are only three copies of each card.\n"
        "\n"
        "Every claim of a card can be challenged by any other player. If the "
        "challenge is correct, the person who was challenged loses a card. "
        "Otherwise the challenger loses one.\n"
        "\n"
        "The winner is the last player left alive. ",
};
