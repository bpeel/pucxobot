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
#include <limits.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "pcx-utf8.h"
#include "pcx-html.h"
#include "pcx-chameleon-list.h"
#include "pcx-chameleon-help.h"
#include "pcx-buffer.h"

#define PCX_CHAMELEON_MIN_PLAYERS 4
#define PCX_CHAMELEON_MAX_PLAYERS 6

struct pcx_chameleon_player {
        char *name;
        struct pcx_buffer guess;
        int vote;
        int vote_count;
        int score;
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

        size_t n_groups;
        int *group_order;

        int next_group_index;
        const struct pcx_chameleon_list_group *current_group;

        int n_players_sent_clue;
        int chameleon_player;
        int dealer;

        /* Bitmask of players that have voted */
        int voted_players;

        const struct pcx_chameleon_list_word *secret_word;

        struct pcx_main_context_source *vote_timeout;
        struct pcx_game_button *vote_buttons;

        /* Options for unit testing */
        int (* rand_func)(void *user_data);
};

static void
start_vote_timeout(struct pcx_chameleon *chameleon);

static void
escape_string(struct pcx_chameleon *chameleon,
              struct pcx_buffer *buf,
              enum pcx_text_string string)
{
        const char *value = pcx_text_get(chameleon->language, string);
        pcx_html_escape(buf, value);
}

static void
add_marker_message(struct pcx_chameleon *chameleon,
                   struct pcx_buffer *buf,
                   enum pcx_text_string string,
                   const char *replacement)
{
        const char *text = pcx_text_get(chameleon->language, string);
        const char *marker = strstr(text, "%p");

        assert(marker);

        pcx_html_escape_limit(buf, text, marker - text);

        pcx_buffer_append_string(buf, "<b>");
        pcx_html_escape(buf, replacement);
        pcx_buffer_append_string(buf, "</b>");

        pcx_html_escape(buf, marker + 2);
}

static void
add_player_message(struct pcx_chameleon *chameleon,
                   struct pcx_buffer *buf,
                   enum pcx_text_string string,
                   int player_num)
{
        add_marker_message(chameleon,
                           buf,
                           string,
                           chameleon->players[player_num].name);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_chameleon *chameleon = user_data;
        chameleon->game_over_source = NULL;
        chameleon->callbacks.game_over(chameleon->user_data);
}

static unsigned
get_winner(struct pcx_chameleon *chameleon)
{
        unsigned winner = 0;
        int best_score = INT_MIN;

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                const struct pcx_chameleon_player *player =
                        chameleon->players + i;

                if (player->score > best_score) {
                        winner = i;
                        best_score = player->score;
                }
        }

        return winner;
}

static int
get_next_clue_player(struct pcx_chameleon *chameleon)
{
        return ((chameleon->dealer +
                 chameleon->n_players_sent_clue) %
                chameleon->n_players);
}

static void
end_game(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        unsigned winner = get_winner(chameleon);

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(chameleon->language,
                                              PCX_TEXT_STRING_WINS_PLAIN),
                                 chameleon->players[winner].name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (const char *) buf.data;
        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);

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

static int
get_random(struct pcx_chameleon *chameleon)
{
        return chameleon->rand_func(chameleon->user_data);
}

static void
shuffle_groups(struct pcx_chameleon *chameleon)
{
        for (unsigned i = 0; i < chameleon->n_groups; i++)
                chameleon->group_order[i] = i;

        if (chameleon->n_groups < 2)
                return;

        for (unsigned i = chameleon->n_groups - 1; i > 0; i--) {
                int j = get_random(chameleon) % (i + 1);
                int t = chameleon->group_order[j];
                chameleon->group_order[j] = chameleon->group_order[i];
                chameleon->group_order[i] = t;
        }
}

static void
pick_secret_word(struct pcx_chameleon *chameleon)
{
        size_t n_words = pcx_list_length(&chameleon->current_group->words);

        int secret_word_num = get_random(chameleon) % n_words;

        const struct pcx_chameleon_list_word *word;

        pcx_list_for_each(word, &chameleon->current_group->words, link) {
                if (secret_word_num-- == 0) {
                        chameleon->secret_word = word;
                        return;
                }
        }

        assert(!"secret word not in list?");
}

