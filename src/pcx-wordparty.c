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

#include "pcx-wordparty.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "pcx-trie.h"
#include "pcx-syllabary.h"
#include "pcx-utf8.h"
#include "pcx-hat.h"
#include "pcx-html.h"

#define PCX_WORDPARTY_MIN_PLAYERS 2
#define PCX_WORDPARTY_MAX_PLAYERS 32

#define PCX_WORDPARTY_LIVES 2

/* The timeout that a player needs will be in this range, depending on
 * the difficulty.
 */
#define PCX_WORDPARTY_MIN_WORD_TIMEOUT (5 * 1000)
#define PCX_WORDPARTY_MAX_WORD_TIMEOUT (60 * 1000)

struct pcx_wordparty_player {
        char *name;
        int lives;
};

struct pcx_wordparty {
        int n_players;

        struct pcx_wordparty_player *players;
        struct pcx_game_callbacks callbacks;
        void *user_data;

        enum pcx_text_language language;
        struct pcx_main_context_source *game_over_source;

        struct pcx_trie *trie;
        struct pcx_syllabary *syllabary;

        struct pcx_buffer word_buf;

        int current_player;

        struct pcx_main_context_source *word_timeout;

        char current_syllable[PCX_SYLLABARY_MAX_SYLLABLE_LENGTH + 1];
};

static void
start_turn(struct pcx_wordparty *wordparty);

static void
escape_string(struct pcx_wordparty *wordparty,
              struct pcx_buffer *buf,
              enum pcx_text_string string)
{
        const char *value = pcx_text_get(wordparty->language, string);
        pcx_html_escape(buf, value);
}

static void
add_player_text(struct pcx_wordparty *wordparty,
                struct pcx_buffer *buf,
                enum pcx_text_string string,
                const struct pcx_wordparty_player *player)
{
        const char *str = pcx_text_get(wordparty->language, string);

        const char *player_pos = strstr(str, "%s");

        if (player_pos == NULL) {
                pcx_html_escape(buf, str);
                return;
        }

        pcx_html_escape_limit(buf, str, player_pos - str);
        pcx_html_escape(buf, player->name);
        pcx_html_escape(buf, player_pos + 2);
}

static void
remove_word_timeout(struct pcx_wordparty *wordparty)
{
        if (wordparty->word_timeout == NULL)
                return;

        pcx_main_context_remove_source(wordparty->word_timeout);
        wordparty->word_timeout = NULL;
}

static void
word_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct pcx_wordparty *wordparty = user_data;

        wordparty->word_timeout = NULL;

        struct pcx_wordparty_player *player =
                wordparty->players + wordparty->current_player;

        player->lives--;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (player->lives <= 0) {
                pcx_buffer_append_string(&buf, "💥 ");
                add_player_text(wordparty,
                                &buf,
                                PCX_TEXT_STRING_LOST_ALL_LIVES,
                                player);
        } else {
                pcx_buffer_append_string(&buf, "💔 ");
                add_player_text(wordparty,
                                &buf,
                                PCX_TEXT_STRING_LOST_A_LIFE,
                                player);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (const char *) buf.data;

        wordparty->callbacks.send_message(&message, wordparty->user_data);

        pcx_buffer_destroy(&buf);

        start_turn(wordparty);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_wordparty *wordparty = user_data;
        wordparty->game_over_source = NULL;
        wordparty->callbacks.game_over(wordparty->user_data);
}

static void
end_game(struct pcx_wordparty *wordparty)
{
        const struct pcx_wordparty_player *winner =
                wordparty->players + wordparty->current_player;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        escape_string(wordparty, &buf, PCX_TEXT_STRING_GAME_OVER_WINNER);

        pcx_buffer_append_string(&buf, "\n\n");

        pcx_buffer_append_string(&buf, "🏆 <b>");
        pcx_html_escape(&buf, winner->name);
        pcx_buffer_append_string(&buf, "</b> 🏆");

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (const char *) buf.data;

        wordparty->callbacks.send_message(&message, wordparty->user_data);

        pcx_buffer_destroy(&buf);

        if (wordparty->game_over_source == NULL) {
                wordparty->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     wordparty);
        }
}

