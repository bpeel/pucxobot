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

#include "config.h"

#include "pcx-game.h"

#include "pcx-coup.h"
#include "pcx-snitch.h"
#include "pcx-love.h"
#include "pcx-zombie.h"
#include "pcx-superfight.h"

const struct pcx_game * const
pcx_game_list[] = {
        &pcx_coup_game,
        &pcx_snitch_game,
        &pcx_love_game,
        &pcx_zombie_game,
        &pcx_superfight_game,
        NULL
};
