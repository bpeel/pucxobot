/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

#include "pcx-six.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-six-help.h"

#define PCX_SIX_MIN_PLAYERS 2
#define PCX_SIX_MAX_PLAYERS 10

#define PCX_SIX_N_CARDS 104
#define PCX_SIX_HAND_SIZE 10
#define PCX_SIX_N_ROWS 4
#define PCX_SIX_ROW_SIZE 5
#define PCX_SIX_END_POINTS 66

#define PCX_SIX_BULL_HEAD "🐮"

/* Time in milliseconds to wait before showing each card placement */
#define PCX_SIX_PLACEMENT_TIME 2000

typedef uint8_t pcx_six_card_t;

struct pcx_six_player {
        char *name;
        int score;
        int score_this_round;
        int chosen_card;
        pcx_six_card_t hand[PCX_SIX_HAND_SIZE];
};

struct pcx_six_row {
        int n_cards;
        pcx_six_card_t cards[PCX_SIX_ROW_SIZE];
};

struct pcx_six {
        int n_players;

        /* Number of cards in each player’s hand */
        int n_cards;
        /* Mask with a bit for each player to mark if they have chosen
         * a card.
         */
        uint32_t card_chosen_mask;
        /* Order of players to reveal cards in */
        struct pcx_six_player *reveal_order[PCX_SIX_MAX_PLAYERS];
        /* Next player to place their card */
        int next_placement;
        /* Timer to add a bit of delay between placing cards */
        struct pcx_main_context_source *placement_timer_source;
        /* If not -1, the player that is currently choosing a row to take */
        int player_choosing_row;

        struct pcx_six_player players[PCX_SIX_MAX_PLAYERS];
        struct pcx_six_row rows[PCX_SIX_N_ROWS];
        struct pcx_game_callbacks callbacks;
        void *user_data;
        enum pcx_text_language language;
        struct pcx_main_context_source *game_over_source;
};

static uint8_t
card_bull_heads[] = {
        /*   1 */ 1, 1, 1, 1, 2, 1, 1, 1, 1, 3,
        /*  11 */ 5, 1, 1, 1, 2, 1, 1, 1, 1, 3,
        /*  21 */ 1, 5, 1, 1, 2, 1, 1, 1, 1, 3,
        /*  31 */ 1, 1, 5, 1, 2, 1, 1, 1, 1, 3,
        /*  41 */ 1, 1, 1, 5, 2, 1, 1, 1, 1, 3,
        /*  51 */ 1, 1, 1, 1, 7, 1, 1, 1, 1, 3,
        /*  61 */ 1, 1, 1, 1, 2, 5, 1, 1, 1, 3,
        /*  71 */ 1, 1, 1, 1, 2, 1, 5, 1, 1, 3,
        /*  81 */ 1, 1, 1, 1, 2, 1, 1, 5, 1, 3,
        /*  91 */ 1, 1, 1, 1, 2, 1, 1, 1, 5, 3,
        /* 101 */ 1, 1, 1, 1
};

_Static_assert(PCX_N_ELEMENTS(card_bull_heads) == PCX_SIX_N_CARDS);

static void
start_placement_timer(struct pcx_six *six);

static int
compare_card_cb(const void *a, const void *b)
{
        return ((int) (*(pcx_six_card_t *) a) -
                (int) (*(pcx_six_card_t *) b));
}

static void
deal(struct pcx_six *six)
{
        pcx_six_card_t deck[PCX_SIX_N_CARDS];

        for (unsigned i = 0; i < PCX_SIX_N_CARDS; i++)
                deck[i] = i;

        for (unsigned i = PCX_SIX_N_CARDS - 1; i > 0; i--) {
                unsigned j = rand() % (i + 1);
                pcx_six_card_t t = deck[j];
                deck[j] = deck[i];
                deck[i] = t;
        }

        pcx_six_card_t *p = deck;

        for (int player = 0; player < six->n_players; player++) {
                pcx_six_card_t *hand = six->players[player].hand;

                for (int card = 0; card < PCX_SIX_HAND_SIZE; card++)
                        hand[card] = *(p++);

                qsort(hand,
                      PCX_SIX_HAND_SIZE,
                      sizeof (pcx_six_card_t),
                      compare_card_cb);
        }

        for (int row = 0; row < PCX_SIX_N_ROWS; row++) {
                six->rows[row].cards[0] = *(p++);
                six->rows[row].n_cards = 1;
        }

        six->n_cards = PCX_SIX_HAND_SIZE;
}

