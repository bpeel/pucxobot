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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "pcx-fox.h"
#include "pcx-list.h"
#include "pcx-main-context.h"
#include "test-message.h"

#define FIRST_ARG_TYPE 1000

enum arg_type {
        ARG_TYPE_BUTTONS = FIRST_ARG_TYPE,
        ARG_TYPE_LEADER_CHOICE,
        ARG_TYPE_FOLLOW_CHOICE,
        ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
};

#define N_SUITS 3
#define MAX_VALUE 11

static const char *
suit_symbols[] = {
        [0] = "🔔",
        [1] = "🗝",
        [2] = "🌜",
};

static const char *
value_symbols[] = {
        [1] = "🔼",
        [3] = "🔄",
        [5] = "📤",
        [7] = "💎",
        [9] = "🎩",
        [11] = "↕️",
};

struct player {
        uint16_t hand[N_SUITS];
};

struct test_data {
        struct pcx_fox *fox;
        struct test_message_data message_data;
        struct player players[2];
};

static void
add_card_to_hand(struct player *player,
                 int suit,
                 int value)
{
        player->hand[suit] |= 1 << value;
}

static void
remove_card_from_hand(struct player *player,
                      int suit,
                      int value)
{
        player->hand[suit] &= ~(1 << value);
}

static bool
has_card(const struct player *player,
         int suit,
         int value)
{
        return player->hand[suit] & (1 << value);
}

static int
fake_random_number_generator(void)
{
        /* This ends up making the deck rotated by one so that the
         * first card (1 bells) becomes the top card and from the
         * bottom up the remaining cards are in order.
         *
         * Therefore Alice will have 1 bells, all the moons and 11 keys.
         * Bob will 1-10 keys and 9-11 bells.
         * The trump card will be 8 bells.
         *
         * The first dealer is Alice so Bob will go first.
         */
        return 0;
}

static void
add_card(struct pcx_buffer *buf,
         int suit,
         int value)
{
        pcx_buffer_append_printf(buf, "%s%i", suit_symbols[suit], value);

        if (value & 1)
                pcx_buffer_append_string(buf, value_symbols[value]);

}

static void
add_card_button(struct test_message *message,
                const char *keyword,
                int suit,
                int value)
{
        struct pcx_buffer data = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&data,
                                 "%s:%i",
                                 keyword,
                                 (suit << 4) | value);

        struct pcx_buffer label = PCX_BUFFER_STATIC_INIT;

        add_card(&label, suit, value);

        test_message_add_button(message,
                                (char *) data.data,
                                (char *) label.data);

        pcx_buffer_destroy(&label);
        pcx_buffer_destroy(&data);
}

static void
add_card_list(struct pcx_buffer *buf,
              const struct player *player)
{
        for (int suit = 0; suit < N_SUITS; suit++) {
                bool had_card = false;

                for (int value = 1; value <= MAX_VALUE; value++) {
                        if (!has_card(player, suit, value))
                                continue;

                        if (had_card)
                                pcx_buffer_append_string(buf, " ");

                        had_card = true;

                        add_card(buf, suit, value);
                }

                if (had_card)
                        pcx_buffer_append_string(buf, "\n");
        }
}

static char *
make_card_choice_question(const struct player *player,
                          bool leader)
{
        struct pcx_buffer note = PCX_BUFFER_STATIC_INIT;

        if (!leader) {
                pcx_buffer_append_string(&note, "Viaj kartoj estas:\n");
                add_card_list(&note, player);
                pcx_buffer_append_string(&note, "\n");
        }

        pcx_buffer_append_string(&note, "Kiun karton vi volas ludi?");

        return (char *) note.data;
}

static void
add_card_choice_buttons_for_suit(struct test_message *message,
                                 int suit,
                                 const struct player *player)
{
        for (int value = 1; value <= MAX_VALUE; value++) {
                if (has_card(player, suit, value))
                        add_card_button(message, "play", suit, value);
        }
}

static void
add_card_choice_buttons(struct test_message *message,
                        const struct player *player)
{
        test_message_enable_check_buttons(message);

        for (int suit = 0; suit < N_SUITS; suit++)
                add_card_choice_buttons_for_suit(message, suit, player);
}

