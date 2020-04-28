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

#ifndef PCX_PLAYER_H
#define PCX_PLAYER_H

#include <stdint.h>

#include "pcx-list.h"
#include "pcx-conversation.h"

struct pcx_player {
        /* This is the randomly generated globally unique ID for the
         * player that is used like a password for the clients.
         */
        uint64_t id;

        /* Player index within the conversation */
        int player_num;

        struct pcx_conversation *conversation;

        /* The number of connections listening to this player. The
         * player is a candidate for garbage collection if this
         * reaches zero.
         */
        int ref_count;

        /* The last time a connection that is using this player sent
         * some data. If this gets too old it will be a candidate for
         * garbage collection.
         */
        uint64_t last_update_time;

        /* Position in the global list of players */
        struct pcx_list link;

        /* Used to implement the hash table */
        struct pcx_player *hash_next;
};

struct pcx_player *
pcx_player_new(uint64_t id,
               struct pcx_conversation *conversation);

void
pcx_player_free(struct pcx_player *player);

#endif /* PCX_PLAYER_H */