static int
count_players(struct pcx_wordparty *wordparty)
{
        int count = 0;

        for (int i = 0; i < wordparty->n_players; i++) {
                if (wordparty->players[i].lives > 0)
                        count++;
        }

        return count;
}

static void
start_turn(struct pcx_wordparty *wordparty)
{
        int next_player = wordparty->current_player;

        while (true) {
                next_player = (next_player + 1) % wordparty->n_players;

                if (wordparty->players[next_player].lives > 0 ||
                    next_player == wordparty->current_player)
                        break;
        }

        wordparty->current_player = next_player;

        if (count_players(wordparty) <= 1) {
                end_game(wordparty);
                return;
        }

        int difficulty;

        if (wordparty->syllabary == NULL ||
            !pcx_syllabary_get_random(wordparty->syllabary,
                                      wordparty->current_syllable,
                                      &difficulty)) {
                /* Fallback to at least not crash */
                strcpy(wordparty->current_syllable, "a");
                difficulty = 0;
        }

        remove_word_timeout(wordparty);

        long timeout = (PCX_WORDPARTY_MIN_WORD_TIMEOUT +
                        (difficulty * (PCX_WORDPARTY_MAX_WORD_TIMEOUT -
                                       PCX_WORDPARTY_MIN_WORD_TIMEOUT) /
                         PCX_SYLLABARY_MAX_DIFFICULTY));

        wordparty->word_timeout =
                pcx_main_context_add_timeout(NULL,
                                             timeout,
                                             word_timeout_cb,
                                             wordparty);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const struct pcx_wordparty_player *player =
                wordparty->players + next_player;

        pcx_buffer_append_string(&buf, "<b>");
        pcx_html_escape(&buf, player->name);
        pcx_buffer_append_string(&buf, "</b> ");

        for (int i = 0; i < player->lives; i++)
                pcx_buffer_append_string(&buf, "❤️");

        pcx_buffer_append_string(&buf, "\n\n");

        escape_string(wordparty, &buf, PCX_TEXT_STRING_TYPE_A_WORD);

        pcx_buffer_append_string(&buf, "\n\n<b>");
        pcx_html_escape(&buf, wordparty->current_syllable);
        pcx_buffer_append_string(&buf, "</b>");

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;
        message.text = (const char *) buf.data;

        wordparty->callbacks.send_message(&message, wordparty->user_data);

        pcx_buffer_destroy(&buf);
}

static char *
get_data_filename(const struct pcx_config *config,
                  const char *name,
                  enum pcx_text_language language)
{
        return pcx_strconcat(config->data_dir,
                             "/",
                             name,
                             "-",
                             pcx_text_get(language,
                                          PCX_TEXT_STRING_LANGUAGE_CODE),
                             ".bin",
                             NULL);
}

static void
open_dictionary(struct pcx_wordparty *wordparty,
                const struct pcx_config *config,
                enum pcx_text_language language)
{
        char *full_filename = get_data_filename(config, "dictionary", language);

        struct pcx_error *error = NULL;

        wordparty->trie = pcx_trie_new(full_filename, &error);

        pcx_free(full_filename);

        if (wordparty->trie == NULL) {
                pcx_log("Error opening wordparty dictionary: %s",
                        error->message);
                pcx_error_free(error);
        }
}

static void
open_syllabary(struct pcx_wordparty *wordparty,
               const struct pcx_config *config,
               enum pcx_text_language language)
{
        char *full_filename = get_data_filename(config, "syllabary", language);

        struct pcx_error *error = NULL;

        wordparty->syllabary = pcx_syllabary_new(full_filename, &error);

        pcx_free(full_filename);

