/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2024  Neil Roberts
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
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pcx-werewolf.h"
#include "pcx-list.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "test-message.h"
#include "test-time-hack.h"

struct test_data {
        struct pcx_werewolf *werewolf;
        struct test_message_data message_data;
};

static struct test_data *
create_test_data(void)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        test_message_data_init(&data->message_data);

        return data;
}

static void
free_test_data(struct test_data *data)
{
        if (data->werewolf)
                pcx_werewolf_game.free_game_cb(data->werewolf);

        test_message_data_destroy(&data->message_data);

        pcx_free(data);
}

static struct test_message *
queue_global_message(struct test_data *data,
                     const char *message_text)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_GLOBAL);

        message->message = pcx_strdup(message_text);

        return message;
}

static struct test_message *
queue_private_message(struct test_data *data,
                      int destination,
                      const char *message_text)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_PRIVATE);

        message->destination = destination;
        message->message = pcx_strdup(message_text);

        return message;
}

static bool
start_game(struct test_data *data,
           int n_players,
           const enum pcx_werewolf_role *card_overrides)
{
        struct pcx_werewolf_debug_overrides overrides = {
                .cards = card_overrides,
        };

        struct test_message *message =
                queue_global_message(data,
                                     "Which game mode do you want to play?");

        if (n_players == 4) {
                test_message_enable_check_buttons(message);
                test_message_add_button(message, "mode:0", "Basic");
                test_message_add_button(message, "mode:1", "Moonstruck");
                test_message_add_button(message, "mode:2", "Lonely night");
                test_message_add_button(message, "mode:3", "Confusion");
                test_message_add_button(message, "mode:4", "Payback");
                test_message_add_button(message, "mode:6", "House of despair");
                test_message_add_button(message, "mode:8", "Anarchy");
        }

        data->werewolf = pcx_werewolf_new(&test_message_callbacks,
                                          &data->message_data,
                                          PCX_TEXT_LANGUAGE_ENGLISH,
                                          n_players,
                                          test_message_player_names,
                                          &overrides);


        return test_message_run_queue(&data->message_data);
}

static bool
send_vote(struct test_data *data,
          int voter,
          int votee)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf, "vote:%i", votee);

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  voter,
                                                  (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return test_message_run_queue(&data->message_data);
}

static bool
send_simple_vote(struct test_data *data,
                 int voter,
                 int votee)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "%s voted",
                                 test_message_player_names[voter]);

        queue_global_message(data, (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return send_vote(data, voter, votee);
}

static void
zero_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        bool *timeout_hit = user_data;

        *timeout_hit = true;
}

static bool
check_idle(struct test_data *data)
{
        bool timeout_hit = false;

        struct pcx_main_context_source *timeout =
                pcx_main_context_add_timeout(NULL,
                                             0, /* milliseconds */
                                             zero_timeout_cb,
                                             &timeout_hit);

        pcx_main_context_poll(NULL);

        if (!timeout_hit)
                pcx_main_context_remove_source(timeout);

        return test_message_run_queue(&data->message_data);
}

