/*
 * Pucxobot - A bot and website to play some card games
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

#ifndef PCX_WEREWOLF_H
#define PCX_WEREWOLF_H

#include "pcx-game.h"

extern const struct pcx_game pcx_werewolf_game;

struct pcx_werewolf;

enum pcx_werewolf_role {
        PCX_WEREWOLF_ROLE_VILLAGER,
        PCX_WEREWOLF_ROLE_WEREWOLF,
        PCX_WEREWOLF_ROLE_MINION,
        PCX_WEREWOLF_ROLE_MASON,
        PCX_WEREWOLF_ROLE_SEER,
        PCX_WEREWOLF_ROLE_ROBBER,
        PCX_WEREWOLF_ROLE_TROUBLEMAKER,
        PCX_WEREWOLF_ROLE_DRUNK,
        PCX_WEREWOLF_ROLE_INSOMNIAC,
        PCX_WEREWOLF_ROLE_TANNER,
        PCX_WEREWOLF_ROLE_HUNTER,
};

struct pcx_werewolf_debug_overrides {
        const enum pcx_werewolf_role *cards;
};

struct pcx_werewolf *
pcx_werewolf_new(const struct pcx_game_callbacks *callbacks,
                 void *user_data,
                 enum pcx_text_language language,
                 int n_players,
                 const char *const *names,
                 const struct pcx_werewolf_debug_overrides *overrides);

#endif /* PCX_WEREWOLF_H */
