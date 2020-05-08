/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
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

#include "pcx-six-help.h"

#include "pcx-text.h"

const char *
pcx_six_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "En 6 Prenas estas 104 kartoj, ĉiuj kun numero inter 1 kaj 104. Ĉiu "
        "karto ankaŭ havas kvanton de bovkapoj 🐮 kiu dependas de la karto. "
        "La celo de la ludo estas fini kun malpli da 🐮 ol ĉiu alia ludanto.\n"
        "\n"
        "Ĉiu ricevas 10 kartojn en sia mano. Sekve oni kreas 4 liniojn kiuj "
        "komence havas po unu karto. Por fari raŭndon, ĉiu elektas karton el "
        "sia mano samtempe. Kiam ĉiu estas preta oni montras la kartojn. "
        "Komence per la plej malalta karto, oni metas la kartojn en la "
        "liniojn. La linioj ĉiam faras serion de kreskantoj valoroj, do oni "
        "ĉiam metas sian karton al linio kies plej alta karto estas malpli ol "
        "onia karto. Se estas pluraj eblecoj, la karto devas iri al tiu el "
        "ili kiu havas la plej altan karton.\n"
        "\n"
        "Ĉiu linio havas maksimume 5 kartojn. Se ludanto devas meti la 6an "
        "karton en linion, ri unue devas preni la tutan linion kaj meti ĝin "
        "flanken. Sekve ri metas sian karton kiel la unuan karton de la "
        "serio. La 🐮oj sur la flankenmetitaj kartoj aldoniĝas al la poentoj "
        "de la ludantoj.\n"
        "\n"
        "Se la karto de ludanto estas malpli alta ol ĉiu ajn linio, la "
        "ludanto devas preni iun ajn linio laŭ sia elekto kaj anstataŭigi ĝin "
        "per sia karto.\n"
        "\n"
        "Kiam ĉiu ludis siajn 10 kartojn oni aldonas la 🐮ojn al la poentaro. "
        "Se iu ludanto havas almenaŭ 66 poentojn, la ludo finiĝas kaj tiu kiu "
        "havas la malplej da poentoj gajnas la partion. Alikaze oni denove "
        "disdonas la kartojn kiel en la komenco kaj komencas novan raŭndon.",
        [PCX_TEXT_LANGUAGE_FRENCH] =
        "stub",
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>SUMMARY OF THE RULES:</b>\n"
        "\n"
        "In 6 Takes there are 104 cards, each with a number between 1 and "
        "104. Every card also has some number of bullheads 🐮 depending on "
        "the card. The aim of the game is to end up with fewer 🐮s than every "
        "other player.\n"
        "\n"
        "Everybody receives 10 cards in their hand. Next, 4 rows are created "
        "which start with one card each. During a round, each player chooses "
        "a card from their hand at the same time. When everyone is ready, the "
        "cards are all shown. Starting with the lowest card, the players add "
        "their cards to one of the rows. The rows always make a series of "
        "increasing values, so you always have to put your card in a row "
        "whose highest card is lower than yours. If there are several such "
        "rows, the card goes to the one with the highest card.\n"
        "\n"
        "Every row has at most 5 cards. If a player has to put the 6th card "
        "in a row, they first have to take all the cards in the row and put "
        "them aside. Then they place their own card as the only card in the "
        "row. The 🐮s on the cards that were just taken are added to the "
        "player’s score.\n"
        "\n"
        "If the chosen card is lower than the card in every row, the player "
        "has to choose any of the rows, take it and replace it with their "
        "card.\n"
        "\n"
        "When everyone has played all 10 of their cards, the 🐮s that were "
        "put aside are counted up and added to the score. If a player has at "
        "least 66 points the game is over and the player with the fewest "
        "points wins. Otherwise the cards are re-dealt as in the beginning "
        "and a new round is started.",
        [PCX_TEXT_LANGUAGE_PT_BR] =
        "stub",
};
