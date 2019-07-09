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
#include "pcx-html.h"
#include "pcx-love-help.h"

#define PCX_LOVE_MIN_PLAYERS 2
#define PCX_LOVE_MAX_PLAYERS 4

#define PCX_LOVE_N_CARDS 16

/* Number of cards that are immediately visible in a two-player game */
#define PCX_LOVE_N_VISIBLE_CARDS 3

struct pcx_love_character {
        enum pcx_text_string name;
        const char *symbol;
        enum pcx_text_string description;
        int value;
        int count;
        const char *keyword;
};

static const struct pcx_love_character
guard_character = {
        .name = PCX_TEXT_STRING_GUARD,
        .symbol = "👮️",
        .description = PCX_TEXT_STRING_GUARD_DESCRIPTION,
        .value = 1,
        .count = 5,
        .keyword = "guard",
};

static const struct pcx_love_character
spy_character = {
        .name = PCX_TEXT_STRING_SPY,
        .symbol = "🔎",
        .description = PCX_TEXT_STRING_SPY_DESCRIPTION,
        .value = 2,
        .count = 2,
        .keyword = "spy",
};

static const struct pcx_love_character
baron_character = {
        .name = PCX_TEXT_STRING_BARON,
        .symbol = "⚔️",
        .description = PCX_TEXT_STRING_BARON_DESCRIPTION,
        .value = 3,
        .count = 2,
        .keyword = "baron",
};

static const struct pcx_love_character
handmaid_character = {
        .name = PCX_TEXT_STRING_HANDMAID,
        .symbol = "💅",
        .description = PCX_TEXT_STRING_HANDMAID_DESCRIPTION,
        .value = 4,
        .count = 2,
        .keyword = "handmaid",
};

static const struct pcx_love_character
prince_character = {
        .name = PCX_TEXT_STRING_PRINCE,
        .symbol = "🤴",
        .description = PCX_TEXT_STRING_PRINCE_DESCRIPTION,
        .value = 5,
        .count = 2,
        .keyword = "prince",
};

static const struct pcx_love_character
king_character = {
        .name = PCX_TEXT_STRING_KING,
        .symbol = "👑",
        .description = PCX_TEXT_STRING_KING_DESCRIPTION,
        .value = 6,
        .count = 1,
        .keyword = "king",
};

static const struct pcx_love_character
comtesse_character = {
        .name = PCX_TEXT_STRING_COMTESSE,
        .symbol = "👩‍💼",
        .description = PCX_TEXT_STRING_COMTESSE_DESCRIPTION,
        .value = 7,
        .count = 1,
        .keyword = "comtesse",
};

static const struct pcx_love_character
princess_character = {
        .name = PCX_TEXT_STRING_PRINCESS,
        .symbol = "👸",
        .description = PCX_TEXT_STRING_PRINCESS_DESCRIPTION,
        .value = 8,
        .count = 1,
        .keyword = "princess",
};

static const struct pcx_love_character * const
characters[] = {
        &guard_character,
        &spy_character,
        &baron_character,
        &handmaid_character,
        &prince_character,
        &king_character,
        &comtesse_character,
        &princess_character,
};

struct pcx_love_player {
        char *name;
        const struct pcx_love_character *card;
        const struct pcx_love_character *discarded_cards[PCX_LOVE_N_CARDS / 2];
        size_t n_discarded_cards;
        int hearts;
        bool is_alive;
        bool is_protected;
};

struct pcx_love {
        int n_players;
        int current_player;
        struct pcx_love_player players[PCX_LOVE_MAX_PLAYERS];
        struct pcx_game_callbacks callbacks;
        void *user_data;
        enum pcx_text_language language;
        const struct pcx_love_character *deck[PCX_LOVE_N_CARDS];
        size_t n_cards;
        const struct pcx_love_character *pending_card;
        const struct pcx_love_character *set_aside_card;
        const struct pcx_love_character *
        visible_cards[PCX_LOVE_N_VISIBLE_CARDS];
};

static void
get_value_symbol(const struct pcx_love_character *character,
                 struct pcx_buffer *buf)
{
        pcx_buffer_append_printf(buf,
                                 "%i\xef\xb8\x8f\xe2\x83\xa3",
                                 character->value);
}