static struct test_data *
start_game_with_cards(int n_players,
                      const enum pcx_werewolf_role *cards,
                      const char *village_message)
{
        struct test_data *data = create_test_data();

        if (data == NULL)
                return NULL;

        if (!start_game(data, n_players, cards))
                goto error;

        queue_global_message(data, village_message);

        for (int i = 0; i < n_players; i++) {
                const char *role_message = NULL;

                switch (cards[i]) {
                case PCX_WEREWOLF_ROLE_VILLAGER:
                        role_message = "Your role is: 🧑‍🌾 Villager";
                        break;
                case PCX_WEREWOLF_ROLE_WEREWOLF:
                        role_message = "Your role is: 🐺 Werewolf";
                        break;
                case PCX_WEREWOLF_ROLE_MINION:
                        role_message = "Your role is: 🦺 Minion";
                        break;
                case PCX_WEREWOLF_ROLE_MASON:
                        role_message = "Your role is: ⚒️ Mason";
                        break;
                case PCX_WEREWOLF_ROLE_SEER:
                        role_message = "Your role is: 🔮 Seer";
                        break;
                case PCX_WEREWOLF_ROLE_ROBBER:
                        role_message = "Your role is: 🤏 Robber";
                        break;
                case PCX_WEREWOLF_ROLE_TROUBLEMAKER:
                        role_message = "Your role is: 🐈 Troublemaker";
                        break;
                case PCX_WEREWOLF_ROLE_DRUNK:
                        role_message = "Your role is: 🍺 Drunk";
                        break;
                case PCX_WEREWOLF_ROLE_INSOMNIAC:
                        role_message = "Your role is: 🥱 Insomniac";
                        break;
                case PCX_WEREWOLF_ROLE_TANNER:
                        role_message = "Your role is: 🙍‍♂️ Tanner";
                        break;
                case PCX_WEREWOLF_ROLE_HUNTER:
                        role_message = "Your role is: 🔫 Hunter";
                        break;
                }

                queue_private_message(data, i, role_message);
        }

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "mode:0");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static struct test_data *
start_basic_game(int n_werewolves)
{
        enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        for (int i = 0; i < n_werewolves; i++)
                override_cards[i] = PCX_WEREWOLF_ROLE_WEREWOLF;

        for (int i = 4; i < 4 + (3 - n_werewolves); i++)
                override_cards[i] = PCX_WEREWOLF_ROLE_WEREWOLF;

        struct test_data *data =
                start_game_with_cards(4 /* n_players */,
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 3\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        if (!check_idle(data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static struct test_data *
skip_to_voting_phase(int n_werewolves)
{
        struct test_data *data = start_basic_game(n_werewolves);

        if (data == NULL)
                return NULL;

        test_time_hack_add_time(9);

        if (!check_idle(data))
                goto error;

        test_time_hack_add_time(2);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        if (n_werewolves == 1) {
                queue_private_message(data,
                                      0,
                                      "You are the only werewolf! You can look "
                                      "at a card in the center of the table. "
                                      "Which one do you want to see?");

                if (!test_message_run_queue(&data->message_data))
                        goto error;

                queue_private_message(data,
                                      0,
                                      "The card you picked is B: 🐺 Werewolf");

                queue_global_message(data,
                                     "🌅 The sun has risen. Everyone in the "
                                     "village wakes up and starts discussing "
                                     "who they think the werewolves might be.");

                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "wolfsee:1");
        } else {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_string(&buf,
                                         "The werewolves in the village "
                                         "are:\n");

                for (int i = 0; i < n_werewolves; i++) {
                        pcx_buffer_append_printf(&buf,
                                                 "\n%s",
                                                 test_message_player_names[i]);
                }

                for (int i = 0; i < n_werewolves; i++)
                        queue_private_message(data, i, (const char *) buf.data);

                pcx_buffer_destroy(&buf);

                if (!test_message_run_queue(&data->message_data))
                        goto error;

                /* The wolves can’t see center cards when there is
                 * more than one.
                 */
                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "wolfsee:2");

                if (!check_idle(data))
                        goto error;

                queue_global_message(data,
                                     "🌅 The sun has risen. Everyone in the "
                                     "village wakes up and starts discussing "
                                     "who they think the werewolves might be.");

                test_time_hack_add_time(16);
        }

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(59);

        if (!check_idle(data))
                goto error;

        test_time_hack_add_time(2);

        struct test_message *message =
                queue_global_message(data,
                                     "If you’ve finished the discussion, you "
                                     "can vote for who you think the werewolf "
                                     "is. You can change your mind up until "
                                     "everyone has voted.");
        test_message_enable_check_buttons(message);
        test_message_add_button(message, "vote:0", "Alice");
        test_message_add_button(message, "vote:1", "Bob");
        test_message_add_button(message, "vote:2", "Charles");
        test_message_add_button(message, "vote:3", "David");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_vote_one_person_villagers_win(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🐺👉 Alice\n"
                             "Charles 🧑‍🌾👉 Alice\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice Alice. Their "
                             "role was: 🐺 Werewolf\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_vote_one_person_werewolves_win(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Charles\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 Alice\n"
                             "David 🧑‍🌾👉 Bob\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🧑‍🌾 Villager\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_vote_multiple_people_villagers_win(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Charles\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 Alice\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Alice (🐺 Werewolf)\n"
                             "Charles (🧑‍🌾 Villager)\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_vote_multiple_people_werewolves_win(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 David\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Charles (🧑‍🌾 Villager)\n"
                             "David (🧑‍🌾 Villager)\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_vote_multiple_people_nobody_wins(void)
{
        struct test_data *data = skip_to_voting_phase(0);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 David\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Charles (🧑‍🌾 Villager)\n"
                             "David (🧑‍🌾 Villager)\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🤦 Nobody wins 🤦");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_lone_wolf(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 3\n"
                                      "🐺 Werewolf × 3\n"
                                      "🔮 Seer\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        struct test_message *message =
                queue_private_message(data,
                                      3,
                                      "You are the only werewolf! You can look "
                                      "at a card in the center of the table. "
                                      "Which one do you want to see?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "wolfsee:0", "A");
        test_message_add_button(message, "wolfsee:1", "B");
        test_message_add_button(message, "wolfsee:2", "C");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* Bad sees should do nothing */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  2,
                                                  "wolfsee:2");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "wolfsee");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "wolfsee:potato");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "wolfsee:3");
        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        queue_private_message(data,
                              3,
                              "The card you picked is A: 🔮 Seer");

        queue_global_message(data,
                             "🔮 The seer wakes up and can look at another "
                             "player’s card or two of the cards that aren’t "
                             "being used.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "wolfsee:0");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_no_kill_no_werewolves(void)
{
        struct test_data *data = skip_to_voting_phase(0);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Bob\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "There were no werewolves at the end of the "
                             "game!\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_no_kill_one_werewolf(void)
{
        struct test_data *data = skip_to_voting_phase(1);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice is a werewolf!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_no_kill_two_werewolves(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice and Bob are werewolves!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_no_kill_three_werewolves(void)
{
        struct test_data *data = skip_to_voting_phase(3);

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🐺👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice, Bob and Charles are werewolves!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_bad_vote(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vocxdoni:0");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:4");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:three");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:3:");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:3/");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vate:3");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static bool
test_vote_before_voting_phase(void)
{
        struct test_data *data = start_basic_game(2);

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "vote:0");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static bool
test_vote_self(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              1,
                              "You can’t vote for yourself.");

        if (!send_vote(data, 1, 1))
                ret = false;

        free_test_data(data);

        return ret;
}

static bool
test_change_vote(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, i + 1)) {
                        ret = false;
                        goto out;
                }
        }

        /* Doing this twice because we should still get the message
         * even if they clicked on the same person twice.
         */
        for (int i = 0; i < 2; i++) {
                queue_global_message(data, "Charles has changed their vote.");

                if (!send_vote(data, 2, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 Bob\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice Bob. Their "
                             "role was: 🐺 Werewolf\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_destroy_game_with_game_over_source(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, i + 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🧑‍🌾👉 Bob\n"
                             "\n"
                             "The village has chosen to sacrifice Bob. Their "
                             "role was: 🐺 Werewolf\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");


        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "vote:1");

out:
        free_test_data(data);
        return ret;
}

static struct test_data *
create_see_player_game(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_TANNER,
                PCX_WEREWOLF_ROLE_HUNTER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager\n"
                                      "🐺 Werewolf × 3\n"
                                      "🔮 Seer\n"
                                      "🙍‍♂️ Tanner\n"
                                      "🔫 Hunter\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 1; i < 3; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Bob\n"
                                      "Charles");
        }

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🔮 The seer wakes up and can look at another "
                             "player’s card or two of the cards that aren’t "
                             "being used.");

        struct test_message *message =
                queue_private_message(data,
                                      0, /* player */
                                      "Whose card do you want to see?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "see:1", "Bob");
        test_message_add_button(message, "see:2", "Charles");
        test_message_add_button(message, "see:3", "David");
        test_message_add_button(message,
                                "see:center",
                                "Two cards from the center");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_see_player_card(void)
{
        struct test_data *data = create_see_player_game();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "Bob’s role is: 🐺 Werewolf");

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:1");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static struct test_data *
create_see_center_cards_game(void)
{
        struct test_data *data = create_see_player_game();

        if (data == NULL)
                return NULL;

        struct test_message *message =
                queue_private_message(data,
                                      0,
                                      "Which two cards from the center do you "
                                      "want to see?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "see:19", "A+B");
        test_message_add_button(message, "see:21", "A+C");
        test_message_add_button(message, "see:22", "B+C");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:center");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_see_center_cards_ab(void)
{
        struct test_data *data = create_see_center_cards_game();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "Two of the cards from the center are:\n"
                              "\n"
                              "A: 🐺 Werewolf\n"
                              "B: 🙍‍♂️ Tanner");

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:19");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_see_center_cards_ac(void)
{
        struct test_data *data = create_see_center_cards_game();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "Two of the cards from the center are:\n"
                              "\n"
                              "A: 🐺 Werewolf\n"
                              "C: 🔫 Hunter");

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:21");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_see_center_cards_bc(void)
{
        struct test_data *data = create_see_center_cards_game();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "Two of the cards from the center are:\n"
                              "\n"
                              "B: 🙍‍♂️ Tanner\n"
                              "C: 🔫 Hunter");

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:22");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_seer_in_middle_cards(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 2\n"
                                      "🔮 Seer\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 0; i < 2; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Alice\n"
                                      "Bob");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🔮 The seer wakes up and can look at another "
                             "player’s card or two of the cards that aren’t "
                             "being used.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* The bot should wait anywhere between 5 and 15 seconds to
         * make it look like the seer might be doing something.
         */
        test_time_hack_add_time(4);

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(12);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_bad_see(void)
{
        struct test_data *data = create_see_player_game();

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see");
        /* A non-seer trying to see */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  1,
                                                  "see:0");
        /* The seer can’t see her own card */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:0");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:potato");

        /* Can’t see just one card */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:17");
        /* Can’t see all three cards */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:23");
        /* Can’t see the non-existant 4th card */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:24");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static bool
test_action_in_wrong_phase(void)
{
        struct test_data *data = start_basic_game(2);

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "see:0");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "wolfsee:0");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:0");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:0");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "mode:0");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "take:0");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static struct test_data *
skip_to_robber_phase(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 3\n"
                                      "🐺 Werewolf × 3\n"
                                      "🤏 Robber\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 1; i < 3; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Bob\n"
                                      "Charles");
        }

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🤏 The robber wakes up and may swap his card "
                             "with another player’s card. If so he will look "
                             "at the new card.");

        struct test_message *message =
                queue_private_message(data,
                                      0, /* player */
                                      "Who do you want to rob?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "rob:1", "Bob");
        test_message_add_button(message, "rob:2", "Charles");
        test_message_add_button(message, "rob:3", "David");
        test_message_add_button(message, "rob:nobody", "Nobody");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
skip_to_robber_vote_phase(struct test_data *data)
{
        test_time_hack_add_time(61);

        struct test_message *message =
                queue_global_message(data,
                                     "If you’ve finished the discussion, you "
                                     "can vote for who you think the werewolf "
                                     "is. You can change your mind up until "
                                     "everyone has voted.");
        test_message_enable_check_buttons(message);
        test_message_add_button(message, "vote:0", "Alice");
        test_message_add_button(message, "vote:1", "Bob");
        test_message_add_button(message, "vote:2", "Charles");
        test_message_add_button(message, "vote:3", "David");

        return test_message_run_queue(&data->message_data);
}

static bool
test_rob_werewolf(void)
{
        struct test_data *data = skip_to_robber_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "You take the card in front of Bob and give them "
                              "your card. Their card was: 🐺 Werewolf");
        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:1");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 1) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐺👉 Bob\n"
                             "Bob 🤏👉 Alice\n"
                             "Charles 🐺👉 Bob\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Alice (🐺 Werewolf)\n"
                             "Bob (🤏 Robber)\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_rob_villager(void)
{
        struct test_data *data = skip_to_robber_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "You take the card in front of David and give "
                              "them your card. Their card was: 🧑‍🌾 Villager");
        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:3");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 David\n"
                             "Bob 🐺👉 Alice\n"
                             "Charles 🐺👉 David\n"
                             "David 🤏👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Alice (🧑‍🌾 Villager)\n"
                             "David (🤏 Robber)\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_rob_nobody(void)
{
        struct test_data *data = skip_to_robber_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "You keep the card you have and don’t rob "
                              "anyone.");
        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:nobody");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🤏👉 David\n"
                             "Bob 🐺👉 Alice\n"
                             "Charles 🐺👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Alice (🤏 Robber)\n"
                             "David (🧑‍🌾 Villager)\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_robber_in_middle_cards(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 2\n"
                                      "🤏 Robber\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 0; i < 2; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Alice\n"
                                      "Bob");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🤏 The robber wakes up and may swap his card "
                             "with another player’s card. If so he will look "
                             "at the new card.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* The bot should wait anywhere between 5 and 15 seconds to
         * make it look like the robber might be doing something.
         */
        test_time_hack_add_time(4);

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(12);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_bad_rob(void)
{
        struct test_data *data = skip_to_robber_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob");
        /* A non-robber tring to rob */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  1,
                                                  "rob:0");
        /* The robber can’t rob himself */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:0");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "rob:potato");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static struct test_data *