static void
add_card(struct pcx_buffer *buf,
         pcx_six_card_t card)
{
        pcx_buffer_append_printf(buf, "%i ", (int) card + 1);

        int n_bull_heads = card_bull_heads[card];

        for (int i = 0; i < n_bull_heads; i++)
                pcx_buffer_append_string(buf, PCX_SIX_BULL_HEAD);
}

static void
show_card_question(struct pcx_six *six,
                   const struct pcx_six_player *player)
{
        struct pcx_game_button buttons[PCX_SIX_HAND_SIZE];

        for (int i = 0; i < six->n_cards; i++) {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                add_card(&buf, player->hand[i]);

                buttons[i].text = (char *) buf.data;

                pcx_buffer_init(&buf);

                pcx_buffer_append_printf(&buf, "play:%i", i);

                buttons[i].data = (char *) buf.data;
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = pcx_text_get(six->language,
                                    PCX_TEXT_STRING_WHICH_CARD_TO_PLAY);
        message.n_buttons = six->n_cards;
        message.buttons = buttons;
        message.target = player - six->players;

        six->callbacks.send_message(&message, six->user_data);

        for (int i = 0; i < six->n_cards; i++) {
                pcx_free((char *) buttons[i].data);
                pcx_free((char *) buttons[i].text);
        }
}

static void
show_card_questions(struct pcx_six *six)
{
        for (int i = 0; i < six->n_players; i++)
                show_card_question(six, six->players + i);
}

static int
get_row_score(const struct pcx_six_row *row)
{
        int bull_heads = 0;

        for (int i = 0; i < row->n_cards; i++)
                bull_heads += card_bull_heads[row->cards[i]];

        return bull_heads;
}

static void
add_row(struct pcx_six *six,
        struct pcx_buffer *buf,
        int row_num)
{
        const struct pcx_six_row *row = six->rows + row_num;

        pcx_buffer_append_printf(buf, "%c) ", row_num + 'A');

        for (int i = 0; i < row->n_cards; i++) {
                pcx_six_card_t card = row->cards[i];

                pcx_buffer_append_printf(buf, "%i ", card + 1);

                int counter = card + 1;

                /* Add U+2007 FIGURE SPACEs if the number is less than
                 * 3 digits to make them all align.
                 */
                while (counter < 100) {
                        pcx_buffer_append_string(buf, "\xe2\x80\x87");
                        counter *= 10;
                }
        }

        /* Pad so that the bullhead count aligns */
        for (int i = row->n_cards; i < PCX_SIX_ROW_SIZE; i++) {
                pcx_buffer_append_string(buf,
                                         "\xe2\x80\x87"
                                         "\xe2\x80\x87"
                                         "\xe2\x80\x87 ");
        }

        pcx_buffer_append_printf(buf,
                                 "(%i × " PCX_SIX_BULL_HEAD ")\n",
                                 get_row_score(row));
}

static void
show_rows(struct pcx_six *six)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (int i = 0; i < PCX_SIX_N_ROWS; i++)
                add_row(six, &buf, i);

        pcx_buffer_append_c(&buf, '\n');

        const char *note = pcx_text_get(six->language,
                                        PCX_TEXT_STRING_EVERYBODY_CHOOSE_CARD);

        pcx_buffer_append_string(&buf, note);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        six->callbacks.send_message(&message, six->user_data);

        pcx_buffer_destroy(&buf);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_six *six = user_data;
        six->game_over_source = NULL;
        six->callbacks.game_over(six->user_data);
}