static void
send_word_list(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        escape_string(chameleon, &buf, PCX_TEXT_STRING_WORDS_ARE);

        pcx_buffer_append_string(&buf, "\n\n<b>");
        pcx_html_escape(&buf, chameleon->current_group->topic);
        pcx_buffer_append_string(&buf, "</b>\n\n");

        const struct pcx_chameleon_list_word *word;

        pcx_list_for_each(word, &chameleon->current_group->words, link) {
                pcx_html_escape(&buf, word->word);

                if (word->link.next != &chameleon->current_group->words)
                        pcx_buffer_append_string(&buf, "\n");
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (char *) buf.data;

        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);
}

static void
send_secret_word(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        escape_string(chameleon, &buf, PCX_TEXT_STRING_SECRET_WORD_IS);

        pcx_buffer_append_string(&buf, " <b>");
        pcx_html_escape(&buf, chameleon->secret_word->word);
        pcx_buffer_append_string(&buf, "</b>");

        struct pcx_game_message normal_message = PCX_GAME_DEFAULT_MESSAGE;

        normal_message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        normal_message.text = (const char *) buf.data;

        struct pcx_game_message chameleon_message = PCX_GAME_DEFAULT_MESSAGE;

        chameleon_message.text =
                pcx_text_get(chameleon->language,
                             PCX_TEXT_STRING_YOU_ARE_THE_CHAMELEON);

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                struct pcx_game_message *message =
                        i == chameleon->chameleon_player ?
                        &chameleon_message :
                        &normal_message;
                message->target = i;
                chameleon->callbacks.send_message(message,
                                                  chameleon->user_data);
        }

        pcx_buffer_destroy(&buf);
}

static void
send_clue_question(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        add_player_message(chameleon,
                           &buf,
                           PCX_TEXT_STRING_CLUE_QUESTION,
                           get_next_clue_player(chameleon));

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (const char *) buf.data;

        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);
}

static bool
is_game_over(struct pcx_chameleon *chameleon)
{
        /* If there are no more groups left then we have to stop.
         * Ideally there should be enough cards so that the game
         * always ends before this.
         */
        if (chameleon->next_group_index >= chameleon->n_groups)
                return true;

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                const struct pcx_chameleon_player *player =
                        chameleon->players + i;

                if (player->score >= 5)
                        return true;
        }

        return false;
}

static void
start_round(struct pcx_chameleon *chameleon)
{
        if (is_game_over(chameleon)) {
                end_game(chameleon);
                return;
        }

        int group_num = chameleon->group_order[chameleon->next_group_index++];

        struct pcx_chameleon_list *word_list =
                chameleon->class_data->word_list;

        chameleon->current_group =
                pcx_chameleon_list_get_group(word_list, group_num);
        chameleon->n_players_sent_clue = 0;
        chameleon->chameleon_player =
                get_random(chameleon) % chameleon->n_players;
        chameleon->voted_players = 0;
        pick_secret_word(chameleon);

        send_word_list(chameleon);
        send_secret_word(chameleon);
        send_clue_question(chameleon);
}

static void
stop_vote_timeout(struct pcx_chameleon *chameleon)
{
        if (chameleon->vote_timeout == NULL)
                return;

        pcx_main_context_remove_source(chameleon->vote_timeout);
        chameleon->vote_timeout = NULL;
}

static void
vote_cb(struct pcx_main_context_source *source,
        void *user_data)
{
        struct pcx_chameleon *chameleon = user_data;

        chameleon->vote_timeout = NULL;

        start_vote_timeout(chameleon);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = pcx_text_get(chameleon->language,
                                    PCX_TEXT_STRING_YOU_CAN_VOTE_FOR_A_PLAYER);
        message.n_buttons = chameleon->n_players;
        message.buttons = chameleon->vote_buttons;

        chameleon->callbacks.send_message(&message, chameleon->user_data);
}

static void
start_vote_timeout(struct pcx_chameleon *chameleon)
{
        if (chameleon->vote_timeout)
                return;

        chameleon->vote_timeout =
                pcx_main_context_add_timeout(NULL,
                                             1 * 60 * 1000,
                                             vote_cb,
                                             chameleon);
}

static void
start_voting(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        escape_string(chameleon, &buf, PCX_TEXT_STRING_START_DEBATE);

        pcx_buffer_append_string(&buf, "\n");

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                const struct pcx_chameleon_player *player =
                        chameleon->players + i;

                pcx_buffer_append_string(&buf, "\n<b>");
                pcx_html_escape(&buf, player->name);
                pcx_buffer_append_string(&buf, "</b>: ");
                pcx_html_escape(&buf, (const char *) player->guess.data);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (const char *) buf.data;

        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);

        start_vote_timeout(chameleon);
}

