/*
 * Puxcobot - A robot to play Love in Esperanto (Puĉo)
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

#include "pcx-love.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"

#define PCX_LOVE_MIN_PLAYERS 2
#define PCX_LOVE_MAX_PLAYERS 4

struct pcx_love {
        int n_players;
        int current_player;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        enum pcx_text_language language;
};

static void *
create_game_cb(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_LOVE_MAX_PLAYERS);

        struct pcx_love *love = pcx_calloc(sizeof *love);

        love->language = language;
        love->callbacks = *callbacks;
        love->user_data = user_data;

        love->n_players = n_players;
        love->current_player = rand() % n_players;

        return love;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup("help");
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_love *love = user_data;

        assert(player_num >= 0 && player_num < love->n_players);

        int extra_data;
        const char *colon = strchr(callback_data, ':');

        if (colon == NULL) {
                extra_data = -1;
                colon = callback_data + strlen(callback_data);
        } else {
                char *tail;

                errno = 0;
                extra_data = strtol(colon + 1, &tail, 10);

                if (*tail || errno || extra_data < 0)
                        return;
        }

        if (colon <= callback_data)
                return;

        char *main_data = pcx_strndup(callback_data, colon - callback_data);

        pcx_free(main_data);
}

static void
free_game_cb(void *data)
{
        struct pcx_love *love = data;

        pcx_free(love);
}

const struct pcx_game
pcx_love_game = {
        .name = "loveletter",
        .name_string = PCX_TEXT_STRING_NAME_LOVE,
        .start_command = PCX_TEXT_STRING_LOVE_START_COMMAND,
        .min_players = PCX_LOVE_MIN_PLAYERS,
        .max_players = PCX_LOVE_MAX_PLAYERS,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
