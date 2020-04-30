/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2015, 2020  Neil Roberts
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

#ifndef PCX_PLAYERBASE_H
#define PCX_PLAYERBASE_H

#include <stdint.h>

#include "pcx-player.h"
#include "pcx-conversation.h"

struct pcx_playerbase *
pcx_playerbase_new(void);

struct pcx_player *
pcx_playerbase_get_player_by_id(struct pcx_playerbase *playerbase,
                                uint64_t id);

struct pcx_player *
pcx_playerbase_add_player(struct pcx_playerbase *playerbase,
                          struct pcx_conversation *conversation,
                          const char *name,
                          uint64_t id);

int
pcx_playerbase_get_n_players(struct pcx_playerbase *playerbase);

void
pcx_playerbase_free(struct pcx_playerbase *playerbase);

#endif /* PCX_PLAYERBASE_H */
