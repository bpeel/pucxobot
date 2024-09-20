/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2024  Neil Roberts
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

#include "pcx-werewolf-help.h"

#include "pcx-text.h"

const char *
pcx_werewolf_help[] = {
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>SUMMARY OF THE RULES:</b>\n"
        "\n"
        "All of the players take on the role of an inhabitant of a village "
        "that has been attacked by werewolves. Each player is either on "
        "the werewolf team or the villager team. The game takes place "
        "over a single night where strange things happen. In the morning "
        "the village must collectively vote to kill a person. If they pick "
        "a werewolf the villagers win, otherwise the werewolf team wins.\n"
        "\n"
        "Each player is assigned a role by being given a card. Three extra "
        "cards are left on the table face down in front of the players. "
        "Each role will be woken up in turn during the night to do the "
        "following actions:\n"
        "\n"
        "🐺 <b>Werewolf</b> The werewolves will wake up during the night to "
        "find out who else is on their team. If there is only one werewolf, "
        "they can also look at one of the cards in the middle of the "
        "table.\n"
        "\n"
        "🦺 <b>Minion</b> The minion is on the werewolf team. He will be "
        "woken up to find out who the werewolves are. If he dies at the "
        "end the werewolves can still win.\n"
        "\n"
        "⚒️ <b>Mason</b> The masons are on the villager team. They will be "
        "woken up to find each other.\n"
        "\n"
        "🔮 <b>Seer</b> The seer can choose to see another player’s card, "
        "or see two cards from the center of the table.\n"
        "\n"
        "🤏 <b>Robber</b> The robber can swap his card with another "
        "player’s card and then look at the new card. He doesn’t wake up "
        "again for the new role however.\n"
        "\n"
        "🐈 <b>Troublemaker</b> The troublemaker can swap around the cards "
        "of two other players without looking at them.\n"
        "\n"
        "🍺 <b>Drunk</b> The drunk swaps his card with one from the center "
        "without looking at it. He no longer knows what card he has.\n"
        "\n"
        "🥱 <b>Insomniac</b> The insomniac wakes up last to look at her "
        "card and see if she’s still the insomniac.\n"
        "\n"
        "The remaining roles don’t wake up during the night:\n"
        "\n"
        "🧑‍🌾 <b>Villager</b> The villager has no special powers.\n"
        "\n"
        "🙍‍♂️ <b>Tanner</b> The tanner is on his own team and wants to be "
        "killed. If that happens he alone wins.\n"
        "\n"
        "🔫 <b>Hunter</b> The hunter is on the villager team. If he is "
        "killed then he takes whoever he voted for with him.",

        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Ĉiu ludanto estas rolulo en vilaĝo kiun atakis homlupoj. "
        "Ĉiuj estas aŭ en la teamo de la homlupoj aŭ en tiu de la "
        "vilaĝanoj. La ludo daŭras nur unu nokton dum kiu strangaj "
        "aferoj okazas. En la mateno la vilaĝo devas kune decidi "
        "mortigi homon. Se ĝi elektas homlupon la vilaĝanoj venkas, "
        "aliokaze la teamo de la homlupoj venkas.\n"
        "\n"
        "Ĉiu ludanto ricevas karton kun rolo. Tri pliaj kartoj restas "
        "sur la tablo tiel ke oni ne povas vidi la rolon. Ĉiu rolo "
        "vekiĝos dum la nokto por fari la jenajn agojn:\n"
        "\n"
        "🐺 <b>Homlupo</b> La homlupoj vekiĝas dum la nokto por vidi "
        "unu la alian. Se estas nur unu homlupo, ri rajtas rigardi "
        "unu el la kartoj sur la tablo.\n"
        "\n"
        "🦺 <b>Sbiro</b> La sbiro estas en la homlupa teamo. Li "
        "vekiĝos por vidi kiuj estas la homlupoj. Se li mortas je la "
        "fino la homlupoj tamen povas venki.\n"
        "\n"
        "⚒️ <b>Masonisto</b> La masonistoj estas en la teamo de la "
        "vilaĝanoj. Ili vekiĝas por vidi unu la alian.\n"
        "\n"
        "🔮 <b>Klarvidulino</b> La klarvidulino rajtas vidi karton de "
        "alia ludanto, aŭ du kartojn de la mezo de la tablo.\n"
        "\n"
        "🤏 <b>Ŝtelisto</b> La ŝtelisto povas interŝanĝi sian karton "
        "kun karto de alia ludanto kaj rigardi sian novan karton. Li "
        "tamen ne vekiĝas por la agoj de la nova rolo.\n"
        "\n"
        "🐈 <b>Petolulo</b> La petolulo rajtas interŝanĝi kartojn de "
        "du aliaj ludantoj sen rigardi ilin.\n"
        "\n"
        "🍺 <b>Ebriulo</b> La ebriulo interŝanĝas sian karton kun unu "
        "el la mezo de la tablo sen rigardi ĝin. Li ne plu scias kiun "
        "rolon li havas.\n"
        "\n"
        "🥱 <b>Sendormulo</b> La sendormulo vekiĝas laste por rigardi "
        "sian karton kaj kontroli ĉu ŝi ankoraŭ estas la sendormulo.\n"
        "\n"
        "La ceteraj roluloj ne vekiĝas dum la nokto:\n"
        "\n"
        "🧑‍🌾 <b>Vilaĝanoj</b> La vilaĝanoj ne havas specialan povon.\n"
        "\n"
        "🙍‍♂️ <b>Tanisto</b> La tanisto estas sia propra teamo kaj li "
        "volas morti. Se oni mortigas lin li sola venkas.\n"
        "\n"
        "🔫 <b>Ĉasisto</b> La ĉasisto estas en la teamo de la "
        "vilaĝanoj. Se oni mortigas lin li lastmomente pafas kaj "
        "mortigas ankaŭ la homon por kiu li voĉdonis.",
};
