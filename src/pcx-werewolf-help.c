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
        "killed then he takes whoever he voted for with him."
};