static void
get_long_name(enum pcx_text_language language,
              const struct pcx_love_character *character,
              struct pcx_buffer *buf)
{
        pcx_html_escape(buf, pcx_text_get(language, character->name));
        pcx_buffer_append_c(buf, ' ');
        pcx_buffer_append_string(buf, character->symbol);
        get_value_symbol(character, buf);
}

static void
shuffle_deck(struct pcx_love *love)
{
        if (love->n_cards < 2)
                return;

        for (unsigned i = love->n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                const struct pcx_love_character *t = love->deck[j];
                love->deck[j] = love->deck[i];
                love->deck[i] = t;
        }
}

static const struct pcx_love_character *
take_card(struct pcx_love *love)
{
        assert(love->n_cards > 1);
        return love->deck[--love->n_cards];
}

static void
show_card(struct pcx_love *love,
          int player_num)
{
        const struct pcx_love_player *player = love->players + player_num;
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        const char *note = pcx_text_get(love->language,
                                        PCX_TEXT_STRING_YOUR_CARD_IS);
        pcx_html_escape(&buf, note);
        get_long_name(love->language, player->card, &buf);

        love->callbacks.send_private_message(player_num,
                                             PCX_GAME_MESSAGE_FORMAT_HTML,
                                             (const char *) buf.data,
                                             0, /* n_buttons */
                                             NULL, /* buttons */
                                             love->user_data);

        pcx_buffer_destroy(&buf);
}

static void
start_round(struct pcx_love *love)
{
        love->pending_card = NULL;

        love->n_cards = 0;

        for (unsigned ch = 0; ch < PCX_N_ELEMENTS(characters); ch++) {
                const struct pcx_love_character *character = characters[ch];

                for (int i = 0; i < character->count; i++)
                        love->deck[love->n_cards++] = character;

                assert(ch == 0 ||
                       character->value == characters[ch - 1]->value + 1);
        }

        assert(love->n_cards == PCX_LOVE_N_CARDS);

        shuffle_deck(love);

        love->set_aside_card = take_card(love);

        if (love->n_players == 2) {
                for (unsigned i = 0; i < PCX_LOVE_N_VISIBLE_CARDS; i++)
                        love->visible_cards[i] = take_card(love);
        }

        for (unsigned i = 0; i < love->n_players; i++) {
                struct pcx_love_player *player = love->players + i;
                player->card = take_card(love);
                player->is_alive = true;
                player->is_protected = false;
                player->n_discarded_cards = 0;
                show_card(love, i);
        }
}

static void *
create_game_cb(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_LOVE_MAX_PLAYERS);

        struct pcx_love *love = pcx_calloc(sizeof *love);

        for (unsigned i = 0; i < n_players; i++)
                love->players[i].name = pcx_strdup(names[i]);

        love->language = language;
        love->callbacks = *callbacks;
        love->user_data = user_data;

        love->n_players = n_players;
        love->current_player = rand() % n_players;

        start_round(love);

        return love;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        const char *text = pcx_love_help[language];
        const char *text_end = strstr(text, "@CARDS@");

        assert(text_end);

        pcx_buffer_append(&buf, text, text_end - text);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(characters); i++) {
                if (i > 0)
                        pcx_buffer_append_string(&buf, "\n\n");

                pcx_buffer_append_string(&buf, "<b>");
                get_long_name(language, characters[i], &buf);
                pcx_buffer_append_string(&buf, "</b> ");

                if (characters[i]->count == 1) {
                        const char *copy =
                                pcx_text_get(language,
                                             PCX_TEXT_STRING_ONE_COPY);
                        pcx_buffer_append_string(&buf, copy);
                } else {
                        const char *copy =
                                pcx_text_get(language,
                                             PCX_TEXT_STRING_PLURAL_COPIES);
                        pcx_buffer_append_printf(&buf,
                                                 copy,
                                                 characters[i]->count);
                }

                pcx_buffer_append_c(&buf, '\n');

                const char *desc = pcx_text_get(language,
                                                characters[i]->description);
                pcx_html_escape(&buf, desc);
        }

        pcx_buffer_append_string(&buf, text_end + 7);

        return (char *) buf.data;
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

        for (int i = 0; i < love->n_players; i++)
                pcx_free(love->players[i].name);

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