skip_to_troublemaker_phase(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_TROUBLEMAKER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 3\n"
                                      "🐺 Werewolf × 3\n"
                                      "🐈 Troublemaker\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 1; i < 3; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Bob\n"
                                      "Charles");
        }

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🐈 The troublemaker wakes up and can swap two "
                             "players’ cards.");

        struct test_message *message =
                queue_private_message(data,
                                      0, /* player */
                                      "Pick the first player whose card you "
                                      "want to swap.");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "swap:1", "Bob");
        test_message_add_button(message, "swap:2", "Charles");
        test_message_add_button(message, "swap:3", "David");
        test_message_add_button(message, "swap:nobody", "Nobody");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_swap(void)
{
        struct test_data *data = skip_to_troublemaker_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        struct test_message *message =
                queue_private_message(data,
                                      0,
                                      "Great, now pick the second player whose "
                                      "card you want to swap.");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "swap:1", "Bob");
        test_message_add_button(message, "swap:2", "Charles");
        test_message_add_button(message, "swap:nobody", "Nobody");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:3");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* Trying to swap with the same person should be ignored */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:3");
        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        queue_private_message(data,
                              0,
                              "You swap the cards of David and Bob.");
        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:1");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 3) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐈👉 David\n"
                             "Bob 🧑‍🌾👉 David\n"
                             "Charles 🐺👉 Bob\n"
                             "David 🐺👉 Bob\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Bob (🧑‍🌾 Villager)\n"
                             "David (🐺 Werewolf)\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_swap_nobody(void)
{
        struct test_data *data = skip_to_troublemaker_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        queue_private_message(data,
                              0,
                              "You don’t cause any trouble tonight and don’t "
                              "swap any cards.");
        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:nobody");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🐈👉 David\n"
                             "Bob 🐺👉 Alice\n"
                             "Charles 🐺👉 David\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Alice (🐈 Troublemaker)\n"
                             "David (🧑‍🌾 Villager)\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_troublemaker_in_middle_cards(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_TROUBLEMAKER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 2\n"
                                      "🐈 Troublemaker\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 0; i < 2; i++) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Alice\n"
                                      "Bob");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🐈 The troublemaker wakes up and can swap two "
                             "players’ cards.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* The bot should wait anywhere between 5 and 15 seconds to
         * make it look like the troublemaker might be doing something.
         */
        test_time_hack_add_time(4);

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(12);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_bad_swap(void)
{
        struct test_data *data = skip_to_troublemaker_phase();

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap");
        /* A non-troublemaker tring to swap */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  1,
                                                  "swap:0");
        /* The troublemaker can’t swap her own card */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:0");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  0,
                                                  "swap:potato");

        if (!check_idle(data))
                ret = false;

        free_test_data(data);

        return ret;
}

