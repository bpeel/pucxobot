/*
 * Pucxobot - A bot and website to play some card games
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

#include "config.h"

#include "pcx-playerbase.h"

#include <assert.h>

#include "pcx-util.h"
#include "pcx-main-context.h"

/* Number of microseconds of inactivity before a player will be
 * considered for garbage collection.
 */
#define PCX_PLAYERBASE_MAX_PLAYER_AGE ((uint64_t) 2 * 60 * 1000000)

struct pcx_playerbase {
        struct pcx_list players;

        int n_players;
        int hash_size;
        struct pcx_player **hash_table;

        struct pcx_main_context_source *gc_source;
};

static void
queue_gc_source(struct pcx_playerbase *playerbase);

static int
get_hash_pos(struct pcx_playerbase *playerbase,
             uint64_t id)
{
        return id % playerbase->hash_size;
}

static void
remove_player(struct pcx_playerbase *playerbase,
              struct pcx_player *player)
{
        pcx_list_remove(&player->link);

        int pos = get_hash_pos(playerbase, player->id);
        struct pcx_player **prev = playerbase->hash_table + pos;
        while (true) {
                assert(*prev);

                if (*prev == player)
                        break;

                prev = &(*prev)->hash_next;
        }

        *prev = player->hash_next;

        if (!player->has_left) {
                pcx_conversation_remove_player(player->conversation,
                                               player->player_num);
        }

        pcx_player_free(player);

        playerbase->n_players--;
}

static void
gc_cb(struct pcx_main_context_source *source,
      void *user_data)
{
        struct pcx_playerbase *playerbase = user_data;
        uint64_t now = pcx_main_context_get_monotonic_clock(NULL);
        struct pcx_player *player, *tmp;

        playerbase->gc_source = NULL;

        pcx_list_for_each_safe(player, tmp, &playerbase->players, link) {
                if (player->ref_count == 0 &&
                    now - player->last_update_time >=
                    PCX_PLAYERBASE_MAX_PLAYER_AGE) {
                        remove_player(playerbase, player);
                }
        }

        if (playerbase->n_players > 0)
                queue_gc_source(playerbase);
}

static void
queue_gc_source(struct pcx_playerbase *playerbase)
{
        if (playerbase->gc_source)
                return;

        playerbase->gc_source =
                pcx_main_context_add_timeout(NULL,
                                             PCX_PLAYERBASE_MAX_PLAYER_AGE /
                                             1000 +
                                             1,
                                             gc_cb,
                                             playerbase);
}

struct pcx_playerbase *
pcx_playerbase_new(void)
{
        struct pcx_playerbase *playerbase = pcx_calloc(sizeof *playerbase);

        pcx_list_init(&playerbase->players);

        playerbase->hash_size = 8;
        playerbase->hash_table = pcx_calloc(playerbase->hash_size *
                                            sizeof *playerbase->hash_table);

        return playerbase;
}

struct pcx_player *
pcx_playerbase_get_player_by_id(struct pcx_playerbase *playerbase,
                                uint64_t id)
{
        int pos = get_hash_pos(playerbase, id);

        for (struct pcx_player *player = playerbase->hash_table[pos];
             player;
             player = player->hash_next) {
                if (player->id == id)
                        return player;
        }

        return NULL;
}

static void
add_player_to_hash(struct pcx_playerbase *playerbase,
                   struct pcx_player *player)
{
        int pos = get_hash_pos(playerbase, player->id);

        player->hash_next = playerbase->hash_table[pos];
        playerbase->hash_table[pos] = player;
}

static void
grow_hash_table(struct pcx_playerbase *playerbase)
{
        pcx_free(playerbase->hash_table);

        playerbase->hash_size *= 2;

        playerbase->hash_table = pcx_calloc(playerbase->hash_size *
                                            sizeof *playerbase->hash_table);

        struct pcx_player *player;

        pcx_list_for_each(player, &playerbase->players, link) {
                add_player_to_hash(playerbase, player);
        }
}

struct pcx_player *
pcx_playerbase_add_player(struct pcx_playerbase *playerbase,
                          struct pcx_conversation *conversation,
                          const char *name,
                          uint64_t id)
{
        if ((playerbase->n_players + 1) > playerbase->hash_size * 3 / 4)
                grow_hash_table(playerbase);

        struct pcx_player *player = pcx_player_new(id, conversation, name);

        pcx_list_insert(playerbase->players.prev, &player->link);
        add_player_to_hash(playerbase, player);

        playerbase->n_players++;

        queue_gc_source(playerbase);

        return player;
}

int
pcx_playerbase_get_n_players(struct pcx_playerbase *playerbase)
{
        return playerbase->n_players;
}

void
pcx_playerbase_free(struct pcx_playerbase *playerbase)
{
        struct pcx_player *player, *tmp;

        pcx_list_for_each_safe(player, tmp, &playerbase->players, link) {
                pcx_player_free(player);
        }

        pcx_free(playerbase->hash_table);

        if (playerbase->gc_source)
                pcx_main_context_remove_source(playerbase->gc_source);

        pcx_free(playerbase);
}
