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

#include "pcx-tty-game.h"
#include "pcx-bot.h"
#include "pcx-game.h"
#include "pcx-main-context.h"
#include "pcx-config.h"
#include "pcx-curl-multi.h"

static void
quit_cb(struct pcx_main_context_source *source,
        void *user_data)
{
        bool *quit = user_data;
        *quit = true;
}

int
main(int argc, char **argv)
{
        struct pcx_tty_game *tty_game = NULL;
        struct pcx_curl_multi *pcurl = NULL;
        size_t n_bots = 0;
        struct pcx_bot **bots = NULL;
        struct pcx_config *config = NULL;
        bool curl_inited = false;

        if (argc > 1) {
                if (argc - 1 > PCX_GAME_MAX_PLAYERS) {
                        fprintf(stderr, "usage: pucxobot <tty_file>…\n");
                        return EXIT_FAILURE;
                }

                struct pcx_error *error = NULL;
                tty_game = pcx_tty_game_new(argc - 1,
                                            (const char *const *) argv + 1,
                                            &error);

                if (tty_game == NULL) {
                        fprintf(stderr, "%s\n", error->message);
                        pcx_error_free(error);
                        return EXIT_FAILURE;
                }
        } else {
                struct pcx_error *error = NULL;

                config = pcx_config_load(&error);
                if (config == NULL) {
                        fprintf(stderr, "%s\n", error->message);
                        pcx_error_free(error);
                        return EXIT_FAILURE;
                }

                curl_global_init(CURL_GLOBAL_ALL);
                curl_inited = true;

                pcurl = pcx_curl_multi_new();

                time_t t;
                time(&t);
                srand(t);

                struct pcx_config_bot *bot;

                pcx_list_for_each(bot, &config->bots, link) {
                        n_bots++;
                }

                bots = pcx_alloc((sizeof *bots) * MAX(n_bots, 1));

                unsigned i = 0;

                pcx_list_for_each(bot, &config->bots, link) {
                        bots[i++] = pcx_bot_new(pcurl, bot);
                }
        }

        bool quit = false;
        struct pcx_main_context_source *quit_source =
                pcx_main_context_add_quit(NULL, quit_cb, &quit);

        do
                pcx_main_context_poll(NULL);
        while (!quit);

        pcx_main_context_remove_source(quit_source);

        if (tty_game)
                pcx_tty_game_free(tty_game);
        for (unsigned i = 0; i < n_bots; i++)
                pcx_bot_free(bots[i]);
        pcx_free(bots);
        if (pcurl)
                pcx_curl_multi_free(pcurl);
        if (curl_inited)
                curl_global_cleanup();
        if (config)
                pcx_config_free(config);

        pcx_main_context_free(pcx_main_context_get_default());

        return EXIT_SUCCESS;
}