static int
default_rand_func(void *user_data)
{
        return rand();
}

struct pcx_chameleon *
pcx_chameleon_new(const struct pcx_config *config,
                  const struct pcx_game_callbacks *callbacks,
                  void *user_data,
                  enum pcx_text_language language,
                  int n_players,
                  const char *const *names,
                  const struct pcx_chameleon_debug_overrides *overrides)
{
        assert(n_players > 0 && n_players <= PCX_CHAMELEON_MAX_PLAYERS);

        struct pcx_chameleon *chameleon = pcx_calloc(sizeof *chameleon);

        chameleon->language = language;
        chameleon->callbacks = *callbacks;
        chameleon->user_data = user_data;

        chameleon->rand_func = default_rand_func;

        if (overrides) {
                if (overrides->rand_func)
                        chameleon->rand_func = overrides->rand_func;
        }

        chameleon->n_players = n_players;

        chameleon->players = pcx_calloc(n_players *
                                        sizeof (struct pcx_chameleon_player));
        chameleon->vote_buttons = pcx_alloc(n_players *
                                            sizeof (struct pcx_game_button));

        chameleon->dealer = get_random(chameleon) % n_players;

        for (unsigned i = 0; i < n_players; i++) {
                chameleon->players[i].name = pcx_strdup(names[i]);
                pcx_buffer_init(&chameleon->players[i].guess);

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "vote:%i", i);

                chameleon->vote_buttons[i].text =
                        chameleon->players[i].name;
                chameleon->vote_buttons[i].data =
                        (char *) buf.data;
        }

        chameleon->class_data =
                pcx_class_store_ref_data(callbacks->get_class_store(user_data),
                                         config,
                                         &pcx_chameleon_game,
                                         language,
                                         &class_store_callbacks);

        struct pcx_chameleon_list *word_list =
                chameleon->class_data->word_list;

        if (word_list) {
                chameleon->n_groups =
                        pcx_chameleon_list_get_n_groups(word_list);
                chameleon->group_order =
                        pcx_alloc(sizeof (int) * chameleon->n_groups);
                shuffle_groups(chameleon);
        }

        start_round(chameleon);

        return chameleon;
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        return pcx_chameleon_new(config,
                                 callbacks,
                                 user_data,
                                 language,
                                 n_players,
                                 names,
                                 NULL /* overrides */);
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_chameleon_help[language]);
}

static void
tally_votes(struct pcx_chameleon *chameleon,
            struct pcx_buffer *buf)
{
        for (unsigned i = 0; i < chameleon->n_players; i++)
                chameleon->players[i].vote_count = 0;

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                pcx_buffer_append_string(buf, "<b>");
                pcx_html_escape(buf, chameleon->players[i].name);
                pcx_buffer_append_string(buf, "</b>: ");

                int voted_player = chameleon->players[i].vote;

                pcx_html_escape(buf, chameleon->players[voted_player].name);
                pcx_buffer_append_c(buf, '\n');

                chameleon->players[voted_player].vote_count++;
        }
}

static int
get_chosen_player(struct pcx_chameleon *chameleon,
                  struct pcx_buffer *buf)
{
        int most_voted_player = -1;
        int most_votes = -1;
        int equalled_count = 0;

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                int votes = chameleon->players[i].vote_count;

                if (votes > most_votes) {
                        most_votes = votes;
                        most_voted_player = i;
                        equalled_count = 1;
                } else if (votes == most_votes) {
                        equalled_count++;
                }
        }

        if (equalled_count > 1) {
                add_player_message(chameleon,
                                   buf,
                                   PCX_TEXT_STRING_ITS_A_DRAW,
                                   chameleon->dealer);
                most_voted_player = chameleon->players[chameleon->dealer].vote;
                pcx_buffer_append_c(buf, ' ');
        }

        add_player_message(chameleon,
                           buf,
                           PCX_TEXT_STRING_CHOSEN_PLAYER,
                           most_voted_player);

        return most_voted_player;
}

static void
add_scores(struct pcx_chameleon *chameleon,
           struct pcx_buffer *buf)
{
        escape_string(chameleon, buf, PCX_TEXT_STRING_SCORES);
        pcx_buffer_append_string(buf, "\n");