static bool
test_anarchy_mode(void)
{
        bool ret = true;

        for (int n_players = 3; n_players <= 10; n_players++) {
                struct test_data *data = create_test_data();

                if (data == NULL)
                        return false;

                enum pcx_werewolf_role *cards =
                        pcx_alloc((n_players + 3) * sizeof *cards);
                for (int i = 0; i < n_players + 3; i++)
                        cards[i] = PCX_WEREWOLF_ROLE_VILLAGER;

                bool start_ret = start_game(data, n_players, cards);

                pcx_free(cards);

                if (!start_ret) {
                        ret = false;
                        goto out;
                }

                /* Sending invalid modes shouldn’t do anything */
                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "mode");
                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "mode:9");
                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "mode:potato");
                if (n_players < 5) {
                        pcx_werewolf_game.handle_callback_data_cb(
                                data->werewolf,
                                0,
                                "mode:7");
                }
                if (n_players > 5) {
                        pcx_werewolf_game.handle_callback_data_cb(
                                data->werewolf,
                                0,
                                "mode:0");
                }

                if (!check_idle(data)) {
                        ret = false;
                        goto out;
                }

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&buf,
                                         "The village consists of the "
                                         "following roles:\n"
                                         "\n"
                                         "🧑‍🌾 Villager × %i\n"
                                         "\n"
                                         "Everybody looks at their role before "
                                         "falling asleep for the night.",
                                         n_players + 3);

                queue_global_message(data, (const char *) buf.data);

                pcx_buffer_destroy(&buf);

                for (int i = 0; i < n_players; i++) {
                        queue_private_message(data,
                                              i,
                                              "Your role is: 🧑‍🌾 Villager");
                }

                pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                          0,
                                                          "mode:8");

                if (!test_message_run_queue(&data->message_data)) {
                        ret = false;
                        goto out;
                }

        out:
                free_test_data(data);
        }

        return ret;
}

