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

#include "pcx-http-game.h"

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"

struct pcx_http_game {
        struct pcx_game *game;
};

struct pcx_http_game *
pcx_http_game_new(void)
{
        struct pcx_http_game *game = pcx_calloc(sizeof *game);

        return game;
}

void
pcx_http_game_free(struct pcx_http_game *game)
{
        if (game->game)
                pcx_game_free(game->game);

        pcx_free(game);
}