        for (unsigned i = 0; i < chameleon->n_players; i++) {
                const struct pcx_chameleon_player *player =
                        chameleon->players + i;

                pcx_buffer_append_string(buf, "\n<b>");
                pcx_html_escape(buf, player->name);
                pcx_buffer_append_printf(buf, "</b>: %i", player->score);
        }
}

static void
add_word_buttons(struct pcx_chameleon *chameleon,
                 struct pcx_game_message *message)
{
        message->n_buttons = pcx_list_length(&chameleon->current_group->words);

        struct pcx_game_button *buttons = pcx_alloc(sizeof message->buttons[0] *
                                                    message->n_buttons);

        const struct pcx_chameleon_list_word *word;
        int i = 0;

        pcx_list_for_each(word, &chameleon->current_group->words, link) {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "guess:%i", i);

                buttons[i].text = word->word;
                buttons[i].data = (char *) buf.data;

                i++;
        }

        message->buttons = buttons;
}

static void
end_voting(struct pcx_chameleon *chameleon)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        stop_vote_timeout(chameleon);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;

        escape_string(chameleon, &buf, PCX_TEXT_STRING_EVERYBODY_VOTED);
        pcx_buffer_append_string(&buf, "\n\n");

        tally_votes(chameleon, &buf);

        pcx_buffer_append_c(&buf, '\n');

        int chosen_player = get_chosen_player(chameleon, &buf);

        pcx_buffer_append_string(&buf, "\n\n");

        if (chosen_player == chameleon->chameleon_player) {
                escape_string(chameleon,
                              &buf,
                              PCX_TEXT_STRING_YOU_FOUND_THE_CHAMELEON);

                pcx_buffer_append_string(&buf, "\n\n");

                add_player_message(chameleon,
                                   &buf,
                                   PCX_TEXT_STRING_NOW_GUESS,
                                   chameleon->chameleon_player);

                add_word_buttons(chameleon, &message);
        } else {
                escape_string(chameleon,
                              &buf,
                              PCX_TEXT_STRING_YOU_DIDNT_FIND_THE_CHAMELEON);
                pcx_buffer_append_string(&buf, "\n\n");
                add_player_message(chameleon,
                                   &buf,
                                   PCX_TEXT_STRING_CHAMELEON_WINS_POINTS,
                                   chameleon->chameleon_player);
                chameleon->players[chameleon->chameleon_player].score += 2;

                pcx_buffer_append_string(&buf, "\n\n");

                add_scores(chameleon, &buf);
        }

        message.text = (char *) buf.data;

        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);

        for (unsigned i = 0; i < message.n_buttons; i++)
                pcx_free((char *) message.buttons[i].data);
        pcx_free((void *) message.buttons);

        chameleon->dealer = chameleon->chameleon_player;

        if (message.n_buttons == 0)
                start_round(chameleon);
}

static void
show_voted_message(struct pcx_chameleon *chameleon,
                   int player_num)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(chameleon->language,
                                              PCX_TEXT_STRING_PLAYER_VOTED),
                                 chameleon->players[player_num].name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (char *) buf.data;
        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);
}

static void
handle_vote(struct pcx_chameleon *chameleon,
            int player_num,
            long vote_num)
{
        if (vote_num >= chameleon->n_players)
                return;

        if (chameleon->n_players_sent_clue < chameleon->n_players ||
            chameleon->voted_players == (1 << chameleon->n_players) - 1) {
                /* It’s not time to vote */
                return;
        }

        chameleon->players[player_num].vote = vote_num;
        chameleon->voted_players |= 1 << player_num;

        if (chameleon->voted_players == (1 << chameleon->n_players) - 1)
                end_voting(chameleon);
        else
                show_voted_message(chameleon, player_num);
}

