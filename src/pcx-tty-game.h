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

#ifndef PCX_TTY_GAME_H
#define PCX_TTY_GAME_H

#include "pcx-error.h"
#include "pcx-config.h"

extern struct pcx_error_domain
pcx_tty_game_error;

enum pcx_tty_game_error {
        PCX_TTY_GAME_ERROR_IO
};

struct pcx_tty_game;

struct pcx_tty_game *
pcx_tty_game_new(const struct pcx_config *config,
                 int n_players,
                 const char * const *files,
                 struct pcx_error **error);

void
pcx_tty_game_free(struct pcx_tty_game *game);

#endif /* PCX_TTY_GAME_H */
