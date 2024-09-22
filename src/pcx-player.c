/*
 * Pucxobot - A bot and website to play some card games
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

#include "config.h"

#include "pcx-player.h"
#include "pcx-main-context.h"

struct pcx_player *
pcx_player_new(uint64_t id,
               struct pcx_conversation *conversation,
               const char *name)
{
        struct pcx_player *player = pcx_alloc(sizeof *player);

        player->id = id;
        player->ref_count = 0;
        player->last_update_time = pcx_main_context_get_monotonic_clock(NULL);
        player->has_left = false;

        pcx_conversation_ref(conversation);
        player->conversation = conversation;

        player->player_num = pcx_conversation_add_player(conversation, name);

        return player;
}

void
pcx_player_free(struct pcx_player *player)
{
        pcx_conversation_unref(player->conversation);
        pcx_free(player);
}
