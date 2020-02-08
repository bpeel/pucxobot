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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "pcx-coup.h"
#include "pcx-list.h"
#include "pcx-main-context.h"

enum message_type {
        MESSAGE_TYPE_PRIVATE,
        MESSAGE_TYPE_GLOBAL,
        MESSAGE_TYPE_GAME_OVER,
        MESSAGE_TYPE_STATUS,
        MESSAGE_TYPE_SHOW_CARDS,
};

struct message {
        struct pcx_list link;
        enum message_type type;
        int destination;
        char *message;
};

struct card_status {
        enum pcx_coup_character character;
        bool dead;
};

struct player_status {
        struct card_status cards[2];
        int coins;
};

struct status {
        struct player_status players[2];
        int current_player;
};

struct test_data {
        struct pcx_coup *coup;
        struct pcx_list message_queue;
        struct pcx_main_context_source *check_timeout_source;
        struct status status;
        bool had_error;
};

static const char *const
player_names[] = {
        "Alice",
        "Bob",
};

static void
free_message(struct message *message)
{
        pcx_free(message->message);
        pcx_free(message);
}

static const char *
get_card_name(enum pcx_coup_character character)
{
        return pcx_text_get(PCX_TEXT_LANGUAGE_ESPERANTO,
                            pcx_coup_characters[character].name);
}

static char *
make_show_cards_message(const struct player_status *status)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf, "Viaj kartoj estas:\n");

        for (unsigned i = 0; i < PCX_N_ELEMENTS(status->cards); i++) {
                const char *card_name =
                        get_card_name(status->cards[i].character);
                bool dead = status->cards[i].dead;
                if (dead)
                        pcx_buffer_append_string(&buf, "☠");
                pcx_buffer_append_string(&buf, card_name);
                if (dead)
                        pcx_buffer_append_string(&buf, "☠");
                pcx_buffer_append_string(&buf, "\n");
        }

        return (char *) buf.data;
}

static void
make_card_status(struct pcx_buffer *buf,
                 const struct card_status *card)
{
        if (card->dead) {
                const char *card_name = get_card_name(card->character);
                pcx_buffer_append_printf(buf, "☠%s☠", card_name);
        } else {
                pcx_buffer_append_string(buf, "🂠");
        }
}

static bool
is_alive(const struct player_status *player)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(player->cards); i++) {
                if (!player->cards[i].dead)
                        return true;
        }

        return false;
}

static char *
make_status_message(const struct status *status)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        int winner = -1;
        int n_alive_players = 0;

        for (unsigned i = 0; i < PCX_N_ELEMENTS(player_names); i++) {
                if (is_alive(status->players + i)) {
                        winner = i;
                        n_alive_players++;
                }
        }

        for (unsigned i = 0; i < PCX_N_ELEMENTS(player_names); i++) {
                const struct player_status *player = status->players + i;

                if (n_alive_players == 1) {
                        if (winner == i)
                                pcx_buffer_append_string(&buf, "🏆 ");
                } else if (status->current_player == i) {
                        pcx_buffer_append_string(&buf, "👉 ");
                }

                pcx_buffer_append_printf(&buf, "%s:\n", player_names[i]);

                for (unsigned card = 0;
                     card < PCX_N_ELEMENTS(player->cards);
                     card++) {
                        make_card_status(&buf, player->cards + card);
                }

                if (is_alive(status->players + i)) {
                        pcx_buffer_append_string(&buf, ", ");

                        if (player->coins == 1) {
                                pcx_buffer_append_string(&buf, "1 monero");
                        } else {
                                pcx_buffer_append_printf(&buf,
                                                         "%i moneroj",
                                                         player->coins);
                        }
                }

                pcx_buffer_append_string(&buf, "\n\n");
        }

        if (n_alive_players == 1) {
                pcx_buffer_append_printf(&buf,
                                         "%s venkis!",
                                         player_names[winner]);
        } else {
                pcx_buffer_append_printf(&buf,
                                         "%s, estas via vico, "
                                         "kion vi volas fari?",
                                         player_names[status->current_player]);
        }


        return (char *) buf.data;
}