static bool
test_no_masons(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 5\n"
                                      "⚒️ Mason × 2\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "⚒️ The masons wake up and look at each other "
                             "before going back to sleep.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_lone_mason(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 5\n"
                                      "⚒️ Mason × 2\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "⚒️ The masons wake up and look at each other "
                             "before going back to sleep.");

        queue_private_message(data,
                              3,
                              "You are the only mason.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_two_masons(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 5\n"
                                      "⚒️ Mason × 2\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "⚒️ The masons wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 2; i <= 3; i++) {
                queue_private_message(data,
                                      i,
                                      "The masons in the village are:\n"
                                      "\n"
                                      "Charles\n"
                                      "David");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_drunk(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_DRUNK,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 5\n"
                                      "🤏 Robber\n"
                                      "🍺 Drunk\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🤏 The robber wakes up and may swap his card "
                             "with another player’s card. If so he will look "
                             "at the new card.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🍺 The drunk wakes up confused and swaps his "
                             "card with one of the cards in the middle of the "
                             "table. He no longer knows what role he is.");

        struct test_message *message =
                queue_private_message(data,
                                      3,
                                      "Which card from the center do you "
                                      "want?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "take:0", "A");
        test_message_add_button(message, "take:1", "B");
        test_message_add_button(message, "take:2", "C");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        /* Bad takes should do nothing */
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  2,
                                                  "take:2");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "take");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "take:potato");
        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "take:3");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        queue_private_message(data,
                              3,
                              "You take card A");

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "take:0");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!skip_to_robber_vote_phase(data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 3) ||
            !send_simple_vote(data, 1, 3) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 David\n"
                             "Bob 🧑‍🌾👉 David\n"
                             "Charles 🧑‍🌾👉 David\n"
                             "David 🤏👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice David. "
                             "Their role was: 🤏 Robber\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🤦 Nobody wins 🤦");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_no_drunk(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_DRUNK,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🍺 Drunk\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🍺 The drunk wakes up confused and swaps his "
                             "card with one of the cards in the middle of the "
                             "table. He no longer knows what role he is.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_still_insomniac(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🥱 Insomniac\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🥱 The insomniac wakes up and checks her card to "
                             "see if she’s still the insomniac.");
        queue_private_message(data, 3, "You are still the insomniac");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_no_longer_insomniac(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 5\n"
                                      "🤏 Robber\n"
                                      "🥱 Insomniac\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🤏 The robber wakes up and may swap his card "
                             "with another player’s card. If so he will look "
                             "at the new card.");

        struct test_message *message =
                queue_private_message(data,
                                      2, /* player */
                                      "Who do you want to rob?");

        test_message_enable_check_buttons(message);
        test_message_add_button(message, "rob:0", "Alice");
        test_message_add_button(message, "rob:1", "Bob");
        test_message_add_button(message, "rob:3", "David");
        test_message_add_button(message, "rob:nobody", "Nobody");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        queue_private_message(data,
                              2,
                              "You take the card in front of David and give "
                              "them your card. Their card was: "
                              "🥱 Insomniac");

        queue_global_message(data,
                             "🥱 The insomniac wakes up and checks her card to "
                             "see if she’s still the insomniac.");

        queue_private_message(data,
                              3,
                              "Your card is now: 🤏 Robber");

        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  2,
                                                  "rob:3");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_insomniac_in_middle(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🥱 Insomniac\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🥱 The insomniac wakes up and checks her card to "
                             "see if she’s still the insomniac.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_hunter_kills(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_HUNTER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🔫 Hunter\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(61);

        struct test_message *message =
                queue_global_message(data,
                                     "If you’ve finished the discussion, you "
                                     "can vote for who you think the werewolf "
                                     "is. You can change your mind up until "
                                     "everyone has voted.");
        test_message_enable_check_buttons(message);
        test_message_add_button(message, "vote:0", "Alice");
        test_message_add_button(message, "vote:1", "Bob");
        test_message_add_button(message, "vote:2", "Charles");
        test_message_add_button(message, "vote:3", "David");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🔫👉 Bob\n"
                             "David 🧑‍🌾👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🔫 Hunter\n"
                             "\n"
                             "With his dying breath, the hunter shoots and "
                             "kills Bob (🧑‍🌾 Villager).\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🤦 Nobody wins 🤦");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_hunter_doesnt_kill(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_HUNTER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🔫 Hunter\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(61);

        struct test_message *message =
                queue_global_message(data,
                                     "If you’ve finished the discussion, you "
                                     "can vote for who you think the werewolf "
                                     "is. You can change your mind up until "
                                     "everyone has voted.");
        test_message_enable_check_buttons(message);
        test_message_add_button(message, "vote:0", "Alice");
        test_message_add_button(message, "vote:1", "Bob");
        test_message_add_button(message, "vote:2", "Charles");
        test_message_add_button(message, "vote:3", "David");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🔫👉 Bob\n"
                             "David 🧑‍🌾👉 Bob\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Bob (🧑‍🌾 Villager)\n"
                             "Charles (🔫 Hunter)\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🤦 Nobody wins 🤦");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static struct test_data *
