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
#include "pcx-wordparty-help.h"
#include "pcx-proto.h"

#define PCX_WORDPARTY_MIN_PLAYERS 2
#define PCX_WORDPARTY_MAX_PLAYERS 32

#define PCX_WORDPARTY_LIVES 2
#define PCX_WORDPARTY_MAX_LIVES 3

/* If the player needs to use at most this many letters to regain a
 * life we will show them a hint about which letters need to be used.
 */
#define PCX_WORDPARTY_MAX_LETTERS_FOR_HINT 8

/* The timeout that a player needs will be in this range, depending on
 * the difficulty.
 */
#define PCX_WORDPARTY_MIN_WORD_TIMEOUT (5 * 1000)
#define PCX_WORDPARTY_MAX_WORD_TIMEOUT (60 * 1000)

struct pcx_wordparty_player {
        char *name;
        int lives;
        /* A mask of letters that this player has used */
        uint32_t letters_used;
};

struct pcx_wordparty_class_data {
        struct pcx_trie *trie;
        struct pcx_syllabary *syllabary;
};

struct pcx_wordparty {
        int n_players;

        struct pcx_wordparty_player *players;
        struct pcx_game_callbacks callbacks;
        void *user_data;

        enum pcx_text_language language;
        struct pcx_main_context_source *game_over_source;

        struct pcx_wordparty_class_data *class_data;

        struct pcx_buffer word_buf;

        int current_player;

        /* The number times the current syllable has failed. Once each
         * player has seen it then we’ll pick a new one.
         */
        int fail_count;
        /* The number of players still in the game when the current
         * syllable was chosen. This is used to detect when everyone
         * has had a go at it.
         */
        int max_fail_count;

        /* Array of word tokens returned from the trie so that we can
         * detect duplicate words.
         */
        struct pcx_buffer used_words;

        struct pcx_main_context_source *word_timeout;

        char current_syllable[PCX_SYLLABARY_MAX_SYLLABLE_LENGTH + 1];
        int current_difficulty;

