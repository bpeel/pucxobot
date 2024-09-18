/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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
start_basic_game(int n_werewolves)
{
        struct test_data *data = create_test_data();

        queue_global_message(data,
                             "The village consists of the following roles:\n"
                             "\n"
                             "🧑‍🌾 Villager × 4\n"
                             "🐺 Werewolf × 3\n"
                             "\n"
                             "Everybody looks at their role before falling "
                             "asleep for the night.");

        for (int i = 0; i < n_werewolves; i++) {
                queue_private_message(data,
                                      i,
                                      "Your role is: 🐺 Werewolf");
        }

        for (int i = n_werewolves; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "Your role is: 🧑‍🌾 Villager");
        }

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

        if (!start_game(data, 4, override_cards))
                goto error;

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
                             "The werewolves wake up and look at each other "
                             "before going back to sleep.");

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf, "The werewolves in the village are:\n");

        for (int i = 0; i < n_werewolves; i++) {
                pcx_buffer_append_printf(&buf,
                                         "\n%s",
                                         test_message_player_names[i]);
        }

        for (int i = 0; i < n_werewolves; i++) {
                queue_private_message(data, i, (const char *) buf.data);
        }

        pcx_buffer_destroy(&buf);

        if (!test_message_run_queue(&data->message_data))
                goto error;

        test_time_hack_add_time(9);

        if (!check_idle(data))
                goto error;

        test_time_hack_add_time(2);

        queue_global_message(data,
                             "The sun rises and everyone in the village wakes "
                             "up and starts discussing who they think the "
                             "werewolves might be. When you are ready you can "
                             "start voting.");

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

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Alice\n"
                             "Charles 👉 Alice\n"
                             "David 👉 Alice\n"
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

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 2)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Charles\n"
                             "Bob 👉 Charles\n"
                             "Charles 👉 Charles\n"
                             "David 👉 Alice\n"
                             "\n"
                             "The village has chosen to sacrifice Charles. "
                             "Their role was: 🧑‍🌾 Villager\n"
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
                             "Alice 👉 Charles\n"
                             "Bob 👉 Charles\n"
                             "Charles 👉 Alice\n"
                             "David 👉 Alice\n"
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

        if (!send_simple_vote(data, 0, 2) ||
            !send_simple_vote(data, 1, 2) ||
            !send_simple_vote(data, 2, 3)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Charles\n"
                             "Bob 👉 Charles\n"
                             "Charles 👉 David\n"
                             "David 👉 David\n"
                             "\n"
                             "The village has chosen to sacrifice the "
                             "following people:\n"
                             "\n"
                             "Charles (🧑‍🌾 Villager)\n"
                             "David (🧑‍🌾 Villager)\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 3)) {
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

        if (!send_simple_vote(data, 0, 0) ||
            !send_simple_vote(data, 1, 1) ||
            !send_simple_vote(data, 2, 2)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Bob\n"
                             "Charles 👉 Charles\n"
                             "David 👉 David\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "There were no werewolves at the end of the "
                             "game!\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 3)) {
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

        if (!send_simple_vote(data, 0, 0) ||
            !send_simple_vote(data, 1, 1) ||
            !send_simple_vote(data, 2, 2)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Bob\n"
                             "Charles 👉 Charles\n"
                             "David 👉 David\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice is a werewolf!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 3)) {
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

        if (!send_simple_vote(data, 0, 0) ||
            !send_simple_vote(data, 1, 1) ||
            !send_simple_vote(data, 2, 2)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Bob\n"
                             "Charles 👉 Charles\n"
                             "David 👉 David\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice and Bob are werewolves!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 3)) {
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

        if (!send_simple_vote(data, 0, 0) ||
            !send_simple_vote(data, 1, 1) ||
            !send_simple_vote(data, 2, 2)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Bob\n"
                             "Charles 👉 Charles\n"
                             "David 👉 David\n"
                             "\n"
                             "No one got more than one vote so no one dies! "
                             "However, Alice, Bob and Charles are werewolves!\n"
                             "\n"
                             "🐺 The werewolves win! 🐺");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 3)) {
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
test_change_vote(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
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
                             "Alice 👉 Alice\n"
                             "Bob 👉 Alice\n"
                             "Charles 👉 Bob\n"
                             "David 👉 Alice\n"
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
test_destroy_game_with_game_over_source(void)
{
        struct test_data *data = skip_to_voting_phase(2);

        if (data == NULL)
                return false;

        bool ret = true;

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Everybody voted! The votes were:\n"
                             "\n"
                             "Alice 👉 Alice\n"
                             "Bob 👉 Alice\n"
                             "Charles 👉 Alice\n"
                             "David 👉 David\n"
                             "\n"
                             "The village has chosen to sacrifice Alice. Their "
                             "role was: 🐺 Werewolf\n"
                             "\n"
                             "🧑‍🌾 The villagers win! 🧑‍🌾");


        pcx_werewolf_game.handle_callback_data_cb(data->werewolf,
                                                  3,
                                                  "vote:3");

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

        if (!test_change_vote())
                ret = EXIT_FAILURE;

        if (!test_destroy_game_with_game_over_source())
                ret = EXIT_FAILURE;

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