static void
end_game(struct pcx_six *six)
{
        if (six->game_over_source == NULL) {
                six->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     six);
        }
}

static int
compare_score_cb(const void *a,
                 const void *b)
{
        struct pcx_six_player * const *pa = a;
        struct pcx_six_player * const *pb = b;

        return (*pa)->score - (*pb)->score;
}

static void
show_round_end(struct pcx_six *six,
               bool *game_is_over)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(six->language,
                                              PCX_TEXT_STRING_ROUND_OVER));

        pcx_buffer_append_string(&buf, "\n\n");

        struct pcx_six_player *score_order[PCX_SIX_MAX_PLAYERS];

        for (int i = 0; i < six->n_players; i++) {
                struct pcx_six_player *player = six->players + i;
                score_order[i] = player;
                player->score += player->score_this_round;
        }

        qsort(score_order,
              six->n_players,
              sizeof score_order[0],
              compare_score_cb);

        const struct pcx_six_player *end_player = NULL;

        for (int i = 0; i < six->n_players; i++) {
                struct pcx_six_player *player = score_order[i];

                pcx_buffer_append_printf(&buf,
                                         "%s: %i (+ %i)\n",
                                         player->name,
                                         player->score,
                                         player->score_this_round);

                player->score_this_round = 0;

                if (player->score >= PCX_SIX_END_POINTS)
                        end_player = player;
        }

        if (end_player) {
                pcx_buffer_append_string(&buf, "\n");
                const char *note = pcx_text_get(six->language,
                                                PCX_TEXT_STRING_END_POINTS);
                pcx_buffer_append_printf(&buf,
                                         note,
                                         end_player->name,
                                         PCX_SIX_END_POINTS);
                pcx_buffer_append_string(&buf, "\n\n");
                note = pcx_text_get(six->language,
                                    PCX_TEXT_STRING_WINS_PLAIN);
                pcx_buffer_append_printf(&buf,
                                         note,
                                         score_order[0]->name);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        six->callbacks.send_message(&message, six->user_data);

        pcx_buffer_destroy(&buf);

        *game_is_over = end_player != NULL;
}

static void
start_round(struct pcx_six *six)
{
        six->card_chosen_mask = 0;

        if (six->n_cards <= 0) {
                bool game_is_over;

                show_round_end(six, &game_is_over);

                if (game_is_over) {
                        end_game(six);
                        return;
                }

                deal(six);
        }

        show_rows(six);
        show_card_questions(six);
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_SIX_MAX_PLAYERS);

        struct pcx_six *six = pcx_calloc(sizeof *six);

        for (unsigned i = 0; i < n_players; i++)
                six->players[i].name = pcx_strdup(names[i]);

        six->language = language;
        six->callbacks = *callbacks;
        six->user_data = user_data;

        six->n_players = n_players;
        six->player_choosing_row = -1;

        deal(six);

        start_round(six);

        return six;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_six_help[language]);
}

static bool
is_button(const char *callback_data,
          const char *colon,
          const char *button)
{
        int len = strlen(button);

        return (colon - callback_data == len &&
                !memcmp(button, callback_data, len));
}

static void
show_chosen_card(struct pcx_six *six,
                 int player_num)
{
        const struct pcx_six_player *player = six->players + player_num;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(six->language,
                                              PCX_TEXT_STRING_CARD_CHOSEN));

        pcx_buffer_append_c(&buf, '\n');

        add_card(&buf, player->hand[player->chosen_card]);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.target = player_num;

        six->callbacks.send_message(&message, six->user_data);

        pcx_buffer_destroy(&buf);
}

static void
place_card(struct pcx_six *six,
           struct pcx_six_player *player,
           int row_num)
{
        pcx_six_card_t card = player->hand[player->chosen_card];

        memmove(player->hand + player->chosen_card,
                player->hand + player->chosen_card + 1,
                (six->n_cards - player->chosen_card - 1) *
                sizeof player->hand[0]);

        assert(row_num >= 0 && row_num < PCX_SIX_N_ROWS);

        struct pcx_six_row *row = six->rows + row_num;

        assert(row->n_cards < PCX_SIX_ROW_SIZE);

        row->cards[row->n_cards++] = card;
}