set_up_minion_with_no_werewolves_vote(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MINION,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🦺 Minion\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return NULL;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🦺 The minion wakes up and finds out who the "
                             "werewolves are.");

        queue_private_message(data,
                              2,
                              "Nobody is a werewolf!");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static bool
test_minion_no_werewolves_villagers_win(void)
{
        struct test_data *data = set_up_minion_with_no_werewolves_vote();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🦺👉 Bob\n"
                             "David 🧑‍🌾👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🦺 Minion\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_minion_no_werewolves_minion_wins(void)
{
        struct test_data *data = set_up_minion_with_no_werewolves_vote();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🧑‍🌾👉 Alice\n"
                             "Charles 🦺👉 Alice\n"
                             "David 🧑‍🌾👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice Alice. "
                             "Their role was: 🧑‍🌾 Villager\n"
                             "\n"
                             "Nobody is a werewolf!\n"
                             "\n"
                             "🦺 The minion wins! 🦺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
minion_dies_but_there_are_werewolves(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_MINION,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 2\n"
                                      "🦺 Minion\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 1; i <= 3; i += 2) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Bob\n"
                                      "David");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🦺 The minion wakes up and finds out who the "
                             "werewolves are.");

        queue_private_message(data,
                              2,
                              "The werewolves in the village are:\n"
                              "\n"
                              "Bob\n"
                              "David");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🦺👉 Alice\n"
                             "David 🐺👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🦺 Minion\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
minion_in_middle_cards(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MINION,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🦺 Minion\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🦺 The minion wakes up and finds out who the "
                             "werewolves are.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
tanner_and_village_win(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_TANNER,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 4\n"
                                      "🐺 Werewolf × 2\n"
                                      "🙍‍♂️ Tanner\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🐺 The werewolves wake up and look at each other "
                             "before going back to sleep.");

        for (int i = 1; i <= 3; i += 2) {
                queue_private_message(data,
                                      i,
                                      "The werewolves in the village are:\n"
                                      "\n"
                                      "Bob\n"
                                      "David");
        }

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        test_time_hack_add_time(16);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🐺👉 Charles\n"
                             "Charles 🙍‍♂️👉 Bob\n"
                             "David 🐺👉 Bob\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Bob (🐺 Werewolf)\n"
                             "Charles (🙍‍♂️ Tanner)\n"
                             "\n"
                             "🙍‍♂️🧑‍🌾 The tanner AND the villagers win! 🧑‍🌾🙍‍♂️");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
tanner_wins(void)
{
        static const enum pcx_werewolf_role override_cards[] = {
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_TANNER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        struct test_data *data =
                start_game_with_cards(4, /* n_players */
                                      override_cards,
                                      "The village consists of the following "
                                      "roles:\n"
                                      "\n"
                                      "🧑‍🌾 Villager × 6\n"
                                      "🙍‍♂️ Tanner\n"
                                      "\n"
                                      "Everybody looks at their role before "
                                      "falling asleep for the night.");

        if (!data)
                return false;

        bool ret = true;

        test_time_hack_add_time(11);

        queue_global_message(data,
                             "🌅 The sun has risen. Everyone in the village "
                             "wakes up and starts discussing who they think "
                             "the werewolves might be.");

        if (!test_message_run_queue(&data->message_data)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 🧑‍🌾👉 Charles\n"
                             "Bob 🧑‍🌾👉 Charles\n"
                             "Charles 🙍‍♂️👉 Bob\n"
                             "David 🧑‍🌾👉 Charles\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🙍‍♂️ Tanner\n"
                             "\n"
                             "🙍‍♂️ The tanner wins! 🙍‍♂️");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 2)) {
                ret = false;
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

        if (!test_vote_one_person_villagers_win())
                ret = EXIT_FAILURE;

        if (!test_vote_one_person_werewolves_win())
                ret = EXIT_FAILURE;

        if (!test_vote_multiple_people_villagers_win())
                ret = EXIT_FAILURE;

        if (!test_vote_multiple_people_werewolves_win())
                ret = EXIT_FAILURE;

        if (!test_vote_multiple_people_nobody_wins())
                ret = EXIT_FAILURE;

        if (!test_lone_wolf())
                ret = EXIT_FAILURE;

        if (!test_no_kill_no_werewolves())
                ret = EXIT_FAILURE;

        if (!test_no_kill_one_werewolf())
                ret = EXIT_FAILURE;

        if (!test_no_kill_two_werewolves())
                ret = EXIT_FAILURE;

        if (!test_no_kill_three_werewolves())
                ret = EXIT_FAILURE;

        if (!test_bad_vote())
                ret = EXIT_FAILURE;

        if (!test_vote_before_voting_phase())
                ret = EXIT_FAILURE;

        if (!test_vote_self())
                ret = EXIT_FAILURE;

        if (!test_change_vote())
                ret = EXIT_FAILURE;

        if (!test_destroy_game_with_game_over_source())
                ret = EXIT_FAILURE;

        if (!test_see_player_card())
                ret = EXIT_FAILURE;

        if (!test_see_center_cards_ab())
                ret = EXIT_FAILURE;

        if (!test_see_center_cards_ac())
                ret = EXIT_FAILURE;

        if (!test_see_center_cards_bc())
                ret = EXIT_FAILURE;

        if (!test_seer_in_middle_cards())
                ret = EXIT_FAILURE;

        if (!test_bad_see())
                ret = EXIT_FAILURE;

        if (!test_action_in_wrong_phase())
                ret = EXIT_FAILURE;

        if (!test_rob_werewolf())
                ret = EXIT_FAILURE;

        if (!test_rob_villager())
                ret = EXIT_FAILURE;

        if (!test_rob_nobody())
                ret = EXIT_FAILURE;

        if (!test_bad_rob())
                ret = EXIT_FAILURE;

        if (!test_robber_in_middle_cards())
                ret = EXIT_FAILURE;

        if (!test_swap())
                ret = EXIT_FAILURE;

        if (!test_swap_nobody())
                ret = EXIT_FAILURE;

        if (!test_troublemaker_in_middle_cards())
                ret = EXIT_FAILURE;

        if (!test_bad_swap())
                ret = EXIT_FAILURE;

        if (!test_anarchy_mode())
                ret = EXIT_FAILURE;

        if (!test_no_masons())
                ret = EXIT_FAILURE;

        if (!test_lone_mason())
                ret = EXIT_FAILURE;

        if (!test_two_masons())
                ret = EXIT_FAILURE;

        if (!test_drunk())
                ret = EXIT_FAILURE;

        if (!test_no_drunk())
                ret = EXIT_FAILURE;

        if (!test_still_insomniac())
                ret = EXIT_FAILURE;

        if (!test_no_longer_insomniac())
                ret = EXIT_FAILURE;

        if (!test_insomniac_in_middle())
                ret = EXIT_FAILURE;

        if (!test_hunter_kills())
                ret = EXIT_FAILURE;

        if (!test_hunter_doesnt_kill())
                ret = EXIT_FAILURE;

        if (!test_minion_no_werewolves_villagers_win())
                ret = EXIT_FAILURE;

        if (!test_minion_no_werewolves_minion_wins())
                ret = EXIT_FAILURE;

        if (!minion_dies_but_there_are_werewolves())
                ret = EXIT_FAILURE;

        if (!minion_in_middle_cards())
                ret = EXIT_FAILURE;

        if (!tanner_and_village_win())
                ret = EXIT_FAILURE;

        if (!tanner_wins())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
