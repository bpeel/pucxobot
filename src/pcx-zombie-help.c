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

#include "pcx-zombie-help.h"

#include "pcx-text.h"

const char *
pcx_zombie_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Vi estas zombio kaj vi volas manĝi la maksimumon da cerboj. Dum via "
        "vico vi ĵetos 3 ĵetkubojn. Ĉiu ĵetkubo reprezentas homon. La ĵeto "
        "povas havi unu el la sekvaj tri rezultoj:\n"
        "\n"
        "🧠: Vi manĝas la cerbon de la homo\n"
        "💥: La homo pafas al vi\n"
        "🐾: La homo eskapis\n"
        "\n"
        "Ĵetinte la kubojn, vi flanken metas ĉiujn cerbojn🧠 kaj pafojn💥. Se "
        "vi fine ricevas 3 pafojn, vi mortas kaj perdas ĉiujn cerbojn kiujn vi "
        "gajnis dum ĉi tiu vico. Aliokaze vi rajtas elekti ĉu daŭrigi la "
        "ĵetadon. Se vi ĉesas, vi aldonas ĉiujn cerbojn kiujn vi ĵetis al via "
        "poentaro.\n"
        "\n"
        "Se vi ĵetas denove, vi prenas ĉiujn kubojn kun piedoj🐾, aldonas "
        "pliajn kubojn el la skatolo por rehavi tri kaj sekve ĵetas ilin same "
        "kiel antaŭe.\n"
        "\n"
        "Estas tri koloroj de ĵetkubo: verda🍏, flava💛 kaj ruĝa🧨. La verdaj "
        "ĵetkuboj pli ofte rezultas en cerbo kaj la ruĝaj pli ofte en pafo. La "
        "flavaj estas ekvilibraj.\n"
        "\n"
        "Post kiam unu ludantoj gajnas 13 poentojn, la aliaj ludantoj rajtas "
        "fini unu lastan vicon. Post tio, la homo kun la plej multe da poentoj "
        "gajnas la partion.",
        [PCX_TEXT_LANGUAGE_FRENCH] =
        "stub",
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>SUMMARY OF THE RULES:</b>\n"
        "\n"
        "You are a zombie and you want to eat as many brains as possible. "
        "During your turn you will throw 3 dice with each die representing a "
        "person. A dice can throw can result in one of these three things "
        "happening:\n"
        "\n"
        "🧠: You ate the person’s brain\n"
        "💥: The person shot at you\n"
        "🐾: The person escaped\n"
        "\n"
        "After throwing the dice you put aside all the brains🧠 and shotguns💥. "
        "If you end up with 3 shotguns you die and lose all the brains that "
        "you gained in this turn. Otherwise you can choose whether to continue "
        "rolling. If you stop, you add all of the brains that you threw to "
        "your total score.\n"
        "\n"
        "If you roll again, you take all of the feet🐾 dice, add more dice "
        "from the box to get back up to three and then roll them again as "
        "before.\n"
        "\n"
        "There are three colors of die: green🍏, yellow💛 and red🧨. The green "
        "dice are more likely to throw a brain and the red dice are more "
        "likely to throw a shotgun. The yellow dice are balanced.\n"
        "\n"
        "After one player scores 13 points the other players are allowed to "
        "finish one last turn. After that the person with the highest score "
        "wins.\n",
        [PCX_TEXT_LANGUAGE_PT_BR] =
        "stub",
};