static void
show_choose_row(struct pcx_six *six,
                struct pcx_six_player *player)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(six->language,
                                              PCX_TEXT_STRING_CHOOSE_ROW),
                                 player->name);

        struct pcx_game_button buttons[PCX_SIX_N_ROWS];

        for (int i = 0; i < PCX_SIX_N_ROWS; i++) {
                struct pcx_buffer button_buf = PCX_BUFFER_STATIC_INIT;

                add_row(six, &button_buf, i);

                buttons[i].text = (char *) button_buf.data;

                pcx_buffer_init(&button_buf);

                pcx_buffer_append_printf(&button_buf, "choose_row:%i", i);

                buttons[i].data = (char *) button_buf.data;
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.n_buttons = PCX_SIX_N_ROWS;
        message.buttons = buttons;
        message.button_players = UINT32_C(1) << (player - six->players);

        six->callbacks.send_message(&message, six->user_data);

        for (int i = 0; i < PCX_SIX_N_ROWS; i++) {
                pcx_free((char *) buttons[i].text);
                pcx_free((char *) buttons[i].data);
        }

        pcx_buffer_destroy(&buf);

        six->player_choosing_row = player - six->players;
}

static int
get_row_top(const struct pcx_six_row *row)
{
        assert(row->n_cards > 0);
        return row->cards[row->n_cards - 1];
}

static int
get_row_for_card(struct pcx_six *six,
                 pcx_six_card_t card)
{
        int best_row = -1;

        for (int i = 0; i < PCX_SIX_N_ROWS; i++) {
                pcx_six_card_t row_card = get_row_top(six->rows + i);

                if (row_card >= card)
                        continue;

                if (best_row == -1 ||
                    row_card > get_row_top(six->rows + best_row))
                        best_row = i;
        }

        return best_row;
}

static void
placement_timer_cb(struct pcx_main_context_source *source,
                   void *user_data)
{
        struct pcx_six *six = user_data;

        assert(source == six->placement_timer_source);

        six->placement_timer_source = NULL;

        if (six->next_placement >= six->n_players) {
                six->n_cards--;
                start_round(six);
                return;
        }

        struct pcx_six_player *player =
                six->reveal_order[six->next_placement];
        pcx_six_card_t card = player->hand[player->chosen_card];
        int row = get_row_for_card(six, card);

        if (row == -1) {
                show_choose_row(six, player);
        } else {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                const char *note = pcx_text_get(six->language,
                                                PCX_TEXT_STRING_ADDED_TO_ROW);
                pcx_buffer_append_printf(&buf,
                                         note,
                                         player->name,
                                         row + 'A');

                if (six->rows[row].n_cards >= PCX_SIX_ROW_SIZE) {
                        pcx_buffer_append_string(&buf, "\n\n");
                        note = pcx_text_get(six->language,
                                            PCX_TEXT_STRING_ROW_FULL);
                        int row_score = get_row_score(six->rows + row);
                        pcx_buffer_append_printf(&buf, note, row_score);
                        player->score_this_round += row_score;
                        six->rows[row].n_cards = 0;
                }

                place_card(six, player, row);

                pcx_buffer_append_string(&buf, "\n\n");
                add_row(six, &buf, row);

                struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

                message.text = (char *) buf.data;

                six->callbacks.send_message(&message, six->user_data);

                pcx_buffer_destroy(&buf);

                six->next_placement++;

                start_placement_timer(six);
        }
}

static void
start_placement_timer(struct pcx_six *six)
{
        if (six->placement_timer_source)
                return;

        six->placement_timer_source =
                pcx_main_context_add_timeout(NULL,
                                             PCX_SIX_PLACEMENT_TIME,
                                             placement_timer_cb,
                                             six);
}