static void
handle_guess(struct pcx_chameleon *chameleon,
             int player_num,
             long word_num)
{
        if (chameleon->n_players_sent_clue < chameleon->n_players ||
            chameleon->voted_players != (1 << chameleon->n_players) - 1) {
                /* It’s not time to guess */
                return;
        }

        if (player_num != chameleon->chameleon_player)
                return;

        const struct pcx_chameleon_list_word *word;

        pcx_list_for_each(word, &chameleon->current_group->words, link) {
                if (word_num-- == 0)
                        goto found_word;
        }

        /* word_num is off the end of the list */
        return;

found_word:

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        add_marker_message(chameleon,
                           &buf,
                           PCX_TEXT_STRING_CHAMELEON_GUESSED,
                           word->word);

        pcx_buffer_append_string(&buf, "\n\n");

        if (word == chameleon->secret_word) {
                escape_string(chameleon, &buf, PCX_TEXT_STRING_CORRECT_GUESS);
                pcx_buffer_append_string(&buf, "\n\n");
                add_player_message(chameleon,
                                   &buf,
                                   PCX_TEXT_STRING_ESCAPED_SCORE,
                                   chameleon->chameleon_player);
                chameleon->players[chameleon->chameleon_player].score += 1;
        } else {
                add_marker_message(chameleon,
                                   &buf,
                                   PCX_TEXT_STRING_CORRECT_WORD_IS,
                                   chameleon->secret_word->word);
                pcx_buffer_append_string(&buf, "\n\n");
                add_player_message(chameleon,
                                   &buf,
                                   PCX_TEXT_STRING_CAUGHT_SCORE,
                                   chameleon->chameleon_player);

                for (unsigned i = 0; i < chameleon->n_players; i++) {
                        if (i == chameleon->chameleon_player)
                                continue;

                        chameleon->players[i].score += 2;
                }
        }

        pcx_buffer_append_string(&buf, "\n\n");
        add_scores(chameleon, &buf);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (char *) buf.data;

        chameleon->callbacks.send_message(&message, chameleon->user_data);

        pcx_buffer_destroy(&buf);

        start_round(chameleon);
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_chameleon *chameleon = user_data;

        assert(player_num >= 0 && player_num < chameleon->n_players);

        const char *colon = strchr(callback_data, ':');

        if (colon == NULL)
                return;

        char *tail;

        errno = 0;
        long num = strtol(colon + 1, &tail, 10);

        if (*tail || errno || num < 0)
                return;

        static const struct {
                const char *key;
                void (* func)(struct pcx_chameleon *, int, long);
        } handlers[] = {
                { "vote", handle_vote },
                { "guess", handle_guess },
        };

        size_t key_length = colon - callback_data;

        for (unsigned i = 0; i < PCX_N_ELEMENTS(handlers); i++) {
                if (key_length != strlen(handlers[i].key) ||
                    memcmp(handlers[i].key, callback_data, key_length))
                        continue;

                handlers[i].func(chameleon, player_num, num);

                break;
        }
}

static bool
is_space_char(char ch)
{
        return ch != 0 && strchr("\r\n \t", ch) != NULL;
}

static void
handle_message_cb(void *data,
                  int player_num,
                  const char *text)
{
        struct pcx_chameleon *chameleon = data;

        if (chameleon->n_players_sent_clue >= chameleon->n_players ||
            player_num != get_next_clue_player(chameleon))
                return;

        while (is_space_char(*text))
                text++;

        size_t length = strlen(text);

        while (length > 0 && is_space_char(text[length - 1]))
                length--;

        if (length <= 0)
                return;

        if (memchr(text, '\n', length))
                return;

        struct pcx_buffer *buf = &chameleon->players[player_num].guess;
        pcx_buffer_set_length(buf, 0);
        pcx_buffer_append(buf, text, length);
        pcx_buffer_append_c(buf, '\0');

        chameleon->n_players_sent_clue++;

        if (chameleon->n_players_sent_clue < chameleon->n_players)
                send_clue_question(chameleon);
        else
                start_voting(chameleon);
}

static void
free_game_cb(void *data)
{
        struct pcx_chameleon *chameleon = data;

        stop_vote_timeout(chameleon);

        for (int i = 0; i < chameleon->n_players; i++) {
                pcx_free(chameleon->players[i].name);
                pcx_buffer_destroy(&chameleon->players[i].guess);
                pcx_free((void *) chameleon->vote_buttons[i].data);
        }

        pcx_free(chameleon->players);
        pcx_free(chameleon->vote_buttons);

        if (chameleon->game_over_source)
                pcx_main_context_remove_source(chameleon->game_over_source);

        struct pcx_class_store *class_store =
                chameleon->callbacks.get_class_store(chameleon->user_data);
        pcx_class_store_unref_data(class_store, chameleon->class_data);

        pcx_free(chameleon->group_order);

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
