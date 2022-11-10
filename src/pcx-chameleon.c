/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2022  Neil Roberts
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

#include "pcx-chameleon.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "pcx-utf8.h"
#include "pcx-html.h"
#include "pcx-chameleon-list.h"

#define PCX_CHAMELEON_MIN_PLAYERS 4
#define PCX_CHAMELEON_MAX_PLAYERS 6

struct pcx_chameleon_player {
        char *name;
};

struct pcx_chameleon_class_data {
        struct pcx_chameleon_list *word_list;
};

struct pcx_chameleon {
        int n_players;

        struct pcx_chameleon_player *players;
        struct pcx_game_callbacks callbacks;
        void *user_data;

        enum pcx_text_language language;
        struct pcx_main_context_source *game_over_source;

        struct pcx_chameleon_class_data *class_data;
};

static void
escape_string(struct pcx_chameleon *chameleon,
              struct pcx_buffer *buf,
              enum pcx_text_string string)
{
        const char *value = pcx_text_get(chameleon->language, string);
        pcx_html_escape(buf, value);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_chameleon *chameleon = user_data;
        chameleon->game_over_source = NULL;
        chameleon->callbacks.game_over(chameleon->user_data);
}

static void
end_game(struct pcx_chameleon *chameleon)
{
        if (chameleon->game_over_source == NULL) {
                chameleon->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     chameleon);
        }
}

static void
open_word_list(struct pcx_chameleon_class_data *data,
               const struct pcx_config *config,
               enum pcx_text_language language)
{
        const char *language_code = pcx_text_get(language,
                                                 PCX_TEXT_STRING_LANGUAGE_CODE);
        char *full_filename = pcx_strconcat(config->data_dir,
                                            "/chameleon-word-list-",
                                            language_code,
                                            ".txt",
                                            NULL);

        struct pcx_error *error = NULL;

        data->word_list = pcx_chameleon_list_new(full_filename, &error);

        pcx_free(full_filename);

        if (data->word_list == NULL) {
                pcx_log("Error opening chameleon word list: %s",
                        error->message);
                pcx_error_free(error);
        }
}

static void *
create_class_store_data_cb(const struct pcx_config *config,
                           enum pcx_text_language language)
{
        struct pcx_chameleon_class_data *data = pcx_calloc(sizeof *data);

        open_word_list(data, config, language);

        return data;
}

static void
free_class_store_data_cb(void *user_data)
{
        struct pcx_chameleon_class_data *data = user_data;

        if (data->word_list)
                pcx_chameleon_list_free(data->word_list);

        pcx_free(data);
}

static const struct pcx_class_store_callbacks
class_store_callbacks = {
        .create_data = create_class_store_data_cb,
        .free_data = free_class_store_data_cb,
};

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_CHAMELEON_MAX_PLAYERS);

        struct pcx_chameleon *chameleon = pcx_calloc(sizeof *chameleon);

        chameleon->language = language;
        chameleon->callbacks = *callbacks;
        chameleon->user_data = user_data;

        chameleon->n_players = n_players;

        chameleon->players = pcx_calloc(n_players *
                                        sizeof (struct pcx_chameleon_player));

        for (unsigned i = 0; i < n_players; i++)
                chameleon->players[i].name = pcx_strdup(names[i]);

        chameleon->class_data =
                pcx_class_store_ref_data(callbacks->get_class_store(user_data),
                                         config,
                                         &pcx_chameleon_game,
                                         language,
                                         &class_store_callbacks);

        return chameleon;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup("stub");
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
}

static void
handle_message_cb(void *data,
                  int player_num,
                  const char *text)
{
        struct pcx_chameleon *chameleon = data;
}

static void
free_game_cb(void *data)
{
        struct pcx_chameleon *chameleon = data;

        for (int i = 0; i < chameleon->n_players; i++)
                pcx_free(chameleon->players[i].name);

        pcx_free(chameleon->players);

        if (chameleon->game_over_source)
                pcx_main_context_remove_source(chameleon->game_over_source);

        struct pcx_class_store *class_store =
                chameleon->callbacks.get_class_store(chameleon->user_data);
        pcx_class_store_unref_data(class_store, chameleon->class_data);

        pcx_free(chameleon);
}

const struct pcx_game
pcx_chameleon_game = {
        .name = "chameleon",
        .name_string = PCX_TEXT_STRING_NAME_CHAMELEON,
        .start_command = PCX_TEXT_STRING_CHAMELEON_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_CHAMELEON_START_COMMAND_DESCRIPTION,
        .min_players = PCX_CHAMELEON_MIN_PLAYERS,
        .max_players = PCX_CHAMELEON_MAX_PLAYERS,
        .needs_private_messages = true,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .handle_message_cb = handle_message_cb,
        .free_game_cb = free_game_cb
};