        if (wordparty->trie == NULL) {
                pcx_log("Error opening wordparty syllabary: %s",
                        error->message);
                pcx_error_free(error);
        }
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_WORDPARTY_MAX_PLAYERS);

        struct pcx_wordparty *wordparty = pcx_calloc(sizeof *wordparty);

        wordparty->players = pcx_calloc(n_players *
                                        sizeof (struct pcx_wordparty_player));

        for (unsigned i = 0; i < n_players; i++) {
                wordparty->players[i].name = pcx_strdup(names[i]);
                wordparty->players[i].lives = PCX_WORDPARTY_LIVES;
        }

        wordparty->language = language;
        wordparty->callbacks = *callbacks;
        wordparty->user_data = user_data;

        wordparty->n_players = n_players;

        pcx_buffer_init(&wordparty->word_buf);

        open_dictionary(wordparty, config, language);
        open_syllabary(wordparty, config, language);

        wordparty->current_player = rand() % n_players;

        start_turn(wordparty);

        return wordparty;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup("STUB");
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
}

static bool
is_space_char(char ch)
{
        return strchr("\r\n \t", ch) != NULL;
}

static void
add_unichar(struct pcx_buffer *buf,
            uint32_t ch)
{
        pcx_buffer_ensure_size(buf, buf->length + PCX_UTF8_MAX_CHAR_LENGTH);
        buf->length += pcx_utf8_encode(ch, (char *) buf->data + buf->length);
}

static void
reject_word(struct pcx_wordparty *wordparty)
{
        char *text = pcx_strconcat("👎 ",
                                   (const char *) wordparty->word_buf.data,
                                   NULL);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_PLAIN;
        message.text = text;

        wordparty->callbacks.send_message(&message, wordparty->user_data);

        pcx_free(text);
}

static bool
is_valid_word(struct pcx_wordparty *wordparty)
{
        const char *word = (const char *) wordparty->word_buf.data;

        /* The word must contain the syllable */
        if (!strstr(word, wordparty->current_syllable))
                return false;

        /* The word must be in the dictionary */
        if (wordparty->trie == NULL ||
            !pcx_trie_contains_word(wordparty->trie, word))
                return false;

        return true;
}

static bool
extract_word_from_message(struct pcx_wordparty *wordparty,
                          const char *text)
{
        pcx_buffer_set_length(&wordparty->word_buf, 0);

        struct pcx_hat_iter iter;

        size_t length = strlen(text);

        while (length > 0 && is_space_char(text[length - 1]))
                length--;

        pcx_hat_iter_init(&iter, text, length);

        while (!pcx_hat_iter_finished(&iter)) {
                uint32_t ch = pcx_hat_iter_next(&iter);

                if (!pcx_hat_is_alphabetic(ch))
                        return false;

                add_unichar(&wordparty->word_buf, pcx_hat_to_lower(ch));
        }

        pcx_buffer_append_c(&wordparty->word_buf, '\0');

        return wordparty->word_buf.length > 1;
}

static void
handle_message_cb(void *data,
                  int player_num,
                  const char *text)
{
        struct pcx_wordparty *wordparty = data;

        if (player_num != wordparty->current_player)
                return;

        if (!extract_word_from_message(wordparty, text))
                return;

        if (is_valid_word(wordparty))
                start_turn(wordparty);
        else
                reject_word(wordparty);
}

static void
free_game_cb(void *data)
{
        struct pcx_wordparty *wordparty = data;

        for (int i = 0; i < wordparty->n_players; i++)
                pcx_free(wordparty->players[i].name);

        pcx_free(wordparty->players);

        remove_word_timeout(wordparty);

        if (wordparty->game_over_source)
                pcx_main_context_remove_source(wordparty->game_over_source);

        if (wordparty->trie)
                pcx_trie_free(wordparty->trie);

        if (wordparty->syllabary)
                pcx_syllabary_free(wordparty->syllabary);

        pcx_buffer_destroy(&wordparty->word_buf);

        pcx_free(wordparty);
}

const struct pcx_game
pcx_wordparty_game = {
        .name = "wordparty",
        .name_string = PCX_TEXT_STRING_NAME_WORDPARTY,
        .start_command = PCX_TEXT_STRING_WORDPARTY_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_WORDPARTY_START_COMMAND_DESCRIPTION,
        .min_players = PCX_WORDPARTY_MIN_PLAYERS,
        .max_players = PCX_WORDPARTY_MAX_PLAYERS,
        .needs_private_messages = false,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .handle_message_cb = handle_message_cb,
        .free_game_cb = free_game_cb
};
