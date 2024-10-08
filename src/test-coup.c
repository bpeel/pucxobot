/*
 * Pucxobot - A bot and website to play some card games
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "pcx-coup.h"
#include "pcx-list.h"
#include "pcx-main-context.h"
#include "test-message.h"

#define FIRST_ARG_TYPE 1000

enum arg_type {
        ARG_TYPE_STATUS = FIRST_ARG_TYPE,
        ARG_TYPE_SHOW_CARDS,
        ARG_TYPE_BUTTONS,
};

struct card_status {
        enum pcx_coup_character character;
        bool dead;
};

struct player_status {
        struct card_status cards[2];
        int coins;
        int allegiance;
};

struct status {
        struct player_status players[8];
        int current_player;
        int treasury;
};

struct test_data {
        struct pcx_coup *coup;
        struct test_message_data message_data;
        struct status status;
        bool use_inspector;
        bool reformation;
        int n_players;
};

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

static int
fake_random_number_generator(void)
{
        return 0x42424242;
}

static void
add_buttons_to_message(struct test_message *message,
                       va_list ap)
{
        test_message_enable_check_buttons(message);

        while (true) {
                const char *data = va_arg(ap, const char *);

                if (data == NULL)
                        break;

                const char *text = va_arg(ap, const char *);

                test_message_add_button(message, data, text);
        }
}

static char *
make_status_message(struct test_data *data)
{
        const struct status *status = &data->status;
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        int winner = -1;
        int n_alive_players = 0;

        for (unsigned i = 0; i < data->n_players; i++) {
                if (is_alive(status->players + i)) {
                        winner = i;
                        n_alive_players++;
                }
        }

        for (unsigned i = 0; i < data->n_players; i++) {
                const struct player_status *player = status->players + i;

                if (n_alive_players == 1) {
                        if (winner == i)
                                pcx_buffer_append_string(&buf, "🏆 ");
                } else if (status->current_player == i) {
                        pcx_buffer_append_string(&buf, "👉 ");
                }

                pcx_buffer_append_printf(&buf,
                                         "%s:\n",
                                         test_message_player_names[i]);

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

                        if (data->reformation) {
                                const char *sym =
                                        player->allegiance == 0 ?
                                        "👑" :
                                        "✊";
                                pcx_buffer_append_c(&buf, ' ');
                                pcx_buffer_append_string(&buf, sym);
                        }
                }

                pcx_buffer_append_string(&buf, "\n\n");
        }

        if (data->reformation) {
                pcx_buffer_append_printf(&buf,
                                         "Trezorejo: %i\n\n",
                                         status->treasury);
        }

        if (n_alive_players == 1) {
                pcx_buffer_append_printf(&buf,
                                         "%s venkis!",
                                         test_message_player_names[winner]);
        } else {
                const char *name =
                        test_message_player_names[status->current_player];
                pcx_buffer_append_printf(&buf,
                                         "%s, estas via vico, "
                                         "kion vi volas fari?",
                                         name);
        }


        return (char *) buf.data;
}

static void
add_status_buttons(struct test_data *data,
                   struct test_message *message)
{
        const struct status *status = &data->status;

        test_message_enable_check_buttons(message);

        int n_players = 0;

        for (unsigned i = 0; i < data->n_players; i++) {
                if (is_alive(status->players + i))
                        n_players++;
        }

        if (n_players <= 1)
                return;

        const struct player_status *player =
                status->players + status->current_player;

        if (player->coins >= 10) {
                test_message_add_button(message, "coup", "Puĉo");
                return;
        }

        test_message_add_button(message, "income", "Enspezi");
        test_message_add_button(message, "foreign_aid", "Eksterlanda helpo");

        if (player->coins >= 7)
                test_message_add_button(message, "coup", "Puĉo");

        if (data->reformation) {
                if (player->coins > 0) {
                        test_message_add_button(message,
                                              "convert",
                                              "Konverti");
                }
                test_message_add_button(message,
                                      "embezzle",
                                      "Ŝteli la trezoron");
        }

        test_message_add_button(message, "tax", "Imposto (Duko)");

        if (player->coins >= 3) {
                test_message_add_button(message,
                                      "assassinate",
                                      "Murdi (Murdisto)");
        }

        if (data->use_inspector) {
                test_message_add_button(message,
                                      "exchange",
                                      "Interŝanĝi (Inspektisto)");
                test_message_add_button(message,
                                      "inspect",
                                      "Inspekti (Inspektisto)");
        } else {
                test_message_add_button(message,
                                      "exchange",
                                      "Interŝanĝi (Ambasadoro)");
        }

        test_message_add_button(message, "steal", "Ŝteli (Kapitano)");
}

static void
handle_message_type(struct test_data *data,
                    struct test_message *message,
                    enum test_message_type type,
                    va_list ap)
{
        switch (type) {
        case TEST_MESSAGE_TYPE_PRIVATE:
                message->destination = va_arg(ap, int);
                message->message = pcx_strdup(va_arg(ap, const char *));
                return;
        case TEST_MESSAGE_TYPE_GLOBAL:
                message->message = pcx_strdup(va_arg(ap, const char *));
                return;
        case TEST_MESSAGE_TYPE_SIDEBAND_STRING:
                assert(!"sideband string arg used");
                return;
        case TEST_MESSAGE_TYPE_GAME_OVER:
                return;
        }

        assert(!"unexpected message type");
}

static void
handle_arg_type(struct test_data *data,
                struct test_message *message,
                enum arg_type type,
                va_list ap)
{
        switch (type) {
        case ARG_TYPE_STATUS:
                message->type = TEST_MESSAGE_TYPE_GLOBAL;
                message->message = make_status_message(data);
                add_status_buttons(data, message);
                return;
        case ARG_TYPE_SHOW_CARDS:
                message->type = TEST_MESSAGE_TYPE_PRIVATE;
                message->destination = va_arg(ap, int);
                message->message =
                        make_show_cards_message(data->status.players +
                                                message->destination);
                test_message_enable_check_buttons(message);
                return;
        case ARG_TYPE_BUTTONS:
                break;
        }

        assert(!"unexpected arg type");
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

                if (type == ARG_TYPE_BUTTONS) {
                        assert(!pcx_list_empty(&data->message_data.queue));
                        struct test_message *message =
                                pcx_container_of(data->message_data.queue.prev,
                                                 struct test_message,
                                                 link);
                        add_buttons_to_message(message, ap);
                        continue;
                }

                struct test_message *message =
                        test_message_queue(&data->message_data, type);

                if (type < FIRST_ARG_TYPE)
                        handle_message_type(data, message, type, ap);
                else
                        handle_arg_type(data, message, type, ap);
        }

        va_end(ap);

        pcx_coup_game.handle_callback_data_cb(data->coup,
                                              player_num,
                                              callback_data);

        return test_message_run_queue(&data->message_data);
}

static void
queue_configure_cards_message(struct test_data *data)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_GLOBAL);

        message->message =
                pcx_strdup("Bonvolu elekti kiun version de la ludo vi volas "
                           "ludi.");

        test_message_enable_check_buttons(message);

        test_message_add_button(message,
                              "game_type:0",
                              "Originala");
        test_message_add_button(message,
                              "game_type:1",
                              "Inspektisto");
        test_message_add_button(message,
                              "game_type:2",
                              "Reformacio");
        test_message_add_button(message,
                              "game_type:3",
                              "Reformacio + Inspektisto");
}

static struct test_data *
create_test_data(int n_card_overrides,
                 const enum pcx_coup_character *card_overrides,
                 bool use_inspector,
                 bool reformation,
                 int n_players)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        test_message_data_init(&data->message_data);

        data->status.current_player = 1;
        data->use_inspector = use_inspector;
        data->reformation = reformation;
        data->n_players = n_players;

        for (unsigned i = 0; i < n_players; i++) {
                struct player_status *player = data->status.players + i;

                player->coins = 2;
                player->allegiance = i & 1;

                for (unsigned j = 0; j < 2; j++)
                        player->cards[j].character = card_overrides[i * 2 + j];
        }

        if (n_players == 2)
                data->status.players[data->status.current_player].coins = 1;

        struct pcx_coup_debug_overrides overrides = {
                .n_cards = n_card_overrides,
                .cards = card_overrides,
                .start_player = 1,
                .rand_func = fake_random_number_generator,
        };

        queue_configure_cards_message(data);

        data->coup = pcx_coup_new(&test_message_callbacks,
                                  &data->message_data,
                                  PCX_TEXT_LANGUAGE_ESPERANTO,
                                  n_players,
                                  test_message_player_names,
                                  &overrides);

        const char *configure_data, *configure_note;

        if (use_inspector) {
                if (reformation) {
                        configure_data = "game_type:3";
                        configure_note =
                                "La elektita versio estas: "
                                "Inspektisto + Reformacio";
                } else {
                        configure_data = "game_type:1";
                        configure_note =
                                "La elektita versio estas: Inspektisto";
                }
        } else if (reformation) {
                configure_data = "game_type:2";
                configure_note = "La elektita versio estas: Reformacio";
        } else {
                configure_data = "game_type:0";
                configure_note = "La elektita versio estas: Originala";
        }

        struct test_message *note_message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_GLOBAL);
        note_message->message = pcx_strdup(configure_note);

        for (int i = 0; i < n_players; i++) {
                struct test_message *message =
                        test_message_queue(&data->message_data,
                                           TEST_MESSAGE_TYPE_PRIVATE);

                        message->destination = i;
                        message->message =
                                make_show_cards_message(data->status.players +
                                                        message->destination);
                        test_message_enable_check_buttons(message);
        }

        struct test_message *status_message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_PRIVATE);
        status_message->type = TEST_MESSAGE_TYPE_GLOBAL;
        status_message->message = make_status_message(data);
        add_status_buttons(data, status_message);

        send_callback_data(data,
                           0,
                           configure_data,
                           -1);

        return data;
}

static void
free_test_data(struct test_data *data)
{
        test_message_data_destroy(&data->message_data);

        pcx_coup_game.free_game_cb(data->coup);
        pcx_free(data);
}

static void
goto_next_player(struct test_data *data)
{
        int start_player = data->status.current_player;

        do {
                data->status.current_player =
                        (data->status.current_player + 1) % data->n_players;
        } while (!is_alive(data->status.players +
                           data->status.current_player) &&
                 data->status.current_player != start_player);
}

static bool
take_income(struct test_data *data)
{
        int active_player = data->status.current_player;

        data->status.players[active_player].coins++;
        goto_next_player(data);

        struct pcx_buffer message = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&message,
                                 "💲 %s enspezas 1 moneron",
                                 test_message_player_names[active_player]);

        bool ret = send_callback_data(data,
                                      active_player,
                                      "income",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      (char *) message.data,
                                      ARG_TYPE_STATUS,
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
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

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
                                 test_message_player_names[active_player],
                                 test_message_player_names[active_player ^ 1]);

        struct pcx_buffer callback_data = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&callback_data, "coup:%i", active_player ^ 1);

        bool ret = send_callback_data(data,
                                      active_player,
                                      (char *) callback_data.data,
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      (char *) message.data,
                                      TEST_MESSAGE_TYPE_PRIVATE,
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
        data->status.current_player =
                (data->status.current_player + 1) % data->n_players;

        ret = send_callback_data(data,
                                 active_player ^ 1,
                                 (char *) callback_data.data,
                                 ARG_TYPE_SHOW_CARDS,
                                 active_player ^ 1,
                                 ARG_TYPE_STATUS,
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
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

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
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💣 Bob faras puĉon kontraŭ Alice",
                                 ARG_TYPE_STATUS,
                                 TEST_MESSAGE_TYPE_GAME_OVER,
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
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "foreign_aid",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "💴 Bob prenas 2 monerojn per "
                                      "eksterlanda helpo.\n"
                                      "Ĉu iu volas pretendi havi la dukon "
                                      "kaj bloki rin?",
                                      ARG_TYPE_BUTTONS,
                                      "block", "Bloki",
                                      "accept", "Akcepti",
                                      NULL,
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
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "Neniu blokis, Bob prenas la 2 monerojn",
                                      ARG_TYPE_STATUS,
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
                                  TEST_MESSAGE_TYPE_GLOBAL,
                                  "Alice pretendas havi la dukon kaj "
                                  "blokas.\n"
                                  "Ĉu iu volas defii rin?",
                                  ARG_TYPE_BUTTONS,
                                  "challenge", "Defii",
                                  "accept", "Akcepti",
                                  NULL,
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
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj nun Alice elektas kiun karton "
                                 "montri.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Bob ne kredas ke vi havas la dukon.\n"
                                 "Kiun karton vi volas montri?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Duko",
                                 "reveal:1", "Kapitano",
                                 NULL,
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
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 ARG_TYPE_STATUS,
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
                PCX_COUP_CHARACTER_CONTESSA;

        /* Reveal the duke */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis sed Alice ja havis la dukon. "
                                 "Bob perdas karton kaj Alice ricevas "
                                 "novan anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Grafino",
                                 "lose:1", "Murdisto",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[0].dead = true;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 1,
                                 "lose:0",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 ARG_TYPE_STATUS,
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
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj Alice ne havis la dukon kaj "
                                 "Alice perdas karton",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💴 Bob prenas 2 monerojn per "
                                 "eksterlanda helpo.\n"
                                 "Ĉu iu volas pretendi havi la dukon "
                                 "kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
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
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis, Bob prenas la 2 monerojn",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_multiple_people_block_foreign_aid(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_ASSASSIN,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 3 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "foreign_aid",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "💴 Bob prenas 2 monerojn per "
                                      "eksterlanda helpo.\n"
                                      "Ĉu iu volas pretendi havi la dukon "
                                      "kaj bloki rin?",
                                      ARG_TYPE_BUTTONS,
                                      "block", "Bloki",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto done;

        /* Charles blocks */
        ret = send_callback_data(data,
                                 2,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Charles pretendas havi la dukon kaj blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Bob challenges the block */
        ret = send_callback_data(data,
                                 1,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj nun Charles elektas kiun "
                                 "karton montri.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 2,
                                 "Bob ne kredas ke vi havas la dukon.\n"
                                 "Kiun karton vi volas montri?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Kapitano",
                                 "reveal:1", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[2].cards[0].dead = true;

        /* Bob fails the challenge */
        ret = send_callback_data(data,
                                 2,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj Charles ne havis la dukon "
                                 "kaj Charles perdas karton",
                                 ARG_TYPE_SHOW_CARDS,
                                 2,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💴 Bob prenas 2 monerojn per "
                                 "eksterlanda helpo.\n"
                                 "Ĉu iu volas pretendi havi la dukon "
                                 "kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Now Alice can block */
        ret = send_callback_data(data,
                                 0,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice pretendas havi la dukon kaj blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Bob challenges the block again */
        ret = send_callback_data(data,
                                 1,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj nun Alice elektas kiun "
                                 "karton montri.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Bob ne kredas ke vi havas la dukon.\n"
                                 "Kiun karton vi volas montri?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Duko",
                                 "reveal:1", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[0].cards[0].character =
                PCX_COUP_CHARACTER_AMBASSADOR;

        /* This time Alice really does have the duke */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis sed Alice ja havis la dukon. "
                                 "Bob perdas karton kaj Alice ricevas "
                                 "novan anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Grafino",
                                 "lose:1", "Murdisto",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.current_player = 2;
        data->status.players[1].cards[1].dead = true;

        ret = send_callback_data(data,
                                 1,
                                 "lose:1",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 ARG_TYPE_STATUS,
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
                test_failed_block_foreign_aid() &&
                test_multiple_people_block_foreign_aid());
}

static struct test_data *
set_up_tax(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_ASSASSIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "tax",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "💸 Bob pretendas havi la dukon kaj "
                                      "prenas 3 monerojn per imposto.\n"
                                      "Ĉu iu volas defii rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret) {
                free_test_data(data);
                return NULL;
        }

        return data;
}

static bool
test_accept_tax(void)
{
        struct test_data *data = set_up_tax();

        if (data == NULL)
                return false;

        bool ret;

        /* Trying to block the tax should be silently ignored */
        ret = send_callback_data(data,
                                 0,
                                 "block",
                                 -1);
        if (!ret)
                goto done;

        data->status.current_player = 0;
        data->status.players[1].coins += 3;

        /* Accept the tax */
        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob prenas la 3 monerojn",
                                 ARG_TYPE_BUTTONS,
                                 NULL,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
challenge_tax(struct test_data *data)
{
        return send_callback_data(data,
                                  0,
                                  "challenge",
                                  TEST_MESSAGE_TYPE_GLOBAL,
                                  "Alice defiis kaj nun Bob elektas kiun "
                                  "karton montri.",
                                  TEST_MESSAGE_TYPE_PRIVATE,
                                  1,
                                  "Alice ne kredas ke vi havas la dukon.\n"
                                  "Kiun karton vi volas montri?",
                                  ARG_TYPE_BUTTONS,
                                  "reveal:0", "Duko",
                                  "reveal:1", "Murdisto",
                                  NULL,
                                  -1);
}

static bool
test_failed_challenge_tax(void)
{
        struct test_data *data = set_up_tax();

        if (data == NULL)
                return false;

        bool ret;

        ret = challenge_tax(data);
        if (!ret)
                goto done;

        data->status.players[1].cards[0].character =
                PCX_COUP_CHARACTER_CONTESSA;

        /* Reveal the duke */
        ret = send_callback_data(data,
                                 1,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis sed Bob ja havis la dukon. "
                                 "Alice perdas karton kaj Bob ricevas novan "
                                 "anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Grafino",
                                 "lose:1", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[0].cards[1].dead = true;
        data->status.current_player = 0;
        data->status.players[1].coins += 3;

        /* Lose a card */
        ret = send_callback_data(data,
                                 0,
                                 "lose:1",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob prenas la 3 monerojn",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_successful_challenge_tax(void)
{
        struct test_data *data = set_up_tax();

        if (data == NULL)
                return false;

        bool ret;

        ret = challenge_tax(data);
        if (!ret)
                goto done;

        data->status.players[1].cards[1].dead = true;
        data->status.current_player = 0;

        /* Reveal a card other than the duke */
        ret = send_callback_data(data,
                                 1,
                                 "reveal:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj Bob ne havis la dukon kaj "
                                 "Bob perdas karton",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);

        return ret;
}

static bool
test_tax(void)
{
        return (test_accept_tax() &&
                test_failed_challenge_tax() &&
                test_successful_challenge_tax());
}

static struct test_data *
set_up_assassinate(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_ASSASSIN,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CONTESSA,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        /* Give two coins to Bob and one to Alice so that it will be
         * her turn and she’ll have 3 coins.
         */
        for (int i = 0; i < 3; i++) {
                if (!take_income(data))
                        goto error;
        }

        assert(data->status.current_player == 0);
        assert(data->status.players[0].coins == 3);

        bool ret = send_callback_data(data,
                                      0,
                                      "assassinate:1",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "🗡 Alice volas murdi Bob\n"
                                      "Ĉu iu volas defii rin?\n"
                                      "Aŭ Bob, ĉu vi volas pretendi havi la "
                                      "grafinon kaj bloki rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "block", "Bloki",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_accept_assassinate(void)
{
        struct test_data *data = set_up_assassinate();

        if (data == NULL)
                return false;

        bool ret;

        ret = send_callback_data(data,
                                 1,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis aŭ defiis, Alice murdas Bob",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiun karton vi volas perdi?",
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[1].dead = true;
        data->status.players[0].coins = 0;
        data->status.current_player = 1;

        ret = send_callback_data(data,
                                 1,
                                 "lose:1",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        ret = take_income(data);
        if (!ret)
                goto done;

        /* Alice shouldn’t have any coins now so trying an
         * assassination should be ignored
         */
        ret = send_callback_data(data,
                                 0,
                                 "assassinate:1",
                                 -1);
        if (!ret)
                goto done;

        /* Give 3 coins to each player */
        for (int i = 0; i < 6; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        /* Kill again to end the game */
        ret = send_callback_data(data,
                                 0,
                                 "assassinate:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "🗡 Alice volas murdi Bob\n"
                                 "Ĉu iu volas defii rin?\n"
                                 "Aŭ Bob, ĉu vi volas pretendi havi la "
                                 "grafinon kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[0].dead = true;
        data->status.players[0].coins = 0;

        ret = send_callback_data(data,
                                 1,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis aŭ defiis, Alice murdas Bob",
                                 ARG_TYPE_STATUS,
                                 TEST_MESSAGE_TYPE_GAME_OVER,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static struct test_data *
set_up_block_and_challenge_assassinate(void)
{
        struct test_data *data = set_up_assassinate();

        if (data == NULL)
                return NULL;

        bool ret;

        ret = send_callback_data(data,
                                 1,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob pretendas havi la grafinon kaj "
                                 "blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 -1);
        if (!ret)
                goto error;

        ret = send_callback_data(data,
                                 0,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj nun Bob elektas kiun karton "
                                 "montri.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Alice ne kredas ke vi havas la grafinon.\n"
                                 "Kiun karton vi volas montri?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Duko",
                                 "reveal:1", "Grafino",
                                 NULL,
                                 -1);
        if (!ret)
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_block_assassinate(void)
{
        struct test_data *data = set_up_block_and_challenge_assassinate();

        if (data == NULL)
                return false;

        bool ret;

        ret = send_callback_data(data,
                                 1,
                                 "reveal:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis sed Bob ja havis la grafinon. "
                                 "Alice perdas karton kaj Bob ricevas novan "
                                 "anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Murdisto",
                                 "lose:1", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.current_player = 1;
        data->status.players[0].cards[1].dead = true;
        data->status.players[0].coins = 0;

        ret = send_callback_data(data,
                                 0,
                                 "lose:1",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_fail_block_assassinate(void)
{
        struct test_data *data = set_up_block_and_challenge_assassinate();

        if (data == NULL)
                return false;

        bool ret;

        data->status.players[1].cards[0].dead = true;

        ret = send_callback_data(data,
                                 1,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj Bob ne havis la grafinon "
                                 "kaj Bob perdas karton",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "🗡 Alice volas murdi Bob\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Let the assassination continue. This will kill Bob’s other
         * card and end the game.
         */

        data->status.players[1].cards[1].dead = true;
        data->status.players[0].coins = 0;

        ret = send_callback_data(data,
                                 1,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis aŭ defiis, Alice murdas Bob",
                                 ARG_TYPE_STATUS,
                                 TEST_MESSAGE_TYPE_GAME_OVER,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_dead_player_cant_block(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_ASSASSIN,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_DUKE,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 4 /* n_players */);

        bool ret;

        /* Give enough coins to everyone so they all have 7 */
        for (int i = 0; i < 20; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        assert(data->status.current_player == 1);
        assert(data->status.players[0].coins == 7);
        assert(data->status.players[1].coins == 7);
        assert(data->status.players[2].coins == 7);

        /* Everybody does a coup */
        for (int i = 0; i < 4; i++) {
                ret = do_coup(data);
                if (!ret)
                        goto done;
        }

        assert(data->status.current_player == 1);
        assert(data->status.players[0].cards[0].dead == true);
        assert(data->status.players[1].cards[0].dead == true);
        assert(data->status.players[2].cards[0].dead == true);
        assert(data->status.players[3].cards[0].dead == true);

        /* Give everybody 3 coins and skip to Alice’s go */
        for (int i = 0; i < 15; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        assert(data->status.current_player == 0);
        assert(data->status.players[0].coins == 3);

        /* Alice assassinates Bob’s last card */
        ret = send_callback_data(data,
                                 0,
                                 "assassinate:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "🗡 Alice volas murdi Bob\n"
                                 "Ĉu iu volas defii rin?\n"
                                 "Aŭ Bob, ĉu vi volas pretendi havi la "
                                 "grafinon kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[0].coins = 0;
        data->status.current_player = 2;
        data->status.players[1].cards[1].dead = true;

        /* Bob desperately tries to challenge. Alice does have the
         * assassin so this kills his last card. He shouldn’t be able
         * to block anymore and challenging is no longer available, so
         * the action should now happen.
         */
        ret = send_callback_data(data,
                                 1,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis sed Alice ja havis la murdiston. "
                                 "Bob perdas karton kaj Alice ricevas novan "
                                 "anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis aŭ defiis, Alice murdas Bob",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_assassinate(void)
{
        return (test_accept_assassinate() &&
                test_block_assassinate() &&
                test_fail_block_assassinate() &&
                test_dead_player_cant_block());
}

static bool
test_normal_exchange(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_AMBASSADOR,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "exchange",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "🔄 Bob pretendas havi la ambasadoron "
                                      "kaj volas interŝanĝi kartojn\n"
                                      "Ĉu iu volas defii rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob interŝanĝas kartojn",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiujn kartojn vi volas konservi?",
                                 ARG_TYPE_BUTTONS,
                                 "keep:0", "Duko",
                                 "keep:1", "Ambasadoro",
                                 "keep:2", "Grafino",
                                 "keep:3", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "keep:2",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiujn kartojn vi volas konservi?",
                                 ARG_TYPE_BUTTONS,
                                 "keep:0", "Duko",
                                 "keep:1", "Ambasadoro",
                                 "keep:2", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[0].character =
                PCX_COUP_CHARACTER_CONTESSA;
        data->status.players[1].cards[1].character =
                PCX_COUP_CHARACTER_CAPTAIN;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 0,
                                 "keep:2",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_one_dead_exchange(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_AMBASSADOR,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = true;

        /* Give 5 to Alice and 6 to Bob so that it will be her turn
         * and she can do a coup on Bob to kill one of his cards.
         */
        for (int i = 0; i < 11; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        assert(data->status.current_player == 0);
        assert(data->status.players[0].coins == 7);
        assert(data->status.players[1].coins == 7);

        if (!do_coup(data)) {
                ret = false;
                goto done;
        }

        ret = send_callback_data(data,
                                 1,
                                 "exchange",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "🔄 Bob pretendas havi la ambasadoron "
                                 "kaj volas interŝanĝi kartojn\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob interŝanĝas kartojn",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiujn kartojn vi volas konservi?",
                                 ARG_TYPE_BUTTONS,
                                 "keep:0", "Ambasadoro",
                                 "keep:1", "Grafino",
                                 "keep:2", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[1].character =
                PCX_COUP_CHARACTER_CONTESSA;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 0,
                                 "keep:1",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_exchange(void)
{
        return (test_normal_exchange() &&
                test_one_dead_exchange());
}

static bool
test_steal(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_AMBASSADOR,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "steal:0",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "💰 Bob volas ŝteli de Alice\n"
                                      "Ĉu iu volas defii rin?\n"
                                      "Aŭ Alice, ĉu vi volas pretendi havi la "
                                      "kapitanon aŭ la ambasadoron kaj bloki "
                                      "rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "block", "Bloki",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto done;

        data->status.players[0].coins = 0;
        data->status.players[1].coins = 3;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu blokis aŭ defiis, Bob ŝtelas de "
                                 "Alice",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "steal:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💰 Alice volas ŝteli de Bob\n"
                                 "Ĉu iu volas defii rin?\n"
                                 "Aŭ Bob, ĉu vi volas pretendi havi la "
                                 "kapitanon aŭ la ambasadoron kaj bloki "
                                 "rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 1,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob pretendas havi la kapitanon aŭ la "
                                 "ambasadoron kaj blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj nun Bob elektas kiun karton "
                                 "montri.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Alice ne kredas ke vi havas la kapitanon "
                                 "aŭ la ambasadoron.\n"
                                 "Kiun karton vi volas montri?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Duko",
                                 "reveal:1", "Ambasadoro",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[1].cards[1].character =
                PCX_COUP_CHARACTER_CONTESSA;

        ret = send_callback_data(data,
                                 1,
                                 "reveal:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis sed Bob ja havis "
                                 "la ambasadoron. Alice perdas karton kaj Bob "
                                 "ricevas novan anstataŭan karton.",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Duko",
                                 "lose:1", "Duko",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_exchange_inspector(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_INSPECTOR,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 true, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "exchange",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "🔄 Bob pretendas havi la inspektiston "
                                      "kaj volas interŝanĝi kartojn\n"
                                      "Ĉu iu volas defii rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob interŝanĝas kartojn",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiujn kartojn vi volas konservi?",
                                 ARG_TYPE_BUTTONS,
                                 "keep:0", "Duko",
                                 "keep:1", "Inspektisto",
                                 "keep:2", "Grafino",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "keep:2",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Kiujn kartojn vi volas konservi?",
                                 ARG_TYPE_BUTTONS,
                                 "keep:0", "Duko",
                                 "keep:1", "Inspektisto",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;


        data->status.players[1].cards[0].character =
                PCX_COUP_CHARACTER_CONTESSA;
        data->status.players[1].cards[1].character =
                PCX_COUP_CHARACTER_DUKE;
        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 0,
                                 "keep:0",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static struct test_data *
set_up_inspect_can_keep(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_INSPECTOR,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 true, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = send_callback_data(data,
                                      1,
                                      "inspect:0",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "🔍 Bob pretendas havi la inspektiston "
                                      "kaj volas inspekti karton de Alice\n"
                                      "Ĉu iu volas defii rin?",
                                      ARG_TYPE_BUTTONS,
                                      "challenge", "Defii",
                                      "accept", "Akcepti",
                                      NULL,
                                      -1);
        if (!ret)
                goto error;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Alice elektas karton por "
                                 "montri al Bob",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Kiun karton vi montros al Bob?",
                                 ARG_TYPE_BUTTONS,
                                 "show:0", "Duko",
                                 "show:1", "Inspektisto",
                                 NULL,
                                 -1);
        if (!ret)
                goto error;

        ret = send_callback_data(data,
                                 0,
                                 "show:1",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Nun Bob decidas ĉu vi rajtas konservi "
                                 "la inspektiston",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Alice montras al vi la inspektiston. Ĉu "
                                 "ri rajtas konservi ĝin?",
                                 ARG_TYPE_BUTTONS,
                                 "can_keep:1", "Jes",
                                 "can_keep:0", "Ne",
                                 NULL,
                                 -1);
        if (!ret)
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_inspect_keep(void)
{
        struct test_data *data =
                set_up_inspect_can_keep();

        if (data == NULL)
                return false;

        data->status.current_player = 0;

        bool ret = send_callback_data(data,
                                      1,
                                      "can_keep:1",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "Bob permesis Alice konservi la karton "
                                      "kiun ri montris.",
                                      ARG_TYPE_STATUS,
                                      -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_inspect_cant_keep(void)
{
        struct test_data *data =
                set_up_inspect_can_keep();

        if (data == NULL)
                return false;

        data->status.current_player = 0;
        data->status.players[0].cards[1].character =
                PCX_COUP_CHARACTER_CONTESSA;

        bool ret = send_callback_data(data,
                                      1,
                                      "can_keep:0",
                                      ARG_TYPE_SHOW_CARDS,
                                      0,
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "Bob devigis Alice ŝanĝi la karton "
                                      "kiun ri montris.",
                                      ARG_TYPE_STATUS,
                                      -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_inspect_one_card(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_INSPECTOR,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_CAPTAIN,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 true, /* use_inspector */
                                 false, /* reformation */
                                 2 /* n_players */);

        bool ret = true;

        /* Give 6 to each player so that Bob will have enough to kill
         * one of Alice’s cards and it will be his turn.
         */
        for (int i = 0; i < 12; i++) {
                if (!take_income(data)) {
                        ret = false;
                        goto done;
                }
        }

        assert(data->status.current_player == 1);
        assert(data->status.players[0].coins == 8);
        assert(data->status.players[1].coins == 7);

        if (!do_coup(data)) {
                ret = false;
                goto done;
        }

        /* Make it Bob’s turn again */
        if (!take_income(data)) {
                ret = false;
                goto done;
        }

        ret = send_callback_data(data,
                                 1,
                                 "inspect:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "🔍 Bob pretendas havi la inspektiston "
                                 "kaj volas inspekti karton de Alice\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Alice elektas karton por "
                                 "montri al Bob",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Alice montras al vi la inspektiston. Ĉu "
                                 "ri rajtas konservi ĝin?",
                                 ARG_TYPE_BUTTONS,
                                 "can_keep:1", "Jes",
                                 "can_keep:0", "Ne",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.current_player = 0;

        ret = send_callback_data(data,
                                 1,
                                 "can_keep:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob permesis Alice konservi la karton "
                                 "kiun ri montris.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_inspect(void)
{
        return (test_inspect_keep() &&
                test_inspect_cant_keep() &&
                test_inspect_one_card());
}

static struct test_data *
create_reformation_data(void)
{
        enum pcx_coup_character override_cards[] = {
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_CONTESSA,
                PCX_COUP_CHARACTER_AMBASSADOR,
                PCX_COUP_CHARACTER_ASSASSIN,
                PCX_COUP_CHARACTER_DUKE,
                PCX_COUP_CHARACTER_CAPTAIN,
                PCX_COUP_CHARACTER_CONTESSA,
        };

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(override_cards),
                                 override_cards,
                                 false, /* use_inspector */
                                 true, /* reformation */
                                 4 /* n_players */);

        return data;
}

static bool
test_convert(void)
{
        struct test_data *data = create_reformation_data();

        /* Convert yourself for one coin */
        data->status.players[1].coins = 1;
        data->status.players[1].allegiance = 0;
        data->status.current_player = 2;
        data->status.treasury = 1;

        bool ret = send_callback_data(data,
                                      1,
                                      "convert:1",
                                      TEST_MESSAGE_TYPE_GLOBAL,
                                      "Bob pagas 1 moneron al la trezorejo kaj "
                                      "konvertas sin mem.",
                                      ARG_TYPE_STATUS,
                                      -1);

        if (!ret)
                goto done;

        /* Convert Bob for two coins */
        data->status.players[2].coins = 0;
        data->status.players[1].allegiance = 1;
        data->status.current_player = 3;
        data->status.treasury = 3;

        ret = send_callback_data(data,
                                 2,
                                 "convert:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Charles pagas 2 monerojn al la "
                                 "trezorejo kaj konvertas Bob.",
                                 ARG_TYPE_STATUS,
                                 -1);

        if (!ret)
                goto done;

        /* Skip a few turns to make it Bob’s go */
        for (int i = 0; i < 2; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        /* Bob shouldn’t have enough coins to convert another player
         * so this will do nothing.
         */
        ret = send_callback_data(data,
                                 1,
                                 "convert:2",
                                 -1);

        if (!ret)
                goto done;

        /* He should however have enough coins to convert himself */
        data->status.players[1].coins = 0;
        data->status.players[1].allegiance = 0;
        data->status.current_player = 2;
        data->status.treasury = 4;

        ret = send_callback_data(data,
                                 1,
                                 "convert:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob pagas 1 moneron al la trezorejo kaj "
                                 "konvertas sin mem.",
                                 ARG_TYPE_STATUS,
                                 -1);

        if (!ret)
                goto done;

        /* Charles doesn’t have enough money to convert himself */
        ret = send_callback_data(data,
                                 2,
                                 "convert:2",
                                 -1);

        if (!ret)
                goto done;

        /* Skip to David’s go */
        ret = take_income(data);
        if (!ret)
                goto done;

        /* David can convert himself */
        data->status.players[3].coins = 2;
        data->status.players[3].allegiance = 0;
        data->status.current_player = 0;
        data->status.treasury = 5;

        ret = send_callback_data(data,
                                 3,
                                 "convert:3",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "David pagas 1 moneron al la trezorejo kaj "
                                 "konvertas sin mem.",
                                 ARG_TYPE_STATUS,
                                 -1);

        if (!ret)
                goto done;

        /* Everybody is on the same team, but conversion is still
         * allowed.
         */
        data->status.players[0].coins = 2;
        data->status.players[0].allegiance = 1;
        data->status.current_player= 1;
        data->status.treasury = 6;

        ret = send_callback_data(data,
                                 0,
                                 "convert:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice pagas 1 moneron al la trezorejo kaj "
                                 "konvertas sin mem.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_death_makes_reunification(void)
{
        struct test_data *data = create_reformation_data();

        bool ret;

        /* Give everybody 5 coins so that they can all do a coup */
        for (int i = 0; i < 20; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        /* Everybody does a coup to kill one card of each player */
        for (int i = 0; i < 4; i++) {
                ret = do_coup(data);
                if (!ret)
                        goto done;
        }

        /* Give everybody 7 coins so that they can all do a another coup */
        for (int i = 0; i < 28; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        assert(data->status.players[1].coins == 7);
        assert(data->status.players[3].coins == 7);
        assert(data->status.current_player == 1);

        /* Bob does an extra coup to kill Alice */
        data->status.players[1].coins = 0;
        data->status.current_player = 2;
        data->status.players[0].cards[1].dead = true;

        ret = send_callback_data(data,
                                 1,
                                 "coup:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💣 Bob faras puĉon kontraŭ Alice",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Skip to David’s go */
        ret = take_income(data);
        if (!ret)
                goto done;

        /* David does a coup against Charles. This causes reunification. */
        data->status.players[3].coins = 0;
        data->status.current_player = 1;
        data->status.players[2].cards[1].dead = true;

        ret = send_callback_data(data,
                                 3,
                                 "coup:2",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💣 David faras puĉon kontraŭ Charles",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Give everybody 7 coins again */
        for (int i = 0; i < 14; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        /* Bob can now kill David and finish the game */
        data->status.players[1].coins = 0;
        data->status.players[3].cards[1].dead = true;

        ret = send_callback_data(data,
                                 1,
                                 "coup:3",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💣 Bob faras puĉon kontraŭ David",
                                 ARG_TYPE_STATUS,
                                 TEST_MESSAGE_TYPE_GAME_OVER,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_targets(void)
{
        struct test_data *data = create_reformation_data();

        bool ret;

        /* Make sure the target buttons don’t include members of the
         * same allegiance.
         */
        ret = send_callback_data(data,
                                 1,
                                 "steal",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob, de kiu vi volas ŝteli?",
                                 ARG_TYPE_BUTTONS,
                                 "steal:0", "Alice",
                                 "steal:2", "Charles",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Try to target David. This shouldn’t do anything. */
        ret = send_callback_data(data,
                                 1,
                                 "steal:3",
                                 -1);
        if (!ret)
                goto done;

        /* But Bob can target Alice */
        ret = send_callback_data(data,
                                 1,
                                 "steal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💰 Bob volas ŝteli de Alice\n"
                                 "Ĉu iu volas defii rin?\n"
                                 "Aŭ Alice, ĉu vi volas pretendi havi la "
                                 "kapitanon aŭ la ambasadoron kaj bloki "
                                 "rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_block_other_allegiance(void)
{
        struct test_data *data = create_reformation_data();

        bool ret;

        /* Bob tries to take foreign aid */
        ret = send_callback_data(data,
                                 1,
                                 "foreign_aid",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💴 Bob prenas 2 monerojn per eksterlanda "
                                 "helpo.\n"
                                 "Ĉu iu de alia partio volas pretendi havi "
                                 "la dukon kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* David can not block */
        ret = send_callback_data(data,
                                 3,
                                 "block",
                                 -1);

        /* But Alice can */
        ret = send_callback_data(data,
                                 0,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice pretendas havi la dukon kaj blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 -1);
        if (!ret)
                goto done;

        /* Everybody accepts */
        for (int i = 1; i < 3; i++) {
                ret = send_callback_data(data,
                                         i,
                                         "accept",
                                         -1);
                if (!ret)
                        goto done;
        }

        data->status.current_player = 2;

        ret = send_callback_data(data,
                                 3,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis. La ago estis blokita.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[2].allegiance = 1;
        data->status.current_player = 3;
        data->status.players[2].coins = 1;
        data->status.treasury = 1;

        /* Reunify */
        ret = send_callback_data(data,
                                 2,
                                 "convert:2",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Charles pagas 1 moneron al la trezorejo kaj "
                                 "konvertas sin mem.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[0].allegiance = 1;
        data->status.players[3].coins = 0;
        data->status.current_player = 0;
        data->status.treasury = 3;

        ret = send_callback_data(data,
                                 3,
                                 "convert:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "David pagas 2 monerojn al la trezorejo kaj "
                                 "konvertas Alice.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Check that the blocking message is different after reunification */
        ret = send_callback_data(data,
                                 0,
                                 "foreign_aid",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💴 Alice prenas 2 monerojn per eksterlanda "
                                 "helpo.\n"
                                 "Ĉu iu volas pretendi havi "
                                 "la dukon kaj bloki rin?",
                                 ARG_TYPE_BUTTONS,
                                 "block", "Bloki",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* David can now block */
        ret = send_callback_data(data,
                                 3,
                                 "block",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "David pretendas havi la dukon kaj blokas.\n"
                                 "Ĉu iu volas defii rin?",
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_embezzlement(void)
{
        struct test_data *data = create_reformation_data();

        bool ret;

        /* Bob converts to put some money in the treasury */
        data->status.players[1].allegiance = 0;
        data->status.current_player = 2;
        data->status.players[1].coins = 1;
        data->status.treasury = 1;

        ret = send_callback_data(data,
                                 1,
                                 "convert:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob pagas 1 moneron al la trezorejo kaj "
                                 "konvertas sin mem.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Charles takes the money */
        ret = send_callback_data(data,
                                 2,
                                 "embezzle",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💼 Charles pretendas ne havi la dukon kaj "
                                 "ŝtelas la monon de la trezorejo.\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Everybody accepts */
        for (int i = 0; i < 2; i++) {
                ret = send_callback_data(data,
                                         (i + 3) % 4,
                                         "accept",
                                         -1);
                if (!ret)
                        goto done;
        }

        data->status.current_player = 3;
        data->status.treasury = 0;
        data->status.players[2].coins = 3;

        ret = send_callback_data(data,
                                 1,
                                 "accept",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Charles prenas la monon de la "
                                 "trezorejo.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* David converts Bob back again to put some money in the treasury */
        data->status.players[1].allegiance = 1;
        data->status.current_player = 0;
        data->status.players[3].coins = 0;
        data->status.treasury = 2;

        ret = send_callback_data(data,
                                 3,
                                 "convert:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "David pagas 2 monerojn al la trezorejo kaj "
                                 "konvertas Bob.",
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Test successful challenge */
        ret = send_callback_data(data,
                                 0,
                                 "embezzle",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💼 Alice pretendas ne havi la dukon kaj "
                                 "ŝtelas la monon de la trezorejo.\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 1,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj nun Alice elektas ĉu cedi.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Bob kredas ke vi ja havas la dukon.\n"
                                 "Ĉu vi volas cedi?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Cedi",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        /* Alice has the duke so trying to show her cards should do
         * nothing.
         */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:1",
                                 -1);
        if (!ret)
                goto done;

        /* She has to concede */
        ret = send_callback_data(data,
                                 0,
                                 "reveal:0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob defiis kaj Alice cedis do Alice perdas "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Kiun karton vi volas perdi?",
                                 ARG_TYPE_BUTTONS,
                                 "lose:0", "Duko",
                                 "lose:1", "Kapitano",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.players[0].cards[0].dead = true;
        data->status.current_player = 1;

        ret = send_callback_data(data,
                                 0,
                                 "lose:0",
                                 ARG_TYPE_SHOW_CARDS,
                                 0,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

        /* Test unsuccessful challenge */
        ret = send_callback_data(data,
                                 1,
                                 "embezzle",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "💼 Bob pretendas ne havi la dukon kaj "
                                 "ŝtelas la monon de la trezorejo.\n"
                                 "Ĉu iu volas defii rin?",
                                 ARG_TYPE_BUTTONS,
                                 "challenge", "Defii",
                                 "accept", "Akcepti",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        ret = send_callback_data(data,
                                 0,
                                 "challenge",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj nun Bob elektas ĉu cedi.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Alice kredas ke vi ja havas la dukon.\n"
                                 "Ĉu vi volas cedi?",
                                 ARG_TYPE_BUTTONS,
                                 "reveal:0", "Cedi",
                                 "reveal:1", "Montri kartojn",
                                 NULL,
                                 -1);
        if (!ret)
                goto done;

        data->status.current_player = 2;
        data->status.players[0].cards[1].dead = true;
        data->status.treasury = 0;
        data->status.players[1].coins = 3;
        data->status.players[1].cards[0].character =
                PCX_COUP_CHARACTER_DUKE;
        data->status.players[1].cards[1].character =
                PCX_COUP_CHARACTER_ASSASSIN;

        ret = send_callback_data(data,
                                 1,
                                 "reveal:1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice defiis kaj Bob montris la grafinon kaj "
                                 "la ambasadoron do Bob ŝanĝas siajn kartojn "
                                 "kaj Alice perdas karton.",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Neniu defiis, Bob prenas la monon de "
                                 "la trezorejo.",
                                 ARG_TYPE_SHOW_CARDS,
                                 1,
                                 ARG_TYPE_STATUS,
                                 -1);
        if (!ret)
                goto done;

done:
        free_test_data(data);
        return ret;
}

static bool
test_reformation(void)
{
        return (test_convert() &&
                test_death_makes_reunification() &&
                test_targets() &&
                test_block_other_allegiance() &&
                test_embezzlement());
}

static bool
test_max_players(void)
{
        enum pcx_coup_character card_overrides[8 * 2];

        for (int i = 0; i < PCX_N_ELEMENTS(card_overrides); i++)
                card_overrides[i] = i % 5;

        struct test_data *data =
                create_test_data(PCX_N_ELEMENTS(card_overrides),
                                 card_overrides,
                                 false, /* use_inspector */
                                 false, /* reformation */
                                 8 /* n_players */);

        bool ret = true;

        /* Give everybody 5 coins */
        for (int i = 0; i < 5 * 8; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        /* Everybody do a coup on their partner */
        for (int i = 0; i < 8; i++) {
                ret = do_coup(data);
                if (!ret)
                        goto done;
        }

        /* Give everybody 7 coins */
        for (int i = 0; i < 7 * 8; i++) {
                ret = take_income(data);
                if (!ret)
                        goto done;
        }

        /* Everybody kill the player before them */
        for (int i = 0; i < 7; i++) {
                int killer = data->status.current_player;
                int killee = (killer + 7) % 8;

                struct pcx_buffer message = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&message,
                                         "💣 %s faras puĉon kontraŭ %s",
                                         test_message_player_names[killer],
                                         test_message_player_names[killee]);

                struct pcx_buffer callback_data = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&callback_data, "coup:%i", killee);

                data->status.current_player = (killer + 1) % 8;
                data->status.players[killee].cards[1].dead = true;
                data->status.players[killer].coins = 0;

                bool ret;

                if (i == 6) {
                        ret = send_callback_data(data,
                                                 killer,
                                                 (char *) callback_data.data,
                                                 TEST_MESSAGE_TYPE_GLOBAL,
                                                 (char *) message.data,
                                                 ARG_TYPE_STATUS,
                                                 TEST_MESSAGE_TYPE_GAME_OVER,
                                                 -1);
                } else {
                        ret = send_callback_data(data,
                                                 killer,
                                                 (char *) callback_data.data,
                                                 TEST_MESSAGE_TYPE_GLOBAL,
                                                 (char *) message.data,
                                                 ARG_TYPE_STATUS,
                                                 -1);
                }

                pcx_buffer_destroy(&message);
                pcx_buffer_destroy(&callback_data);

                if (!ret)
                        goto done;
        }

done:
        free_test_data(data);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_income() ||
            !test_coup() ||
            !test_foreign_aid() ||
            !test_tax() ||
            !test_assassinate() ||
            !test_steal() ||
            !test_exchange() ||
            !test_exchange_inspector() ||
            !test_inspect() ||
            !test_reformation() ||
            !test_max_players())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
