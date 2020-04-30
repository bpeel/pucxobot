/*
 * Puxcobot - A robot to play coup in Esperanto (Puĉo)
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

#include "config.h"

#include "pcx-snitch.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-snitch-help.h"

#define PCX_SNITCH_MIN_PLAYERS 3
#define PCX_SNITCH_MAX_PLAYERS 5

#define PCX_SNITCH_N_BASE_ROLES 5
#define PCX_SNITCH_N_CARDS_PER_BASE_ROLE 13
#define PCX_SNITCH_N_BASE_CARDS (PCX_SNITCH_N_BASE_ROLES * \
                                 PCX_SNITCH_N_CARDS_PER_BASE_ROLE)

#define PCX_SNITCH_N_SNITCHES_PER_PLAYER 3
#define PCX_SNITCH_N_BASE_CARDS_PER_PLAYER 7

#define PCX_SNITCH_N_ROLES (PCX_SNITCH_N_BASE_ROLES + 1)

#define PCX_SNITCH_N_ROUNDS 8

#define PCX_SNITCH_MIN_HEIST_SIZE 2

enum pcx_snitch_role {
        PCX_SNITCH_ROLE_DRIVER,
        PCX_SNITCH_ROLE_LOCKPICK,
        PCX_SNITCH_ROLE_MUSCLE,
        PCX_SNITCH_ROLE_CON_ARTIST,
        PCX_SNITCH_ROLE_LOOKOUT,
        PCX_SNITCH_ROLE_SNITCH,
};

struct pcx_snitch_role_info {
        enum pcx_text_string name;
        const char *symbol;
        const char *keyword;
};

static const struct pcx_snitch_role_info
role_infos[] = {
        [PCX_SNITCH_ROLE_DRIVER] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_DRIVER,
                .symbol = "🚗",
                .keyword = "driver",
        },
        [PCX_SNITCH_ROLE_LOCKPICK] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_LOCKPICK,
                .symbol = "🔑",
                .keyword = "lockpick",
        },
        [PCX_SNITCH_ROLE_MUSCLE] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_MUSCLE,
                .symbol = "💪",
                .keyword = "muscle",
        },
        [PCX_SNITCH_ROLE_CON_ARTIST] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_CON_ARTIST,
                .symbol = "💼",
                .keyword = "conartist",
        },
        [PCX_SNITCH_ROLE_LOOKOUT] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_LOOKOUT,
                .symbol = "🔭",
                .keyword = "lookout",
        },
        [PCX_SNITCH_ROLE_SNITCH] =
        {
                .name = PCX_TEXT_STRING_ROLE_NAME_SNITCH,
                .symbol = "😈",
                .keyword = "snitch",
        },
};

_Static_assert(PCX_N_ELEMENTS(role_infos) == PCX_SNITCH_N_ROLES);

static const char
heist_size_keyword[] = "heistsize";

struct pcx_snitch_player {
        char *name;
        int coins;
        int cards[PCX_SNITCH_N_ROLES];
        int chosen_role;
};

struct pcx_snitch {
        int n_players;
        int round_num;
        int first_player;
        struct pcx_snitch_player players[PCX_SNITCH_MAX_PLAYERS];
        struct pcx_game_callbacks callbacks;
        void *user_data;
        enum pcx_text_language language;
        enum pcx_snitch_role deck[PCX_SNITCH_N_BASE_CARDS];
        enum pcx_snitch_role discarded_cards[PCX_SNITCH_N_BASE_CARDS];
        size_t n_cards, n_discarded_cards;
        int heist_size;
        int heist[PCX_SNITCH_N_BASE_CARDS];
        struct pcx_main_context_source *game_over_source;
};

static void
start_round(struct pcx_snitch *snitch);

static void
append_buffer_string(struct pcx_snitch *snitch,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string)
{
        const char *value = pcx_text_get(snitch->language, string);
        pcx_buffer_append_string(buffer, value);
}

static void
append_buffer_vprintf(struct pcx_snitch *snitch,
                      struct pcx_buffer *buffer,
                      enum pcx_text_string string,
                      va_list ap)
{
        const char *format = pcx_text_get(snitch->language, string);
        pcx_buffer_append_vprintf(buffer, format, ap);
}

static void
append_buffer_printf(struct pcx_snitch *snitch,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string,
                     ...)
{
        va_list ap;
        va_start(ap, string);
        append_buffer_vprintf(snitch, buffer, string, ap);
        va_end(ap);
}

static void
append_buffer_plural(struct pcx_snitch *snitch,
                     struct pcx_buffer *buf,
                     enum pcx_text_string single,
                     enum pcx_text_string plural,
                     int amount)
{
        if (amount == 1)
                append_buffer_string(snitch, buf, single);
        else
                append_buffer_printf(snitch, buf, plural, amount);
}

static void
shuffle_deck(struct pcx_snitch *snitch)
{
        if (snitch->n_cards < 2)
                return;

        for (unsigned i = snitch->n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                enum pcx_snitch_role t = snitch->deck[j];
                snitch->deck[j] = snitch->deck[i];
                snitch->deck[i] = t;
        }
}

static enum pcx_snitch_role
take_card(struct pcx_snitch *snitch)
{
        if (snitch->n_cards == 0) {
                snitch->n_cards = snitch->n_discarded_cards;
                memcpy(snitch->deck,
                       snitch->discarded_cards,
                       snitch->n_cards * sizeof snitch->deck[0]);
                shuffle_deck(snitch);
                snitch->n_discarded_cards = 0;
        }

        assert(snitch->n_cards > 0);

        return snitch->deck[--snitch->n_cards];
}

static int
get_current_player(struct pcx_snitch *snitch)
{
        return (snitch->first_player + snitch->round_num) % snitch->n_players;
}

static void
start_round(struct pcx_snitch *snitch)
{
        snitch->heist_size = -1;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_printf(snitch,
                             &buf,
                             PCX_TEXT_STRING_ROUND_NUM,
                             snitch->round_num + 1,
                             PCX_SNITCH_N_ROUNDS);
        pcx_buffer_append_string(&buf, "\n\n");

        struct pcx_snitch_player *current_player =
                snitch->players + get_current_player(snitch);

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                player->chosen_role = -1;

                if (player == current_player)
                        pcx_buffer_append_string(&buf, "👉 ");

                pcx_buffer_append_string(&buf, player->name);
                pcx_buffer_append_string(&buf, ", ");

                append_buffer_plural(snitch,
                                     &buf,
                                     PCX_TEXT_STRING_1_COIN,
                                     PCX_TEXT_STRING_PLURAL_COINS,
                                     player->coins);

                pcx_buffer_append_string(&buf, "\n");
        }

        pcx_buffer_append_string(&buf, "\n");

        append_buffer_printf(snitch,
                             &buf,
                             PCX_TEXT_STRING_CHOOSE_HEIST_DIFFICULTY,
                             snitch->players[get_current_player(snitch)].name);

        struct pcx_game_button buttons[PCX_SNITCH_MAX_PLAYERS];
        int n_buttons = 0;

        for (unsigned i = PCX_SNITCH_MIN_HEIST_SIZE;
             i <= snitch->n_players;
             i++) {
                struct pcx_buffer label_buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&label_buf, "%i", i);
                struct pcx_buffer data_buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&data_buf,
                                         "%s:%i",
                                         heist_size_keyword,
                                         i);

                buttons[n_buttons].text = (char *) label_buf.data;
                buttons[n_buttons].data = (char *) data_buf.data;
                n_buttons++;
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;
        message.button_players = UINT32_C(1) << get_current_player(snitch);
        message.n_buttons = n_buttons;
        message.buttons = buttons;

        snitch->callbacks.send_message(&message, snitch->user_data);

        for (int i = 0; i < n_buttons; i++) {
                pcx_free((char *) buttons[i].text);
                pcx_free((char *) buttons[i].data);
        }

        pcx_buffer_destroy(&buf);
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players >= PCX_SNITCH_MIN_PLAYERS &&
               n_players <= PCX_SNITCH_MAX_PLAYERS);

        struct pcx_snitch *snitch = pcx_calloc(sizeof *snitch);

        snitch->language = language;
        snitch->callbacks = *callbacks;
        snitch->user_data = user_data;

        snitch->n_players = n_players;
        snitch->first_player = rand() % n_players;

        for (unsigned role = 0; role < PCX_SNITCH_N_BASE_ROLES; role++) {
                for (unsigned i = 0;
                     i < PCX_SNITCH_N_CARDS_PER_BASE_ROLE;
                     i++) {
                        snitch->deck[snitch->n_cards++] = role;
                }
        }

        shuffle_deck(snitch);

        assert(snitch->n_cards == PCX_SNITCH_N_BASE_CARDS);

        for (unsigned i = 0; i < n_players; i++) {
                snitch->players[i].name = pcx_strdup(names[i]);

                snitch->players[i].cards[PCX_SNITCH_ROLE_SNITCH] =
                        PCX_SNITCH_N_SNITCHES_PER_PLAYER;

                for (unsigned c = 0;
                     c < PCX_SNITCH_N_BASE_CARDS_PER_PLAYER;
                     c++) {
                        snitch->players[i].cards[take_card(snitch)]++;
                }

                snitch->players[i].coins = 3;
        }

        start_round(snitch);

        return snitch;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_snitch_help[language]);
}

static void
send_card_choice(struct pcx_snitch *snitch,
                 int player_num)
{
        struct pcx_game_button buttons[PCX_SNITCH_N_ROLES];
        size_t n_buttons = 0;
        const struct pcx_snitch_player *player = snitch->players + player_num;

        for (unsigned i = 0; i < PCX_SNITCH_N_ROLES; i++) {
                if (player->cards[i] <= 0)
                        continue;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                if (player->cards[i] > 1) {
                        pcx_buffer_append_printf(&buf,
                                                 "%i × ",
                                                 player->cards[i]);
                }

                pcx_buffer_append_string(&buf, role_infos[i].symbol);
                pcx_buffer_append_c(&buf, ' ');
                append_buffer_string(snitch, &buf, role_infos[i].name);

                buttons[n_buttons].text = (char *) buf.data;
                buttons[n_buttons].data = role_infos[i].keyword;
                n_buttons++;
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = pcx_text_get(snitch->language,
                                    PCX_TEXT_STRING_WHICH_ROLE);
        message.n_buttons = n_buttons;
        message.buttons = buttons;
        message.target = player_num;

        snitch->callbacks.send_message(&message, snitch->user_data);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].text);
}

static void
handle_set_heist_size(struct pcx_snitch *snitch,
                      int player_num,
                      int heist_size)
{
        if (snitch->heist_size != -1)
                return;

        if (player_num != get_current_player(snitch))
                return;

        if (heist_size < PCX_SNITCH_MIN_HEIST_SIZE ||
            heist_size > snitch->n_players)
                return;

        snitch->heist_size = heist_size;

        memset(snitch->heist, 0, sizeof snitch->heist);

        for (unsigned i = 0; i < heist_size; i++)
                snitch->heist[take_card(snitch)]++;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_printf(snitch,
                             &buf,
                             PCX_TEXT_STRING_HEIST_SIZE_CHOSEN,
                             heist_size);
        pcx_buffer_append_string(&buf, "\n\n");

        for (unsigned i = 0; i < PCX_SNITCH_N_BASE_ROLES; i++) {
                if (snitch->heist[i] == 0)
                        continue;

                if (snitch->heist[i] > 1) {
                        pcx_buffer_append_printf(&buf,
                                                 "%i × ",
                                                 snitch->heist[i]);
                }

                pcx_buffer_append_string(&buf, role_infos[i].symbol);
                pcx_buffer_append_c(&buf, ' ');

                append_buffer_string(snitch, &buf, role_infos[i].name);

                pcx_buffer_append_string(&buf, "\n");
        }

        pcx_buffer_append_string(&buf, "\n");

        append_buffer_string(snitch,
                             &buf,
                             PCX_TEXT_STRING_DISCUSS_HEIST);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;

        snitch->callbacks.send_message(&message, snitch->user_data);

        pcx_buffer_destroy(&buf);

        for (unsigned i = 0; i < snitch->n_players; i++) {
                snitch->players[i].chosen_role = -1;
                send_card_choice(snitch, i);
        }
}

static bool
are_all_cards_chosen(struct pcx_snitch *snitch)
{
        for (unsigned i = 0; i < snitch->n_players; i++) {
                if (snitch->players[i].chosen_role == -1)
                        return false;
        }

        return true;
}

static bool
is_heist_successful(struct pcx_snitch *snitch)
{
        int chosen_cards[PCX_SNITCH_N_ROLES] = { 0 };

        for (unsigned i = 0; i < snitch->n_players; i++)
                chosen_cards[snitch->players[i].chosen_role]++;

        for (unsigned i = 0; i < PCX_SNITCH_N_BASE_ROLES; i++) {
                if (chosen_cards[i] < snitch->heist[i])
                        return false;
        }

        return true;
}

static void
append_needed_card_symbols(struct pcx_snitch *snitch,
                           struct pcx_buffer *buf)
{
        for (unsigned role = 0; role < PCX_SNITCH_N_BASE_ROLES; role++) {
                for (unsigned i = 0; i < snitch->heist[role]; i++)
                        pcx_buffer_append_string(buf, role_infos[role].symbol);
        }
}

static void
handle_successful_heist(struct pcx_snitch *snitch,
                        struct pcx_buffer *buf)
{
        append_buffer_printf(snitch,
                             buf,
                             PCX_TEXT_STRING_HEIST_SUCCESS,
                             snitch->heist_size);

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                if (player->chosen_role == PCX_SNITCH_ROLE_SNITCH)
                        continue;

                player->coins += snitch->heist_size;
        }
}

static void
handle_failed_heist(struct pcx_snitch *snitch,
                    struct pcx_buffer *buf)
{
        int loot = 0;

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                if (player->chosen_role == PCX_SNITCH_ROLE_SNITCH)
                        continue;

                player->coins--;
                loot++;
        }

        int n_snitches = snitch->n_players - loot;
        int split;

        if (n_snitches == 0)
                split = loot;
        else
                split = (loot + n_snitches - 1) / n_snitches;

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                if (player->chosen_role != PCX_SNITCH_ROLE_SNITCH)
                        continue;

                player->coins += split;
        }

        if (n_snitches >= snitch->n_players) {
                append_buffer_string(snitch,
                                     buf,
                                     PCX_TEXT_STRING_EVERYONE_SNITCHED);
        } else {
                append_buffer_string(snitch, buf, PCX_TEXT_STRING_HEIST_FAILED);
                pcx_buffer_append_c(buf, ' ');

                if (n_snitches == 0) {
                        append_buffer_string(snitch,
                                             buf,
                                             PCX_TEXT_STRING_NOONE_SNITCHED);
                } else {
                        append_buffer_plural(snitch,
                                             buf,
                                             PCX_TEXT_STRING_SNITCH_GAIN_1,
                                             PCX_TEXT_STRING_SNITCH_GAIN_PLURAL,
                                             split);
                }
        }
}

static void
discard_cards(struct pcx_snitch *snitch)
{
        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                assert(player->cards[player->chosen_role] > 0);

                player->cards[player->chosen_role]--;

                if (player->chosen_role == PCX_SNITCH_ROLE_SNITCH)
                        continue;

                assert(snitch->n_discarded_cards <
                       PCX_N_ELEMENTS(snitch->discarded_cards));

                snitch->discarded_cards[snitch->n_discarded_cards++] =
                        player->chosen_role;
        }

        for (unsigned i = 0; i < PCX_SNITCH_N_BASE_ROLES; i++) {
                assert(snitch->n_discarded_cards +
                       snitch->heist[i] <=
                       PCX_N_ELEMENTS(snitch->discarded_cards));
                for (unsigned j = 0; j < snitch->heist[i]; j++) {
                        snitch->discarded_cards[snitch->n_discarded_cards] = i;
                        snitch->n_discarded_cards++;
                }
        }
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_snitch *snitch = user_data;
        snitch->game_over_source = NULL;
        snitch->callbacks.game_over(snitch->user_data);
}

static void
get_final_status(struct pcx_snitch *snitch,
                 struct pcx_buffer *buf)
{
        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                pcx_buffer_append_string(buf, player->name);
                pcx_buffer_append_string(buf, ", ");

                append_buffer_plural(snitch,
                                     buf,
                                     PCX_TEXT_STRING_1_COIN,
                                     PCX_TEXT_STRING_PLURAL_COINS,
                                     player->coins);

                pcx_buffer_append_string(buf, ", ");

                append_buffer_plural(snitch,
                                     buf,
                                     PCX_TEXT_STRING_1_SNITCH,
                                     PCX_TEXT_STRING_PLURAL_SNITCHES,
                                     player->cards[PCX_SNITCH_ROLE_SNITCH]);

                pcx_buffer_append_string(buf, "\n");
        }

        pcx_buffer_append_string(buf, "\n");
}

static void
end_game(struct pcx_snitch *snitch)
{
        const struct pcx_snitch_player *best_player = snitch->players + 0;
        int n_winners = 1;

        for (unsigned i = 1; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                if (player->coins > best_player->coins) {
                        n_winners = 1;
                        best_player = player;
                } else if (player->coins == best_player->coins) {
                        int diff = (player->cards[PCX_SNITCH_ROLE_SNITCH] -
                                    best_player->cards[PCX_SNITCH_ROLE_SNITCH]);
                        if (diff > 0) {
                                n_winners = 1;
                                best_player = player;
                        } else if (diff == 0) {
                                n_winners++;
                        }
                }
        }

        struct pcx_buffer winners = PCX_BUFFER_STATIC_INIT;
        int winner_num = 0;

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                if (player->coins != best_player->coins ||
                    player->cards[PCX_SNITCH_ROLE_SNITCH] !=
                    best_player->cards[PCX_SNITCH_ROLE_SNITCH])
                        continue;

                if (n_winners > 1) {
                        if (winner_num >= n_winners - 1) {
                                enum pcx_text_string sep =
                                        PCX_TEXT_STRING_FINAL_CONJUNCTION;
                                append_buffer_string(snitch, &winners, sep);
                        } else if (winner_num > 0) {
                                pcx_buffer_append_string(&winners, ", ");
                        }
                }

                pcx_buffer_append_string(&winners, player->name);

                winner_num++;
        }

        assert(winner_num == n_winners);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        get_final_status(snitch, &buf);

        pcx_buffer_append_string(&buf, "🏆 ");

        if (n_winners == 1) {
                append_buffer_printf(snitch,
                                     &buf,
                                     PCX_TEXT_STRING_WON_1,
                                     winners.data);
        } else {
                append_buffer_printf(snitch,
                                     &buf,
                                     PCX_TEXT_STRING_WON_PLURAL,
                                     winners.data);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;

        snitch->callbacks.send_message(&message, snitch->user_data);

        pcx_buffer_destroy(&buf);
        pcx_buffer_destroy(&winners);

        if (snitch->game_over_source == NULL) {
                snitch->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     snitch);
        }
}

static void
end_round(struct pcx_snitch *snitch)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_string(snitch, &buf, PCX_TEXT_STRING_CARDS_CHOSEN);
        pcx_buffer_append_string(&buf, "\n\n");

        for (unsigned i = 0; i < snitch->n_players; i++) {
                struct pcx_snitch_player *player = snitch->players + i;

                pcx_buffer_append_string(&buf, player->name);
                pcx_buffer_append_string(&buf, ": ");

                const struct pcx_snitch_role_info *role =
                        role_infos + player->chosen_role;

                pcx_buffer_append_string(&buf, role->symbol);
                pcx_buffer_append_c(&buf, ' ');
                append_buffer_string(snitch, &buf, role->name);

                pcx_buffer_append_string(&buf, "\n");
        }

        pcx_buffer_append_string(&buf, "\n");
        append_buffer_string(snitch, &buf, PCX_TEXT_STRING_NEEDED_CARDS_WERE);
        pcx_buffer_append_string(&buf, "\n");

        append_needed_card_symbols(snitch, &buf);

        pcx_buffer_append_string(&buf, "\n\n");

        bool is_success = is_heist_successful(snitch);

        if (is_success)
                handle_successful_heist(snitch, &buf);
        else
                handle_failed_heist(snitch, &buf);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;

        snitch->callbacks.send_message(&message, snitch->user_data);

        pcx_buffer_destroy(&buf);

        snitch->round_num++;
        discard_cards(snitch);

        if (snitch->round_num >= PCX_SNITCH_N_ROUNDS)
                end_game(snitch);
        else
                start_round(snitch);
}

static void
send_chosen_card(struct pcx_snitch *snitch,
                 int player_num,
                 enum pcx_snitch_role role)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_string(snitch, &buf, PCX_TEXT_STRING_YOU_CHOSE);
        pcx_buffer_append_c(&buf, ' ');
        pcx_buffer_append_string(&buf, role_infos[role].symbol);
        pcx_buffer_append_c(&buf, ' ');
        append_buffer_string(snitch, &buf, role_infos[role].name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;
        message.target = player_num;

        snitch->callbacks.send_message(&message, snitch->user_data);

        pcx_buffer_destroy(&buf);
}

static void
handle_choose_card(struct pcx_snitch *snitch,
                   int player_num,
                   enum pcx_snitch_role role)
{
        if (snitch->heist_size == -1)
                return;

        struct pcx_snitch_player *player = snitch->players + player_num;

        if (player->cards[role] <= 0)
                return;

        player->chosen_role = role;

        send_chosen_card(snitch, player_num, role);

        if (are_all_cards_chosen(snitch))
                end_round(snitch);
}

static bool
is_keyword(const char *data_start,
           const char *data_end,
           const char *keyword)
{
        size_t keyword_len = strlen(keyword);
        return (data_end - data_start == keyword_len &&
                !memcmp(data_start, keyword, keyword_len));
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_snitch *snitch = user_data;

        assert(player_num >= 0 && player_num < snitch->n_players);

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

        if (is_keyword(callback_data, colon, heist_size_keyword)) {
                handle_set_heist_size(snitch, player_num, extra_data);
                return;
        }

        for (unsigned i = 0; i < PCX_SNITCH_N_ROLES; i++) {
                if (is_keyword(callback_data, colon, role_infos[i].keyword)) {
                        handle_choose_card(snitch, player_num, i);
                        return;
                }
        }
}

static void
free_game_cb(void *data)
{
        struct pcx_snitch *snitch = data;

        for (int i = 0; i < snitch->n_players; i++)
                pcx_free(snitch->players[i].name);

        if (snitch->game_over_source)
                pcx_main_context_remove_source(snitch->game_over_source);

        pcx_free(snitch);
}

const struct pcx_game
pcx_snitch_game = {
        .name = "snitch",
        .name_string = PCX_TEXT_STRING_NAME_SNITCH,
        .start_command = PCX_TEXT_STRING_SNITCH_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_SNITCH_START_COMMAND_DESCRIPTION,
        .min_players = PCX_SNITCH_MIN_PLAYERS,
        .max_players = PCX_SNITCH_MAX_PLAYERS,
        .needs_private_messages = true,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