static int
compare_reveal_order_cb(const void *a,
                        const void *b)
{
        struct pcx_six_player * const *pa = a;
        struct pcx_six_player * const *pb = b;

        return ((int) (*pa)->hand[(*pa)->chosen_card] -
                (int) (*pb)->hand[(*pb)->chosen_card]);
}

static void
get_reveal_order(struct pcx_six *six)
{
        for (int i = 0; i < six->n_players; i++)
                six->reveal_order[i] = six->players + i;

        qsort(six->reveal_order,
              six->n_players,
              sizeof six->reveal_order[0],
              compare_reveal_order_cb);
}

static void
reveal_cards(struct pcx_six *six)
{
        get_reveal_order(six);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *note = pcx_text_get(six->language,
                                        PCX_TEXT_STRING_CHOSEN_CARDS_ARE);
        pcx_buffer_append_string(&buf, note);

        for (int i = 0; i < six->n_players; i++) {
                const struct pcx_six_player *player = six->reveal_order[i];

                pcx_buffer_append_string(&buf, "\n\n");
                pcx_buffer_append_string(&buf, player->name);
                pcx_buffer_append_string(&buf, ":\n");
                add_card(&buf, player->hand[player->chosen_card]);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        six->callbacks.send_message(&message, six->user_data);

        pcx_buffer_destroy(&buf);

        six->next_placement = 0;

        start_placement_timer(six);
}

static void
play_card(struct pcx_six *six,
          int player_num,
          int card_num)
{
        if (card_num < 0 || card_num >= six->n_cards)
                return;
        if (six->placement_timer_source)
                return;
        if (six->player_choosing_row != -1)
                return;

        six->players[player_num].chosen_card = card_num;
        six->card_chosen_mask |= UINT32_C(1) << player_num;

        show_chosen_card(six, player_num);

        if (six->card_chosen_mask == (UINT32_C(1) << six->n_players) - 1)
                reveal_cards(six);
}

static void
choose_row(struct pcx_six *six,
           int player_num,
           int row_num)
{
        if (row_num < 0 || row_num >= PCX_SIX_N_ROWS)
                return;
        if (six->player_choosing_row != player_num)
                return;

        assert(six->placement_timer_source == NULL);

        struct pcx_six_player *player = six->players + player_num;

        int row_score = get_row_score(six->rows + row_num);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(six->language,
                                              PCX_TEXT_STRING_CHOSEN_ROW),
                                 player->name,
                                 row_num + 'A',
                                 row_score);

        player->score_this_round += row_score;
        six->rows[row_num].n_cards = 0;

        place_card(six, player, row_num);

        pcx_buffer_append_string(&buf, "\n\n");
        add_row(six, &buf, row_num);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        six->callbacks.send_message(&message, six->user_data);

        pcx_buffer_destroy(&buf);

        six->next_placement++;

        six->player_choosing_row = -1;

        start_placement_timer(six);
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_six *six = user_data;

        assert(player_num >= 0 && player_num < six->n_players);

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

        if (is_button(callback_data, colon, "play"))
                play_card(six, player_num, extra_data);
        else if (is_button(callback_data, colon, "choose_row"))
                choose_row(six, player_num, extra_data);
}

static void
free_game_cb(void *data)
{
        struct pcx_six *six = data;

        for (int i = 0; i < six->n_players; i++)
                pcx_free(six->players[i].name);

        if (six->game_over_source)
                pcx_main_context_remove_source(six->game_over_source);

        if (six->placement_timer_source)
                pcx_main_context_remove_source(six->placement_timer_source);

        pcx_free(six);
}

const struct pcx_game
pcx_six_game = {
        .name = "six",
        .name_string = PCX_TEXT_STRING_NAME_SIX,
        .start_command = PCX_TEXT_STRING_SIX_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_SIX_START_COMMAND_DESCRIPTION,
        .min_players = PCX_SIX_MIN_PLAYERS,
        .max_players = PCX_SIX_MAX_PLAYERS,
        .needs_private_messages = true,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
