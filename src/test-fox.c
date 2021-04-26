/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020, 2021  Neil Roberts
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
add_card_command(struct pcx_buffer *buf,
                 const char *keyword,
                 int suit,
                 int value)
{
        pcx_buffer_append_printf(buf,
                                 "%s:%i",
                                 keyword,
                                 (suit << 4) | value);
}

static void
add_card_button(struct test_message *message,
                const char *keyword,
                int suit,
                int value)
{
        struct pcx_buffer data = PCX_BUFFER_STATIC_INIT;

        add_card_command(&data, keyword, suit, value);

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

static void
deal_cards(struct test_data *data)
{
        for (int i = 0; i < 2; i++)
                memset(data->players[i].hand, 0, sizeof data->players[i].hand);

        add_card_to_hand(data->players + 0, 0, 1);
        add_card_to_hand(data->players + 0, 1, 11);
        for (int i = 1; i <= 11; i++)
                add_card_to_hand(data->players + 0, 2, i);

        for (int i = 9; i <= 11; i++)
        add_card_to_hand(data->players + 1, 0, i);
        for (int i = 1; i <= 10; i++)
                add_card_to_hand(data->players + 1, 1, i);
}

static struct test_data *
create_test_data(void)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        deal_cards(data);

        test_message_data_init(&data->message_data);

        struct pcx_fox_debug_overrides overrides = {
                .rand_func = fake_random_number_generator,
        };

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
make_alice_lead(struct test_data *data)
{
        bool ret = true;

        remove_card_from_hand(data->players + 1, 1, 10);

        /* Play a regular hand to make Alice lead the next one */
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
                return false;

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
                return false;

        return true;
}

static bool
test_trump_suit(void)
{
        bool ret = true;

        struct test_data *data = create_test_data();

        ret = make_alice_lead(data);

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

static bool
test_exchange(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        remove_card_from_hand(data->players + 1, 1, 3);

        ret = send_callback_data(data,
                                 1,
                                 "play:19", /* 3 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝3🔄\n"
                                 "\n"
                                 "Nun ri elektas ĉu interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Kiun karton vi volas meti kiel la dekretan "
                                 "karton?",
                                 ARG_TYPE_BUTTONS,
                                 "exchange:8",
                                 "Lasi la antaŭan dekretan karton",
                                 "exchange:9", "🔔9🎩",
                                 "exchange:10", "🔔10",
                                 "exchange:11", "🔔11↕️",
                                 "exchange:17", "🗝1🔼",
                                 "exchange:18", "🗝2",
                                 "exchange:20", "🗝4",
                                 "exchange:21", "🗝5📤",
                                 "exchange:22", "🗝6",
                                 "exchange:23", "🗝7💎",
                                 "exchange:24", "🗝8",
                                 "exchange:25", "🗝9🎩",
                                 "exchange:26", "🗝10",
                                 NULL,
                                 -1);
        if (!ret)
                goto out;

        /* Try picking a card that Bob doesn’t have. This should just
         * be ignored.
         */
        ret = send_callback_data(data,
                                 1,
                                 "exchange:19",
                                 -1);
        if (!ret)
                goto out;

        /* Leave the trump card alone */
        ret = send_callback_data(data,
                                 1,
                                 "exchange:8",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob decidis ne interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Alice elektas kiun karton ludi.",
                                 ARG_TYPE_FOLLOW_CHOICE,
                                 0,
                                 1, /* keys */
                                 -1);
        if (!ret)
                goto out;

        /* Play a winning keys card so that Alice can lead next */
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

        remove_card_from_hand(data->players + 0, 2, 3);

        ret = send_callback_data(data,
                                 0,
                                 "play:35", /* 3 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜3🔄\n"
                                 "\n"
                                 "Nun ri elektas ĉu interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Kiun karton vi volas meti kiel la dekretan "
                                 "karton?",
                                 ARG_TYPE_BUTTONS,
                                 "exchange:8",
                                 "Lasi la antaŭan dekretan karton",
                                 "exchange:1", "🔔1🔼",
                                 "exchange:33", "🌜1🔼",
                                 "exchange:34", "🌜2",
                                 "exchange:36", "🌜4",
                                 "exchange:37", "🌜5📤",
                                 "exchange:38", "🌜6",
                                 "exchange:39", "🌜7💎",
                                 "exchange:40", "🌜8",
                                 "exchange:41", "🌜9🎩",
                                 "exchange:42", "🌜10",
                                 "exchange:43", "🌜11↕️",
                                 NULL,
                                 -1);
        if (!ret)
                goto out;

        /* Change the trump card to 7 moons */
        ret = send_callback_data(data,
                                 0,
                                 "exchange:39", /* 7 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🌜7💎",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;

out:
        free_test_data(data);

        return ret;
}

static bool
test_draw_card(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        remove_card_from_hand(data->players + 1, 1, 5);

        ret = send_callback_data(data,
                                 1,
                                 "play:21", /* 5 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝5📤\n"
                                 "\n"
                                 "Nun ri prenas karton de la kartaro kaj "
                                 "forĵetas unu.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "Vi prenas: 🔔7💎\n"
                                 "\n"
                                 "Kiun karton vi volas forĵeti?",
                                 ARG_TYPE_BUTTONS,
                                 "discard:7", "🔔7💎", /* card just drawn */
                                 "discard:9", "🔔9🎩",
                                 "discard:10", "🔔10",
                                 "discard:11", "🔔11↕️",
                                 "discard:17", "🗝1🔼",
                                 "discard:18", "🗝2",
                                 "discard:19", "🗝3🔄",
                                 "discard:20", "🗝4",
                                 "discard:22", "🗝6",
                                 "discard:23", "🗝7💎",
                                 "discard:24", "🗝8",
                                 "discard:25", "🗝9🎩",
                                 "discard:26", "🗝10",
                                 NULL,
                                 -1);
        if (!ret)
                goto out;

        /* Try picking a card that Bob doesn’t have. This should just
         * be ignored.
         */
        ret = send_callback_data(data,
                                 1,
                                 "discard:21",
                                 -1);
        if (!ret)
                goto out;

        /* Try playing a card. This should just be ignored. */
        ret = send_callback_data(data,
                                 1,
                                 "play:21",
                                 -1);
        if (!ret)
                goto out;

        /* Return the card that was just picked up */
        ret = send_callback_data(data,
                                 1,
                                 "discard:7",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Alice elektas kiun karton ludi.",
                                 ARG_TYPE_FOLLOW_CHOICE,
                                 0,
                                 1, /* keys */
                                 -1);
        if (!ret)
                goto out;

        /* Play a winning keys card so that Alice can lead next */
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

        remove_card_from_hand(data->players + 0, 2, 5);

        ret = send_callback_data(data,
                                 0,
                                 "play:37", /* 5 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜5📤\n"
                                 "\n"
                                 "Nun ri prenas karton de la kartaro kaj "
                                 "forĵetas unu.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "Vi prenas: 🔔6\n"
                                 "\n"
                                 "Kiun karton vi volas forĵeti?",
                                 ARG_TYPE_BUTTONS,
                                 "discard:1", "🔔1🔼",
                                 "discard:6", "🔔6", /* card just drawn */
                                 "discard:33", "🌜1🔼",
                                 "discard:34", "🌜2",
                                 "discard:35", "🌜3🔄",
                                 "discard:36", "🌜4",
                                 "discard:38", "🌜6",
                                 "discard:39", "🌜7💎",
                                 "discard:40", "🌜8",
                                 "discard:41", "🌜9🎩",
                                 "discard:42", "🌜10",
                                 "discard:43", "🌜11↕️",
                                 NULL,
                                 -1);
        if (!ret)
                goto out;

        /* Discard 7 moons */
        remove_card_from_hand(data->players + 0, 2, 7);
        add_card_to_hand(data->players + 0, 0, 6);

        ret = send_callback_data(data,
                                 0,
                                 "discard:39", /* 7 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;

        /* Let Bob play a card so we can verify that Alice’s hand is correct */
        /* Return the card that was just picked up */
        ret = send_callback_data(data,
                                 1,
                                 "play:18", /* 2 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝2",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 2\n"
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

out:
        free_test_data(data);

        return ret;
}

static bool
test_become_trump(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        ret = make_alice_lead(data);

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

        /* Bob plays 9 keys which is treated as if it were the 9 of
         * bells and he wins the trick.
         */
        remove_card_from_hand(data->players + 1, 1, 9);

        ret = send_callback_data(data,
                                 1,
                                 "play:25", /* 9 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝9🎩",
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
test_two_become_trumps(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        ret = make_alice_lead(data);

        if (!ret)
                goto out;

        /* Alice plays a moon become trump card. Bob doesn’t have any
         * moons so he is free to play any card.
         */
        remove_card_from_hand(data->players + 0, 2, 9);

        ret = send_callback_data(data,
                                 0,
                                 "play:41", /* 9 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜9🎩",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                goto out;

        /* Bob plays 9 keys. This doesn’t get treated as the 9 of
         * bells so Alice wins the trick with the lead suit.
         */
        remove_card_from_hand(data->players + 1, 1, 9);

        ret = send_callback_data(data,
                                 1,
                                 "play:25", /* 9 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝9🎩",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 2\n"
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

out:
        free_test_data(data);

        return ret;
}

static bool
test_force_best_card(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        remove_card_from_hand(data->players + 1, 0, 11);

        char *card_question =
                make_card_choice_question(data->players + 0, false);

        /* Bob plays the 11 of bells. Alice only has the 1 of keys so
         * this doesn’t change anything.
         */
        ret = send_callback_data(data,
                                 1,
                                 "play:11", /* 11 bells */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🔔11↕️",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Alice elektas kiun karton ludi.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 card_question,
                                 ARG_TYPE_BUTTONS,
                                 "play:1", "🔔1🔼",
                                 NULL,
                                 -1);

        pcx_free(card_question);

        if (!ret)
                goto out;

        remove_card_from_hand(data->players + 0, 0, 1);

        ret = send_callback_data(data,
                                 0,
                                 "play:1", /* 1 bells */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🔔1🔼",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 0\n"
                                 "Bob: 1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Alice komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 0,
                                 -1);
        if (!ret)
                goto out;

        remove_card_from_hand(data->players + 0, 1, 11);

        card_question = make_card_choice_question(data->players + 1, false);

        /* Alice plays the 11 of keys. Bob has 10 of these but can
         * only choose between two of them.
         */
        ret = send_callback_data(data,
                                 0,
                                 "play:27", /* 11 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🗝11↕️",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 card_question,
                                 ARG_TYPE_BUTTONS,
                                 "play:17", "🗝1🔼",
                                 "play:26", "🗝10",
                                 NULL,
                                 -1);

        pcx_free(card_question);

        if (!ret)
                goto out;

        /* Try to play a card that isn’t one of the two. This should
         * be ignored.
         */
        ret = send_callback_data(data,
                                 1,
                                 "play:20", /* 4 keys */
                                 -1);
        if (!ret)
                goto out;

        remove_card_from_hand(data->players + 0, 0, 1);

        ret = send_callback_data(data,
                                 1,
                                 "play:26", /* 10 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝10",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 1\n"
                                 "Bob: 1",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Alice komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 0,
                                 -1);
        if (!ret)
                goto out;

out:
        free_test_data(data);

        return ret;
}

static bool
play_even_cards(struct test_data *data)
{
        bool ret;

        /* Play all the even cards of each suit where the player has
         * all of them.
         */
        for (int i = 0; i < 5; i++) {
                int card_value = (i + 1) * 2;
                struct pcx_buffer command_buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&command_buf,
                                         /* play a keys card */
                                         "play:%i",
                                         card_value + 16);

                struct pcx_buffer card_buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&card_buf,
                                         "Bob ludis: 🗝%i",
                                         card_value);

                remove_card_from_hand(data->players + 1, 1, card_value);

                ret = send_callback_data(data,
                                         1,
                                         (char *) command_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         card_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "Nun Alice elektas kiun karton ludi.",
                                         ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                         0,
                                         -1);

                pcx_buffer_destroy(&card_buf);
                pcx_buffer_destroy(&command_buf);

                if (!ret)
                        return false;

                pcx_buffer_init(&command_buf);

                pcx_buffer_append_printf(&command_buf,
                                         /* play a moon card */
                                         "play:%i",
                                         card_value + 32);

                pcx_buffer_init(&card_buf);

                pcx_buffer_append_printf(&card_buf,
                                         "Alice ludis: 🌜%i",
                                         card_value);

                remove_card_from_hand(data->players + 0, 2, card_value);

                struct pcx_buffer result_buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&result_buf,
                                         "Bob gajnis la prenvicon.\n"
                                         "\n"
                                         "La prenoj gajnitaj en ĉi tiu raŭndo "
                                         "ĝis nun estas:\n"
                                         "Alice: 1\n"
                                         "Bob: %i",
                                         i + 1);

                ret = send_callback_data(data,
                                         0,
                                         (char *) command_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         card_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         result_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "La dekreta karto estas: 🔔8\n"
                                         "\n"
                                         "Bob komencas la prenvicon.",
                                         ARG_TYPE_LEADER_CHOICE,
                                         1,
                                         -1);

                pcx_buffer_destroy(&result_buf);
                pcx_buffer_destroy(&card_buf);
                pcx_buffer_destroy(&command_buf);

                if (!ret)
                        return false;
        }

        return true;
}

static bool
play_cards_with_no_text(struct test_data *data)
{
        static const struct {
                int lead_card_suit, lead_card_value;
                int follow_card_suit, follow_card_value;
                int winner;
                int next_leader;
        } cards[] = {
                { 0, 11, 0, 1, 1, 0 },
                { 2, 11, 0, 9, 1, 1 },
                { 1, 9, 2, 9, 1, 1 },
                { 0, 10, 2, 1, 1, 0 },
        };
        int leader = 1;
        int ret;
        int takes[2] = { 1, 5 };

        for (int i = 0; i < PCX_N_ELEMENTS(cards); i++) {
                struct pcx_buffer command_buf = PCX_BUFFER_STATIC_INIT;

                add_card_command(&command_buf,
                                 "play",
                                 cards[i].lead_card_suit,
                                 cards[i].lead_card_value);

                struct pcx_buffer card_buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&card_buf,
                                         "%s ludis: ",
                                         test_message_player_names[leader]);
                add_card(&card_buf,
                         cards[i].lead_card_suit,
                         cards[i].lead_card_value);

                remove_card_from_hand(data->players + leader,
                                      cards[i].lead_card_suit,
                                      cards[i].lead_card_value);

                struct pcx_buffer next_buf = PCX_BUFFER_STATIC_INIT;
                int follower = leader ^ 1;

                pcx_buffer_append_printf(&next_buf,
                                         "Nun %s elektas kiun karton ludi.",
                                         test_message_player_names[follower]);

                int follow_command, follow_suit;

                if (data->players[follower].hand[cards[i].lead_card_suit]) {
                        follow_command = ARG_TYPE_FOLLOW_CHOICE;
                        follow_suit = cards[i].lead_card_suit;
                } else {
                        follow_command = ARG_TYPE_UNLIMITED_FOLLOW_CHOICE;
                        follow_suit = -1;
                }

                ret = send_callback_data(data,
                                         leader,
                                         (char *) command_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         card_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         next_buf.data,
                                         follow_command,
                                         follower,
                                         follow_suit,
                                         -1);

                pcx_buffer_destroy(&next_buf);
                pcx_buffer_destroy(&card_buf);
                pcx_buffer_destroy(&command_buf);

                if (!ret)
                        return false;

                pcx_buffer_init(&command_buf);

                add_card_command(&command_buf,
                                 "play",
                                 cards[i].follow_card_suit,
                                 cards[i].follow_card_value);

                pcx_buffer_init(&card_buf);

                pcx_buffer_append_printf(&card_buf,
                                         "%s ludis: ",
                                         test_message_player_names[follower]);
                add_card(&card_buf,
                         cards[i].follow_card_suit,
                         cards[i].follow_card_value);

                remove_card_from_hand(data->players + follower,
                                      cards[i].follow_card_suit,
                                      cards[i].follow_card_value);

                struct pcx_buffer result_buf = PCX_BUFFER_STATIC_INIT;

                int winner = cards[i].winner;

                takes[winner]++;

                pcx_buffer_append_printf(&result_buf,
                                         "%s gajnis la prenvicon.\n"
                                         "\n"
                                         "La prenoj gajnitaj en ĉi tiu raŭndo "
                                         "ĝis nun estas:\n"
                                         "Alice: %i\n"
                                         "Bob: %i",
                                         test_message_player_names[winner],
                                         takes[0],
                                         takes[1]);

                leader = cards[i].next_leader;

                pcx_buffer_init(&next_buf);
                pcx_buffer_append_printf(&next_buf,
                                         "La dekreta karto estas: 🔔8\n"
                                         "\n"
                                         "%s komencas la prenvicon.",
                                         test_message_player_names[leader]);

                ret = send_callback_data(data,
                                         follower,
                                         (char *) command_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         card_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         result_buf.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         next_buf.data,
                                         ARG_TYPE_LEADER_CHOICE,
                                         leader,
                                         -1);

                pcx_buffer_destroy(&next_buf);
                pcx_buffer_destroy(&result_buf);
                pcx_buffer_destroy(&card_buf);
                pcx_buffer_destroy(&command_buf);

                if (!ret)
                        return false;
        }

        return true;
}

static bool
play_sevens(struct test_data *data)
{
        bool ret = true;

        remove_card_from_hand(data->players + 0, 2, 7);

        ret = send_callback_data(data,
                                 0,
                                 "play:39", /* 7 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜7💎",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                return false;

        remove_card_from_hand(data->players + 1, 1, 7);

        ret = send_callback_data(data,
                                 1,
                                 "play:23", /* 7 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝7💎",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon. Ri tuj gajnas "
                                 "du poentojn pro la du 7oj.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 2\n"
                                 "Bob: 9",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Alice komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 0,
                                 -1);
        if (!ret)
                return false;

        return true;
}

static bool
play_threes(struct test_data *data)
{
        bool ret = true;

        remove_card_from_hand(data->players + 0, 2, 3);

        ret = send_callback_data(data,
                                 0,
                                 "play:35", /* 3 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜3🔄\n"
                                 "\n"
                                 "Nun ri elektas ĉu interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 0,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Kiun karton vi volas meti kiel la dekretan "
                                 "karton?",
                                 ARG_TYPE_BUTTONS,
                                 "exchange:8",
                                 "Lasi la antaŭan dekretan karton",
                                 "exchange:37", "🌜5📤",
                                 NULL,
                                 -1);
        if (!ret)
                return false;

        /* Leave the trump card alone */
        ret = send_callback_data(data,
                                 0,
                                 "exchange:8",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice decidis ne interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                return false;

        remove_card_from_hand(data->players + 1, 1, 3);

        ret = send_callback_data(data,
                                 1,
                                 "play:19", /* 3 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝3🔄\n"
                                 "\n"
                                 "Nun ri elektas ĉu interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Kiun karton vi volas meti kiel la dekretan "
                                 "karton?",
                                 ARG_TYPE_BUTTONS,
                                 "exchange:8",
                                 "Lasi la antaŭan dekretan karton",
                                 "exchange:21", "🗝5📤",
                                 NULL,
                                 -1);
        if (!ret)
                return false;

        ret = send_callback_data(data,
                                 1,
                                 "exchange:8",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob decidis ne interŝanĝi la dekretan "
                                 "karton.",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice gajnis la prenvicon.\n"
                                 "\n"
                                 "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun "
                                 "estas:\n"
                                 "Alice: 3\n"
                                 "Bob: 9",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "La dekreta karto estas: 🔔8\n"
                                 "\n"
                                 "Alice komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 0,
                                 -1);
        if (!ret)
                return false;

        return true;
}

static bool
play_fives(struct test_data *data,
           int round_num)
{
        bool ret;

        remove_card_from_hand(data->players + 0, 2, 5);

        ret = send_callback_data(data,
                                 0,
                                 "play:37", /* 5 moons */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🌜5📤",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 ARG_TYPE_UNLIMITED_FOLLOW_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                return false;

        remove_card_from_hand(data->players + 0, 1, 5);

        struct pcx_buffer round_end = PCX_BUFFER_STATIC_INIT;
        int alice_score = round_num * 3;
        int bob_score = round_num * 6;

        pcx_buffer_append_printf(&round_end,
                                 "La raŭndo finiĝis kaj la poentoj nun estas:\n"
                                 "\n"
                                 "Alice: %i (+ 1)\n"
                                 "Bob: %i (+ 6)",
                                 alice_score,
                                 bob_score);


        if (bob_score >= 21) {
                pcx_buffer_append_string(&round_end,
                                         "\n"
                                         "\n"
                                         "Bob havas almenaŭ 21 poentojn kaj "
                                         "finas la partion.\n"
                                         "\n"
                                         "🏆 Bob gajnis la partion!");

                ret = send_callback_data(data,
                                         1,
                                         "play:21", /* 5 keys */
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "Bob ludis: 🗝5📤",
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "Alice gajnis la prenvicon.\n"
                                         "\n"
                                         "La prenoj gajnitaj en ĉi tiu raŭndo "
                                         "ĝis nun estas:\n"
                                         "Alice: 4\n"
                                         "Bob: 9",
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         round_end.data,
                                         TEST_MESSAGE_TYPE_GAME_OVER,
                                         -1);
        } else {
                struct pcx_buffer round_start = PCX_BUFFER_STATIC_INIT;
                int next_leader = (round_num ^ 1) & 1;
                const char *next_name = test_message_player_names[next_leader];

                pcx_buffer_append_printf(&round_start,
                                         "La dekreta karto estas: 🔔8\n"
                                         "\n"
                                         "%s komencas la prenvicon.",
                                         next_name);

                deal_cards(data);

                ret = send_callback_data(data,
                                         1,
                                         "play:21", /* 5 keys */
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "Bob ludis: 🗝5📤",
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         "Alice gajnis la prenvicon.\n"
                                         "\n"
                                         "La prenoj gajnitaj en ĉi tiu raŭndo "
                                         "ĝis nun estas:\n"
                                         "Alice: 4\n"
                                         "Bob: 9",
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         round_end.data,
                                         TEST_MESSAGE_TYPE_GLOBAL,
                                         round_start.data,
                                         ARG_TYPE_LEADER_CHOICE,
                                         next_leader,
                                         -1);

                pcx_buffer_destroy(&round_start);
        }

        pcx_buffer_destroy(&round_end);

        if (!ret)
                return false;

        return true;
}

static bool
start_odd_round(struct test_data *data)
{
        bool ret;

        /* Play the 1 keys and then the 11 keys so that Alice won’t
         * have any keys and Bob will lead.
         */

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
                return false;

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
                                 "Bob komencas la prenvicon.",
                                 ARG_TYPE_LEADER_CHOICE,
                                 1,
                                 -1);
        if (!ret)
                return false;

        return true;
}

static bool
start_even_round(struct test_data *data)
{
        bool ret;

        /* Play the 11 keys and then the 1 keys so that Alice won’t
         * have any keys and Bob will lead.
         */

        remove_card_from_hand(data->players + 0, 1, 11);

        char *card_question =
                make_card_choice_question(data->players + 1, false);

        ret = send_callback_data(data,
                                 0,
                                 "play:27", /* 11 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Alice ludis: 🗝11↕️",
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Nun Bob elektas kiun karton ludi.",
                                 TEST_MESSAGE_TYPE_PRIVATE,
                                 1,
                                 card_question,
                                 ARG_TYPE_BUTTONS,
                                 "play:17", "🗝1🔼",
                                 "play:26", "🗝10",
                                 NULL,
                                 -1);

        pcx_free(card_question);

        if (!ret)
                return false;

        remove_card_from_hand(data->players + 1, 1, 1);

        ret = send_callback_data(data,
                                 1,
                                 "play:17", /* 1 keys */
                                 TEST_MESSAGE_TYPE_GLOBAL,
                                 "Bob ludis: 🗝1🔼",
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
                return false;

        return true;
}

static bool
play_round(struct test_data *data,
           int round_num)
{
        bool ret;

        if (round_num & 1)
                ret = start_odd_round(data);
        else
                ret = start_even_round(data);

        if (!ret)
                return false;

        ret = play_even_cards(data);
        if (!ret)
                return false;

        /* Now the cards are:
         * Alice: 1 bells, 1, 3, 5, 7, 9, 11 moons.
         * Bob: 3, 5, 7, 9 keys. 9-11 bells.
         */

        ret = play_cards_with_no_text(data);
        if (!ret)
                return false;

        ret = play_sevens(data);
        if (!ret)
                return false;

        /* Now Alice has the 3 and 5 of moons and Bob has the 3 and 5 of keys.
         * The takes won so far are Alice: 2, Bob 9.
         * Alice leads.
         */
        ret = play_threes(data);
        if (!ret)
                return false;

        ret = play_fives(data, round_num);
        if (!ret)
                return false;

        return true;
}

static bool
test_full_game(void)
{
        struct test_data *data = create_test_data();

        bool ret = true;

        for (int i = 1; i <= 4; i++) {
                ret = play_round(data, i);

                if (!ret)
                        goto out;
        }

out:
        free_test_data(data);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_trump_suit() ||
            !test_lose_but_lead() ||
            !test_exchange() ||
            !test_draw_card() ||
            !test_become_trump() ||
            !test_two_become_trumps() ||
            !test_force_best_card() ||
            !test_full_game())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