        /* The alphabet for the current language expanded into unicode
         * codepoints and sorted.
         */
        uint32_t *letters;
        int n_letters;
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
add_unichar(struct pcx_buffer *buf,
            uint32_t ch)
{
        pcx_buffer_ensure_size(buf, buf->length + PCX_UTF8_MAX_CHAR_LENGTH);
        buf->length += pcx_utf8_encode(ch, (char *) buf->data + buf->length);
}

static void
add_uppercase_string(struct pcx_buffer *buf,
                     const char *str)
{
        for (const char *p = str; *p; p = pcx_utf8_next(p))
                add_unichar(buf, pcx_hat_to_upper(pcx_utf8_get_char(p)));
        pcx_buffer_append_c(buf, '\0');
        buf->length--;
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
pick_syllable(struct pcx_wordparty *wordparty)
{
        wordparty->fail_count = 0;
        wordparty->max_fail_count = count_players(wordparty);

        if (wordparty->class_data->syllabary == NULL ||
            !pcx_syllabary_get_random(wordparty->class_data->syllabary,
                                      wordparty->current_syllable,
                                      &wordparty->current_difficulty)) {
                /* Fallback to at least not crash */
                strcpy(wordparty->current_syllable, "a");
                wordparty->current_difficulty = 0;
        }
}

static void
set_lives(struct pcx_wordparty *wordparty,
          int player_num,
          int lives)
{
        wordparty->players[player_num].lives = lives;

        wordparty->callbacks.dirty_sideband_data(player_num + 1,
                                                 wordparty->user_data);
}

static void
set_current_player(struct pcx_wordparty *wordparty,
                   int player_num)
{
        wordparty->current_player = player_num;
        wordparty->callbacks.dirty_sideband_data(0, wordparty->user_data);
}

static void
word_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct pcx_wordparty *wordparty = user_data;

        wordparty->word_timeout = NULL;

        struct pcx_wordparty_player *player =
                wordparty->players + wordparty->current_player;

        if (++wordparty->fail_count >= wordparty->max_fail_count)
                pick_syllable(wordparty);

        /* If the player had the maximum number of lives then reset
         * the letter tally so that they can start trying to gain the
         * life back.
         */
        if (player->lives == PCX_WORDPARTY_MAX_LIVES)
                player->letters_used = 0;

        set_lives(wordparty, wordparty->current_player, player->lives - 1);

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
get_n_used_words(struct pcx_wordparty *wordparty)
{
        return wordparty->used_words.length / sizeof (uint32_t);
}

static void
maybe_add_letters_hint(struct pcx_wordparty *wordparty,
                       const struct pcx_wordparty_player *player,
                       struct pcx_buffer *buf)
{
        if (player->lives >= PCX_WORDPARTY_MAX_LIVES)
                return;

        int n_letters_remaining = 0;

        for (int i = 0; i < wordparty->n_letters; i++) {
                if ((player->letters_used & (UINT32_C(1) << i)) == 0)
                        n_letters_remaining++;
        }

        if (n_letters_remaining > PCX_WORDPARTY_MAX_LETTERS_FOR_HINT)
                return;

        escape_string(wordparty, buf, PCX_TEXT_STRING_LETTERS_HINT);
        pcx_buffer_append_c(buf, ' ');

        for (int i = 0; i < wordparty->n_letters; i++) {
                if ((player->letters_used & (UINT32_C(1) << i)))
                        continue;

                add_unichar(buf, pcx_hat_to_upper(wordparty->letters[i]));
        }

        pcx_buffer_append_string(buf, "\n\n");
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

        set_current_player(wordparty, next_player);

        if (count_players(wordparty) <= 1) {
                end_game(wordparty);
                return;
        }

        /* Make the timeouts gradually get shorter as the game progresses */
        int difficulty = (wordparty->current_difficulty -
                          get_n_used_words(wordparty) / wordparty->n_players);
        if (difficulty < 0)
                difficulty = 0;

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

        maybe_add_letters_hint(wordparty, player, &buf);

        escape_string(wordparty, &buf, PCX_TEXT_STRING_TYPE_A_WORD);

        pcx_buffer_append_string(&buf, "\n\n<b>");
        add_uppercase_string(&buf, wordparty->current_syllable);

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
open_dictionary(struct pcx_wordparty_class_data *data,
                const struct pcx_config *config,
                enum pcx_text_language language)
{
        char *full_filename = get_data_filename(config, "dictionary", language);

        struct pcx_error *error = NULL;

        data->trie = pcx_trie_new(full_filename, &error);

        pcx_free(full_filename);

        if (data->trie == NULL) {
                pcx_log("Error opening wordparty dictionary: %s",
                        error->message);
                pcx_error_free(error);
        }
}

static void
open_syllabary(struct pcx_wordparty_class_data *data,
               const struct pcx_config *config,
               enum pcx_text_language language)
{
        char *full_filename = get_data_filename(config, "syllabary", language);

        struct pcx_error *error = NULL;

        data->syllabary = pcx_syllabary_new(full_filename, &error);

        pcx_free(full_filename);

        if (data->trie == NULL) {
                pcx_log("Error opening wordparty syllabary: %s",
                        error->message);
                pcx_error_free(error);
        }
}

static void *
create_class_store_data_cb(const struct pcx_config *config,
                           enum pcx_text_language language)
{
        struct pcx_wordparty_class_data *data = pcx_calloc(sizeof *data);

        open_dictionary(data, config, language);
        open_syllabary(data, config, language);

        return data;
}

static void
free_class_store_data_cb(void *user_data)
{
        struct pcx_wordparty_class_data *data = user_data;

        if (data->trie)
                pcx_trie_free(data->trie);

        if (data->syllabary)
                pcx_syllabary_free(data->syllabary);

        pcx_free(data);
}

static const struct pcx_class_store_callbacks
class_store_callbacks = {
        .create_data = create_class_store_data_cb,
        .free_data = free_class_store_data_cb,
};

static int
compare_uint32(const void *pa, const void *pb)
{
        uint32_t a = *(const uint32_t *) pa;
        uint32_t b = *(const uint32_t *) pb;

        if (a > b)
                return 1;
        else if (a < b)
                return -1;
        else
                return 0;
}

static void
create_alphabet(struct pcx_wordparty *wordparty)
{
        const char *alphabet = pcx_text_get(wordparty->language,
                                            PCX_TEXT_STRING_ALPHABET);
        int n_letters = 0;

        for (const char *p = alphabet; *p; p = pcx_utf8_next(p))
                n_letters++;

        assert(n_letters > 0 && n_letters < sizeof (uint32_t) * 8);

        wordparty->letters = pcx_alloc(sizeof (uint32_t) * n_letters);
        wordparty->n_letters = n_letters;

        int i = 0;

        for (const char *p = alphabet; *p; p = pcx_utf8_next(p), i++)
                wordparty->letters[i] = pcx_utf8_get_char(p);

        qsort(wordparty->letters,
              n_letters,
              sizeof (uint32_t),
              compare_uint32);
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

        wordparty->language = language;
        wordparty->callbacks = *callbacks;
        wordparty->user_data = user_data;

        wordparty->n_players = n_players;

        wordparty->players = pcx_calloc(n_players *
                                        sizeof (struct pcx_wordparty_player));

        for (unsigned i = 0; i < n_players; i++) {
                wordparty->players[i].name = pcx_strdup(names[i]);
                set_lives(wordparty, i, PCX_WORDPARTY_LIVES);
        }

        pcx_buffer_init(&wordparty->word_buf);
        pcx_buffer_init(&wordparty->used_words);

        wordparty->class_data =
                pcx_class_store_ref_data(callbacks->get_class_store(user_data),
                                         config,
                                         &pcx_wordparty_game,
                                         language,
                                         &class_store_callbacks);

        set_current_player(wordparty, rand() % n_players);

        create_alphabet(wordparty);

        pick_syllable(wordparty);
        start_turn(wordparty);

        return wordparty;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_wordparty_help[language]);
}

static int
write_sideband_current_player(struct pcx_wordparty *wordparty,
                              uint8_t *buffer,
                              size_t buffer_length)
{
        return pcx_proto_write_command(buffer,
                                       buffer_length,
                                       PCX_PROTO_SIDEBAND,

                                       /* data_num */
                                       PCX_PROTO_TYPE_UINT8,
                                       0,

                                       PCX_PROTO_TYPE_UINT8,
                                       wordparty->current_player,

                                       PCX_PROTO_TYPE_NONE);
}

static int
write_sideband_lives(struct pcx_wordparty *wordparty,
                     int player_num,
                     uint8_t *buffer,
                     size_t buffer_length)
{
        const struct pcx_wordparty_player *player =
                wordparty->players + player_num;

        return pcx_proto_write_command(buffer,
                                       buffer_length,
                                       PCX_PROTO_SIDEBAND,

                                       /* data_num */
                                       PCX_PROTO_TYPE_UINT8,
                                       player_num + 1,

                                       PCX_PROTO_TYPE_UINT8,
                                       player->lives,

                                       PCX_PROTO_TYPE_NONE);
}

static int
write_sideband_data_cb(void *user_data,
                       int data_num,
                       uint8_t *buffer,
                       size_t buffer_length)
{
        struct pcx_wordparty *wordparty = user_data;

        if (data_num == 0) {
                return write_sideband_current_player(wordparty,
                                                     buffer,
                                                     buffer_length);
        }

        int player_num = data_num - 1;

        return write_sideband_lives(wordparty,
                                    player_num,
                                    buffer,
                                    buffer_length);
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
reject_word(struct pcx_wordparty *wordparty,
            const char *emoji)
{
        char *text = pcx_strconcat(emoji,
                                   " ",
                                   (const char *) wordparty->word_buf.data,
                                   NULL);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.format = PCX_GAME_MESSAGE_FORMAT_PLAIN;
        message.text = text;

        wordparty->callbacks.send_message(&message, wordparty->user_data);

        pcx_free(text);
}

static bool
is_valid_word(struct pcx_wordparty *wordparty,
              uint32_t *token)
{
        const char *word = (const char *) wordparty->word_buf.data;

        /* The word must contain the syllable */
        if (!strstr(word, wordparty->current_syllable))
                return false;

        /* The word must be in the dictionary */
        if (wordparty->class_data->trie == NULL ||
            !pcx_trie_contains_word(wordparty->class_data->trie, word, token))
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

static bool
is_word_used(struct pcx_wordparty *wordparty,
             uint32_t token)
{
        int n_used_words = get_n_used_words(wordparty);
        const uint32_t *words = (const uint32_t *) wordparty->used_words.data;

        for (int i = 0; i < n_used_words; i++) {
                if (words[i] == token)
                        return true;
        }

        return false;
}

static int
find_letter(struct pcx_wordparty *wordparty,
            uint32_t ch)
{
        int min = 0, max = wordparty->n_letters;

        while (min < max) {
                int mid = (min + max) / 2;
                uint32_t v = wordparty->letters[mid];

                if (ch < v)
                        max = mid;
                else if (ch > v)
                        min = mid + 1;
                else
                        return mid;
        }

        return -1;
}

static void
tally_word(struct pcx_wordparty *wordparty)
{
        struct pcx_wordparty_player *player =
                wordparty->players + wordparty->current_player;

        if (player->lives >= PCX_WORDPARTY_MAX_LIVES)
                return;

        for (const char *p = (const char *) wordparty->word_buf.data;
             *p;
             p = pcx_utf8_next(p)) {
                int letter = find_letter(wordparty, pcx_utf8_get_char(p));

                if (letter == -1)
                        continue;

                player->letters_used |= UINT32_C(1) << letter;
        }

        uint32_t all_letters =
                UINT32_MAX >> (sizeof (uint32_t) * 8 - wordparty->n_letters);

        if (player->letters_used == all_letters) {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_string(&buf, "➕❤️ ");
                pcx_buffer_append_printf(&buf,
                                         pcx_text_get(wordparty->language,
                                                      PCX_TEXT_STRING_ONE_UP),
                                         player->name);

                struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

                message.format = PCX_GAME_MESSAGE_FORMAT_PLAIN;
                message.text = (const char *) buf.data;

                wordparty->callbacks.send_message(&message,
                                                  wordparty->user_data);

                pcx_buffer_destroy(&buf);

                set_lives(wordparty,
                          wordparty->current_player,
                          player->lives + 1);
                player->letters_used = 0;
        }
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

        uint32_t token;

        if (!is_valid_word(wordparty, &token)) {
                reject_word(wordparty, "👎️");
        } else if (is_word_used(wordparty, token)) {
                reject_word(wordparty, "♻️");
        } else {
                tally_word(wordparty);
                pcx_buffer_append(&wordparty->used_words, &token, sizeof token);
                pick_syllable(wordparty);
                start_turn(wordparty);
        }
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

        struct pcx_class_store *class_store =
                wordparty->callbacks.get_class_store(wordparty->user_data);
        pcx_class_store_unref_data(class_store, wordparty->class_data);

        pcx_buffer_destroy(&wordparty->word_buf);
        pcx_buffer_destroy(&wordparty->used_words);

        pcx_free(wordparty->letters);

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
        .write_sideband_data_cb = write_sideband_data_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .handle_message_cb = handle_message_cb,
        .free_game_cb = free_game_cb
};
