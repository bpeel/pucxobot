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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <curl/curl.h>
#include <signal.h>

#include "pcx-tty-game.h"
#include "pcx-bot.h"
#include "pcx-game.h"
#include "pcx-main-context.h"
#include "pcx-config.h"
#include "pcx-curl-multi.h"

struct pcx_main {
        struct pcx_tty_game *tty_game;
        struct pcx_curl_multi *pcurl;
        size_t n_bots;
        struct pcx_bot **bots;
        struct pcx_config *config;
        bool curl_inited;
        bool quit;
};

static void
quit_cb(struct pcx_main_context_source *source,
        int signal_num,
        void *user_data)
{
        struct pcx_main *data = user_data;

        data->quit = true;
}

static void
info_cb(struct pcx_main_context_source *source,
        int signal_num,
        void *user_data)
{
        struct pcx_main *data = user_data;
        int total_games = 0;
        struct pcx_config_bot *bot;
        int bot_num = 0;

        pcx_list_for_each(bot, &data->config->bots, link) {
                int n_games = pcx_bot_get_n_running_games(data->bots[bot_num]);
                printf("@%s: %i\n", bot->botname, n_games);
                total_games += n_games;
                bot_num++;
        }

        printf("Total games: %i\n", total_games);

        if (signal_num == SIGUSR2) {
                if (total_games == 0) {
                        printf("No games running, quitting\n");
                        data->quit = true;
                } else {
                        printf("Not quitting due to running games\n");
                }
        }
}

static bool
init_main_tty(struct pcx_main *data,
              int argc,
              char **argv)
{
        if (argc - 1 > PCX_GAME_MAX_PLAYERS) {
                fprintf(stderr, "usage: pucxobot <tty_file>…\n");
                return false;
        }

        struct pcx_error *error = NULL;
        data->tty_game = pcx_tty_game_new(argc - 1,
                                          (const char *const *) argv + 1,
                                          &error);

        if (data->tty_game == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                return false;
        }

        return true;
}

static bool
init_main_bots(struct pcx_main *data)
{
        struct pcx_error *error = NULL;

        data->config = pcx_config_load(&error);
        if (data->config == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                return false;
        }

        curl_global_init(CURL_GLOBAL_ALL);
        data->curl_inited = true;

        data->pcurl = pcx_curl_multi_new();

        time_t t;
        time(&t);
        srand(t);

        struct pcx_config_bot *bot;

        pcx_list_for_each(bot, &data->config->bots, link) {
                data->n_bots++;
        }

        data->bots = pcx_alloc((sizeof *data->bots) * MAX(data->n_bots, 1));

        unsigned i = 0;

        pcx_list_for_each(bot, &data->config->bots, link) {
                data->bots[i++] = pcx_bot_new(data->pcurl, bot);
        }

        return true;
}

static void
destroy_main(struct pcx_main *data)
{
        if (data->tty_game)
                pcx_tty_game_free(data->tty_game);
        for (unsigned i = 0; i < data->n_bots; i++)
                pcx_bot_free(data->bots[i]);
        pcx_free(data->bots);
        if (data->pcurl)
                pcx_curl_multi_free(data->pcurl);
        if (data->curl_inited)
                curl_global_cleanup();
        if (data->config)
                pcx_config_free(data->config);
}

int
main(int argc, char **argv)
{
        struct pcx_main data = {
                .tty_game = NULL,
                .pcurl = NULL,
                .n_bots = 0,
                .bots = NULL,
                .config = NULL,
                .curl_inited = false,
                .quit = false,
        };

        if (argc > 1) {
                if (!init_main_tty(&data, argc, argv))
                        return EXIT_FAILURE;
        } else {
                if (!init_main_bots(&data))
                        return EXIT_FAILURE;
        }

        struct pcx_main_context_source *int_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGINT,
                                                   quit_cb,
                                                   &data);
        struct pcx_main_context_source *term_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGTERM,
                                                   quit_cb,
                                                   &data);
        struct pcx_main_context_source *usr1_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGUSR1,
                                                   info_cb,
                                                   &data);
        struct pcx_main_context_source *usr2_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGUSR2,
                                                   info_cb,
                                                   &data);

        do
                pcx_main_context_poll(NULL);
        while (!data.quit);

        pcx_main_context_remove_source(usr2_source);
        pcx_main_context_remove_source(usr1_source);
        pcx_main_context_remove_source(term_source);
        pcx_main_context_remove_source(int_source);

        destroy_main(&data);

        pcx_main_context_free(pcx_main_context_get_default());

        return EXIT_SUCCESS;
}
