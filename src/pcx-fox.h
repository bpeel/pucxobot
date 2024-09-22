/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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

#ifndef PCX_FOX_H
#define PCX_FOX_H

#include "pcx-game.h"

extern const struct pcx_game pcx_fox_game;

struct pcx_fox;

struct pcx_fox_debug_overrides {
        int (*rand_func)(void);
};

struct pcx_fox *
pcx_fox_new(const struct pcx_game_callbacks *callbacks,
            void *user_data,
            enum pcx_text_language language,
            int n_players,
            const char *const *names,
            const struct pcx_fox_debug_overrides *overrides);

#endif /* PCX_FOX_H */