static struct test_message *
add_card_question(struct test_data *data,
                  int player_num,
                  bool leader)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_PRIVATE);

        message->destination = player_num;
        message->message = make_card_choice_question(data->players + player_num,
                                                     leader);

        add_card_choice_buttons(message, data->players + player_num);

        return message;
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
        case ARG_TYPE_BUTTONS:
                break;
        case ARG_TYPE_LEADER_CHOICE:
                message->type = TEST_MESSAGE_TYPE_PRIVATE;
                message->destination = va_arg(ap, int);
                message->message =
                        make_card_choice_question(data->players +
                                                  message->destination,
                                                  true /* leader */);
                add_card_choice_buttons(message,
                                        data->players + message->destination);
                return;
        case ARG_TYPE_FOLLOW_CHOICE:
                message->type = TEST_MESSAGE_TYPE_PRIVATE;
                message->destination = va_arg(ap, int);
                message->message =
                        make_card_choice_question(data->players +
                                                  message->destination,
                                                  false /* leader */);
                test_message_enable_check_buttons(message);
                add_card_choice_buttons_for_suit(message,
                                                 va_arg(ap, int),
                                                 data->players +
                                                 message->destination);
                return;
        case ARG_TYPE_UNLIMITED_FOLLOW_CHOICE:
                message->type = TEST_MESSAGE_TYPE_PRIVATE;
                message->destination = va_arg(ap, int);
                message->message =
                        make_card_choice_question(data->players +
                                                  message->destination,
                                                  false /* leader */);
                add_card_choice_buttons(message,
                                        data->players + message->destination);
                return;
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

        pcx_fox_game.handle_callback_data_cb(data->fox,
                                             player_num,
                                             callback_data);

        return test_message_run_queue(&data->message_data);
}

static struct test_data *
create_test_data(void)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        test_message_data_init(&data->message_data);

        struct pcx_fox_debug_overrides overrides = {
                .rand_func = fake_random_number_generator,
        };

        add_card_to_hand(data->players + 0, 0, 1);
        add_card_to_hand(data->players + 0, 1, 11);
        for (int i = 1; i <= 11; i++)
                add_card_to_hand(data->players + 0, 2, i);

        for (int i = 9; i <= 11; i++)
        add_card_to_hand(data->players + 1, 0, i);
        for (int i = 1; i <= 10; i++)
                add_card_to_hand(data->players + 1, 1, i);

        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_GLOBAL);

        message->message = pcx_strdup("La dekreta karto estas: 🔔8\n"
                                      "\n"
                                      "Bob komencas la prenvicon.");

        add_card_question(data, 1, true /* leader */);

        data->fox = pcx_fox_new(&test_message_callbacks,
                                &data->message_data,
                                PCX_TEXT_LANGUAGE_ESPERANTO,
                                2, /* n_players */
                                test_message_player_names,
                                &overrides);

        return data;
}

static void
free_test_data(struct test_data *data)
{
        test_message_data_destroy(&data->message_data);

        pcx_fox_game.free_game_cb(data->fox);
        pcx_free(data);
}

static bool
test_trump_suit(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        remove_card_from_hand(data->players + 1, 1, 10);

        /* Play a regular hand to make player 1 lead the next one */
        ret = send_callback_data(data,
                                 1,
                                 "play:26", /* 10 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝10",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Alice elektas kiun karton ludi.",
                                 ARG_TYPE_FOLLOW_CHOICE,
                                 0,
                                 1, /* keys */
                                 -1);
        if (!ret)
                goto out;

        remove_card_from_hand(data->players + 0, 1, 11);

        ret = send_callback_data(data,
                                 0,
                                 "play:27", /* 11 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🗝11↕️",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 1\n"
                                 "Bob: 0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Alice komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 0,
                                 -1);
        if (!ret)
                goto out;

        /* Alice plays a moon. Bob doesn’t have any of these so he is
         * free to play any card.
         */
        remove_card_from_hand(data->players + 0, 2, 2);

        ret = send_callback_data(data,
                                 0,
                                 "play:34", /* 2 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜2",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;


        /* Bob is free to play a bell card which is the trump suit so
         * he should win the trick.
         */
        remove_card_from_hand(data->players + 1, 0, 10);

        ret = send_callback_data(data,
                                 1,
                                 "play:10", /* 10 bells */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🔔10",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 1\n"
                                 "Bob: 1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Bob komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;

out:
        free_test_data(data);

        return ret;
}

static bool
test_lose_but_lead(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        remove_card_from_hand(data->players + 1, 1, 1);

        ret = send_callback_data(data,
                                 1,
                                 "play:17", /* 1 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝1🔼",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Alice elektas kiun karton ludi.",
                                 ARG_TYPE_FOLLOW_CHOICE,
                                 0,
                                 1, /* keys */
                                 -1);
        if (!ret)
                goto out;

        remove_card_from_hand(data->players + 0, 1, 11);

        /* Alice players a higher keys card and wins the trick, but
         * Bob leads the next trick anyway because he played the 1.
         */
        ret = send_callback_data(data,
                                 0,
                                 "play:27", /* 11 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🗝11↕️",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 1\n"
                                 "Bob: 0",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Bob komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;

out:
        free_test_data(data);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_trump_suit() ||
            !test_lose_but_lead())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
