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
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>Chameleon</b>\n"
        "\n"
        "In every round a player is randomly chosen to be the chameleon. "
        "Nobody knows who that is, except for the the chameleon themself. "
        "Next, there’s a list of words with a topic. One of the words is "
        "the secret word. All of the players apart from the chameleon know "
        "which word it is. Everybody, including the chameleon, takes turns "
        "giving a clue for the secret word. You want to give a clue that "
        "is clear enough to prove that you aren’t the chameleon "
        "but not so obvious that the chameleon could use it to guess the "
        "secret word. When everybody has given their clue, it’s time for "
        "everyone to discuss who they think the chameleon is.\n"
        "\n"
        "When everybody is ready, you can vote. If the person with the most "
        "votes really is the chameleon, they still have a chance to win "
        "if they can guess the secret word using the clues. If they manage it "
        "they get one point. Otherwise everyone apart from the chameleon "
        "gets two points.\n"
        "\n"
        "The first person to get 5 points wins the game.\n",
};