static void
send_private_message_cb(int user_num,
                        enum pcx_game_message_format format,
                        const char *message_text,
                        size_t n_buttons,
                        const struct pcx_game_button *buttons,
                        void *user_data)
{
        struct test_data *data = user_data;

        if (user_num < 0 || user_num >= PCX_N_ELEMENTS(player_names)) {
                fprintf(stderr,
                        "Private message sent to invalide player %i\n",
                        user_num);
                data->had_error = true;
                return;
        }

        if (pcx_list_empty(&data->message_queue)) {
                fprintf(stderr,
                        "Unexpected message sent to “%s”: %s\n",
                        player_names[user_num],
                        message_text);
                data->had_error = true;
                return;
        }

        struct message *message =
                pcx_container_of(data->message_queue.next,
                                 struct message,
                                 link);

        if (message->type != MESSAGE_TYPE_PRIVATE) {
                fprintf(stderr,
                        "Private message to “%s” received when a different "
                        "type was expected: %s\n",
                        player_names[user_num],
                        message_text);
                data->had_error = true;
                return;
        }

        if (message->destination != user_num) {
                fprintf(stderr,
                        "Message sent to “%s” but expected to “%s”\n",
                        player_names[user_num],
                        player_names[message->destination]);
                data->had_error = true;
                return;
        }

        if (strcmp(message_text, message->message)) {
                fprintf(stderr,
                        "Message to “%s” does not match expected message.\n"
                        "Got: %s\n"
                        "Expected: %s\n",
                        player_names[user_num],
                        message_text,
                        message->message);
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static void
send_message_cb(enum pcx_game_message_format format,
                const char *message_text,
                size_t n_buttons,
                const struct pcx_game_button *buttons,
                void *user_data)
{
        struct test_data *data = user_data;

        if (pcx_list_empty(&data->message_queue)) {
                fprintf(stderr,
                        "Unexpected global message sent: %s\n",
                        message_text);
                data->had_error = true;
                return;
        }

        struct message *message =
                pcx_container_of(data->message_queue.next,
                                 struct message,
                                 link);

        if (message->type != MESSAGE_TYPE_GLOBAL) {
                fprintf(stderr,
                        "Global message received when a different "
                        "type was expected: %s\n",
                        message_text);
                data->had_error = true;
                return;
        }

        if (strcmp(message_text, message->message)) {
                fprintf(stderr,
                        "Global Message does not match expected message.\n"
                        "Got: %s\n"
                        "Expected: %s\n",
                        message_text,
                        message->message);
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static void
game_over_cb(void *user_data)
{
        struct test_data *data = user_data;

        if (pcx_list_empty(&data->message_queue)) {
                fprintf(stderr, "Unexpected game over\n");
                data->had_error = true;
                return;
        }

        struct message *message =
                pcx_container_of(data->message_queue.next,
                                 struct message,
                                 link);

        if (message->type != MESSAGE_TYPE_GAME_OVER) {
                fprintf(stderr,
                        "Game over received when a different "
                        "type was expected\n");
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static const struct pcx_game_callbacks
callbacks = {
        .send_private_message = send_private_message_cb,
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
timeout_cb(struct pcx_main_context_source *source,
           void *user_data)
{
        struct test_data *data = user_data;

        assert(data->check_timeout_source == source);

        data->check_timeout_source = NULL;

        fprintf(stderr, "Timeout while waiting for a message\n");

        data->had_error = true;
}

static struct message *
queue_message(struct test_data *data,
              enum message_type type)
{
        struct message *message = pcx_calloc(sizeof *message);

        message->type = type;
        pcx_list_insert(data->message_queue.prev, &message->link);

        return message;
}

static bool
send_callback_data(struct test_data *data,
                   int player_num,
                   const char *callback_data,
                   ...)
{
        va_list ap;

        va_start(ap, callback_data);

        while (true) {
                int type = va_arg(ap, int);

                if (type == -1)
                        break;

                struct message *message = queue_message(data, type);

                switch ((enum message_type) type) {
                case MESSAGE_TYPE_PRIVATE:
                        message->destination = va_arg(ap, int);
                        message->message = pcx_strdup(va_arg(ap, const char *));
                        continue;
                case MESSAGE_TYPE_GLOBAL:
                        message->message = pcx_strdup(va_arg(ap, const char *));
                        continue;
                case MESSAGE_TYPE_GAME_OVER:
                        continue;
                case MESSAGE_TYPE_STATUS:
                        message->type = MESSAGE_TYPE_GLOBAL;
                        message->message = make_status_message(&data->status);
                        continue;
                case MESSAGE_TYPE_SHOW_CARDS:
                        message->type = MESSAGE_TYPE_PRIVATE;
                        message->destination = va_arg(ap, int);
                        message->message =
                                make_show_cards_message(data->status.players +
                                                        message->destination);
                        continue;
                }

                assert(!"unexpected message type");
        }

        va_end(ap);

        assert(data->check_timeout_source == NULL);

        pcx_coup_game.handle_callback_data_cb(data->coup,
                                              player_num,
                                              callback_data);

        data->check_timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             10000,
                                             timeout_cb,
                                             data);

        while (!data->had_error &&
               !pcx_list_empty(&data->message_queue)) {
                pcx_main_context_poll(NULL);
        }

        if (data->check_timeout_source) {
                pcx_main_context_remove_source(data->check_timeout_source);
                data->check_timeout_source = NULL;
        }

        return !data->had_error;
}

static struct test_data *
create_test_data(int n_card_overrides,
                 const enum pcx_coup_character *card_overrides)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        data->had_error = false;
        pcx_list_init(&data->message_queue);

        data->status.current_player = 1;

        for (unsigned i = 0; i < 2; i++) {
                struct player_status *player = data->status.players + i;

                player->coins = (i == data->status.current_player) ? 1 : 2;

                for (unsigned j = 0; j < 2; j++)
                        player->cards[j].character = card_overrides[i * 2 + j];

                struct message *message =
                        queue_message(data, MESSAGE_TYPE_PRIVATE);
                message->destination = i;
                message->message = make_show_cards_message(player);
        }

        queue_message(data, MESSAGE_TYPE_GLOBAL)->message =
                make_status_message(&data->status);

        srand(0);

        struct pcx_coup_debug_overrides overrides = {
                .n_cards = n_card_overrides,
                .cards = card_overrides,
        };

        data->coup = pcx_coup_new(&callbacks,
                                  data,
                                  PCX_TEXT_LANGUAGE_ESPERANTO,
                                  PCX_N_ELEMENTS(player_names),
                                  player_names,
                                  &overrides);

        return data;
}

static void
free_test_data(struct test_data *data)
{
        struct message *message, *tmp;

        pcx_list_for_each_safe(message, tmp, &data->message_queue, link) {
                free_message(message);
        }

        pcx_coup_game.free_game_cb(data->coup);
        pcx_free(data);
}

static bool
take_income(struct test_data *data)
{
        int active_player = data->status.current_player;

        data->status.players[active_player].coins++;
        data->status.current_player = active_player ^ 1;

        struct pcx_buffer message = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&message,
                                 "💲 %s enspezas 1 moneron",
                                 player_names[active_player]);

        bool ret = send_callback_data(data,
                                      active_player,
                                      "income",
                                      MESSAGE_TYPE_GLOBAL,
                                      (char *) message.data,
                                      MESSAGE_TYPE_STATUS,
                                      -1);

        pcx_buffer_destroy(&message);

        return ret;
}

static bool
test_income(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_ASSASSIN
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards);

        bool ret = true;

        for (int i = 0; i < 4; i++) {
                if (!take_income(data)) {
                        ret = false;
                        break;
                }
        }

        free_test_data(data);

        return ret;
}

static bool
do_coup(struct test_data *data)
{
        int active_player = data->status.current_player;

        struct pcx_buffer message = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&message,
                                 "💣 %s faras puĉon kontraŭ %s",
                                 player_names[active_player],
                                 player_names[active_player ^ 1]);

        struct pcx_buffer callback_data = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&callback_data, "coup:%i", active_player ^ 1);

        bool ret = send_callback_data(data,
                                      active_player,
                                      (char *) callback_data.data,
                                      MESSAGE_TYPE_GLOBAL,
                                      (char *) message.data,
                                      MESSAGE_TYPE_PRIVATE,
                                      active_player ^ 1,
                                      "Kiun karton vi volas perdi?",
                                      -1);
        if (!ret)
                goto done;

        int card_to_lose;

        for (card_to_lose = 0; card_to_lose < 2; card_to_lose++) {
                struct card_status *card =
                        (data->status.players[active_player ^ 1].cards +
                         card_to_lose);
                if (!card->dead)
                        goto found_card;
        }

        assert(!"no alive card found to reveal");

found_card:
        pcx_buffer_set_length(&callback_data, 0);
        pcx_buffer_append_printf(&callback_data, "lose:%i", card_to_lose);
        data->status.players[active_player].coins -= 7;
        data->status.players[active_player ^ 1].cards[card_to_lose].dead = true;
        data->status.current_player ^= 1;

        ret = send_callback_data(data,
                                 active_player ^ 1,
                                 (char *) callback_data.data,
                                 MESSAGE_TYPE_SHOW_CARDS,
                                 active_player ^ 1,
                                 MESSAGE_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        pcx_buffer_destroy(&message);
        pcx_buffer_destroy(&callback_data);

        return ret;
}

static bool
test_coup(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_ASSASSIN
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards);

        /* Try to do a coup without having enough coins. The request
         * should just be silently ignored.
         */
        bool ret = send_callback_data(data,
                                      1,
                                      "coup:0",
                                      -1);
        if (!ret)
                goto done;

        /* Give 6 coins to each player so that Bob will have 7 */
        for (int i = 0; i < 12; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        assert(data->status.players[1].coins == 7 &&
               data->status.current_player == 1);

        /* The coup should work this time */
        if (!do_coup(data)) {
                ret = false;
                goto done;
        }

        /* Give two coins to each player. That way it should be
         * Alice’s turn and she should have 10 coins.
         */
        for (int i = 0; i < 4; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        assert(data->status.players[0].coins == 10 &&
               data->status.current_player == 0);

        /* Try to take income. This should be silently ignored because
         * Alice is forced to do a coup when she has 10 coins.
         */
        ret = send_callback_data(data,
                                 0,
                                 "income",
                                 -1);
        if (!ret)
                goto done;

        /* The coup should work this time */
        if (!do_coup(data)) {
                ret = false;
                goto done;
        }

        /* Give five coins to each player so that Bob will have 7
         * again
         */
        for (int i = 0; i < 10; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        assert(data->status.players[1].coins == 7 &&
               data->status.current_player == 1);

        /* Let Bob finish the game with a coup */
        data->status.players[1].coins = 0;
        data->status.players[0].cards[1].dead = true;

        ret = send_callback_data(data,
                                 1,
                                 "coup:0",
                                 MESSAGE_TYPE_GLOBAL,
                                 "💣 Bob faras puĉon kontraŭ Alice",
                                 MESSAGE_TYPE_STATUS,
                                 MESSAGE_TYPE_GAME_OVER,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static struct test_data *
set_up_foreign_aid(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_ASSASSIN,
                PCX_COUP_CHARACTER_AMBASSADOR
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards);

        bool ret = send_callback_data(data,
                                      1,
                                      "foreign_aid",
                                      MESSAGE_TYPE_GLOBAL,
                                      "💴 Bob prenas 2 monerojn per "
                                      "eksterlanda helpo.\n"
                                      "Ĉu iu volas pretendi havi la dukon "
                                      "kaj bloki rin?",
                                      -1);
        if (!ret) {
                free_test_data(data);
                return NULL;
        }

        return data;
}

static bool
test_accept_foreign_aid(void)
{
        struct test_data *data = set_up_foreign_aid();

        if (data == NULL)
                return false;

        data->status.current_player = 0;
        data->status.players[1].coins += 2;

        /* Try block the action */
        bool ret = send_callback_data(data,
                                      0,
                                      "accept",
                                      MESSAGE_TYPE_GLOBAL,
                                      "Neniu blokis, Bob prenas la 2 monerojn",
                                      MESSAGE_TYPE_STATUS,
                                      -1);

        free_test_data(data);

        return ret;
}

static bool
block_foreign_aid(struct test_data *data)
{
        return send_callback_data(data,
                                  0,
                                  "block",
                                  MESSAGE_TYPE_GLOBAL,
                                  "Alice pretendas havi la dukon kaj "
                                  "blokas.\n"
                                  "Ĉu iu volas defii rin?",
                                  -1);
}

static bool
block_and_challenge_foreign_aid(struct test_data *data)
{
        bool ret;

        /* Try blocking the action */
        ret = block_foreign_aid(data);
        if (!ret)
                return false;

        /* Challenge it! */
        ret = send_callback_data(data,
                                 1,
                                 "challenge",
                                 MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Bob ne kredas ke vi havas la dukon.\n"
                                 "Kiun karton vi volas montri?",
                                 -1);
        if (!ret)
                return false;

        return true;
}

static bool
test_accept_block_foreign_aid(void)
{
        struct test_data *data = set_up_foreign_aid();

        if (data == NULL)
                return false;

        bool ret;

        ret = block_foreign_aid(data);
        if (!ret)
                goto done;

        data->status.current_player = 0;

        /* Accept the block */
        ret = send_callback_data(data,
                                 1,
                                 "accept",
                                 MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 MESSAGE_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_failed_challenge_block_foreign_aid(void)
{
        struct test_data *data = set_up_foreign_aid();

        if (data == NULL)
                return false;

        bool ret;

        ret = block_and_challenge_foreign_aid(data);
        if (!ret)
                goto done;

        data->status.players[0].cards[0].character =
                PCX_COUP_CHARACTER_AMBASSADOR;

        /* Reveal the duke */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:0",
                                 MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis sed Alice ja havis la dukon kaj "
                                 "Bob perdas karton",
                                 MESSAGE_TYPE_SHOW_CARDS,
                                 0,
                                 MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiun karton vi volas perdi?",
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[0].dead = true;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 1,
                                 "lose:0",
                                 MESSAGE_TYPE_SHOW_CARDS,
                                 1,
                                 MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 MESSAGE_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_failed_block_foreign_aid(void)
{
        struct test_data *data = set_up_foreign_aid();

        if (data == NULL)
                return false;

        bool ret;

        ret = block_and_challenge_foreign_aid(data);
        if (!ret)
                goto done;

        data->status.players[0].cards[1].dead = true;

        /* Reveal something other than the duke */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:1",
                                 MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj Alice ne havis la dukon kaj "
                                 "Alice perdas karton",
                                 MESSAGE_TYPE_SHOW_CARDS,
                                 0,
                                 MESSAGE_TYPE_GLOBAL,
                                 "💴 Bob prenas 2 monerojn per "
                                 "eksterlanda helpo.\n"
                                 "Ĉu iu volas pretendi havi la dukon "
                                 "kaj bloki rin?",
                                 -1);
        if (!ret)
                goto done;

        /* Now just accept it */
        data->status.current_player = 0;
        data->status.players[1].coins += 2;

        /* Try block the action */
        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis, Bob prenas la 2 monerojn",
                                 MESSAGE_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_foreign_aid(void)
{
        return (test_accept_foreign_aid() &&
                test_accept_block_foreign_aid() &&
                test_failed_challenge_block_foreign_aid() &&
                test_failed_block_foreign_aid());
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_income() ||
            !test_coup() ||
            !test_foreign_aid())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
