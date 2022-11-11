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

#include "test-message.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pcx-main-context.h"
#include "pcx-game.h"

const char *const
test_message_player_names[] = {
        "Alice",
        "Bob",
        "Charles",
        "David",
        "Eva",
        "Fred",
        "George",
        "Harry",
};

static void
free_message(struct test_message *message)
{
        if (message->check_buttons) {
                struct test_message_button *button, *tmp;

                pcx_list_for_each_safe(button, tmp, &message->buttons, link) {
                        pcx_free(button->data);
                        pcx_free(button->text);
                        pcx_free(button);
                }
        }

        pcx_free(message->message);
        pcx_free(message);
}

void
test_message_add_button(struct test_message *message,
                        const char *data,
                        const char *text)
{
        assert(message->type == TEST_MESSAGE_TYPE_GLOBAL ||
               message->type == TEST_MESSAGE_TYPE_PRIVATE);
        assert(message->check_buttons);

        struct test_message_button *button = pcx_alloc(sizeof *button);
        button->data = pcx_strdup(data);
        button->text = pcx_strdup(text);
        pcx_list_insert(message->buttons.prev, &button->link);
}

static bool
check_buttons(const struct test_message *message,
              size_t n_buttons,
              const struct pcx_game_button *buttons)
{
        if (!message->check_buttons)
                return true;

        unsigned i = 0;
        const struct test_message_button *button;

        pcx_list_for_each(button, &message->buttons, link) {
                if (i >= n_buttons) {
                        fprintf(stderr,
                                "message received containing %u "
                                "buttons when %u were expected\n",
                                (unsigned) n_buttons,
                                (unsigned) pcx_list_length(&message->buttons));
                        return false;
                }

                if (strcmp(button->data, buttons[i].data) ||
                    strcmp(button->text, buttons[i].text)) {
                        fprintf(stderr,
                                "button does not match.\n"
                                "Got: %s) %s\n"
                                "Expected: %s) %s\n",
                                buttons[i].data,
                                buttons[i].text,
                                button->data,
                                button->text);
                        return false;
                }

                i++;
        }

        if (i != n_buttons) {
                fprintf(stderr,
                        "message received containing %u "
                        "buttons when %u were expected\n",
                        (unsigned) n_buttons,
                        (unsigned) pcx_list_length(&message->buttons));
                return false;
        }

        return true;
}

static void
handle_private_message(struct test_message_data *data,
                       const struct pcx_game_message *msg)
{
        if (msg->target < 0 ||
            msg->target >= PCX_N_ELEMENTS(test_message_player_names)) {
                fprintf(stderr,
                        "Private message sent to invalid player %i\n",
                        msg->target);
                data->had_error = true;
                return;
        }

        if (pcx_list_empty(&data->queue)) {
                fprintf(stderr,
                        "Unexpected message sent to “%s”: %s\n",
                        test_message_player_names[msg->target],
                        msg->text);
                data->had_error = true;
                return;
        }

        struct test_message *message =
                pcx_container_of(data->queue.next,
                                 struct test_message,
                                 link);

        if (message->type != TEST_MESSAGE_TYPE_PRIVATE) {
                fprintf(stderr,
                        "Private message to “%s” received when a different "
                        "type was expected: %s\n",
                        test_message_player_names[msg->target],
                        msg->text);
                data->had_error = true;
                return;
        }

        if (message->destination != msg->target) {
                fprintf(stderr,
                        "Message sent to “%s” but expected to “%s”\n",
                        test_message_player_names[msg->target],
                        test_message_player_names[message->destination]);
                data->had_error = true;
                return;
        }

        if (strcmp(msg->text, message->message)) {
                fprintf(stderr,
                        "Message to “%s” does not match expected message.\n"
                        "Got: %s\n"
                        "Expected: %s\n",
                        test_message_player_names[msg->target],
                        msg->text,
                        message->message);
                data->had_error = true;
                return;
        }

        if (!check_buttons(message, msg->n_buttons, msg->buttons)) {
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static void
handle_public_message(struct test_message_data *data,
                      const struct pcx_game_message *msg)
{
        if (pcx_list_empty(&data->queue)) {
                fprintf(stderr,
                        "Unexpected global message sent: %s\n",
                        msg->text);
                data->had_error = true;
                return;
        }

        struct test_message *message =
                pcx_container_of(data->queue.next,
                                 struct test_message,
                                 link);

        if (message->type != TEST_MESSAGE_TYPE_GLOBAL) {
                fprintf(stderr,
                        "Global message received when a different "
                        "type was expected: %s\n",
                        msg->text);
                data->had_error = true;
                return;
        }

        if (strcmp(msg->text, message->message)) {
                fprintf(stderr,
                        "Global Message does not match expected message.\n"
                        "Got: %s\n"
                        "Expected: %s\n",
                        msg->text,
                        message->message);
                data->had_error = true;
                return;
        }

        if (!check_buttons(message, msg->n_buttons, msg->buttons)) {
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static void
set_sideband_data_cb(int data_num,
                     const struct pcx_game_sideband_data *value,
                     bool force,
                     void *user_data)
{
        struct test_message_data *data = user_data;

        if (value->type != PCX_GAME_SIDEBAND_TYPE_STRING) {
                fprintf(stderr,
                        "Received side band data in a format that the "
                        "test harness doesn’t support.\n");
                data->had_error = true;
                return;
        }

        if (pcx_list_empty(&data->queue)) {
                fprintf(stderr,
                        "Unexpected sideband string sent: %i: %s\n",
                        data_num,
                        value->string);
                data->had_error = true;
                return;
        }

        struct test_message *message =
                pcx_container_of(data->queue.next,
                                 struct test_message,
                                 link);

        if (message->type != TEST_MESSAGE_TYPE_SIDEBAND_STRING) {
                fprintf(stderr,
                        "Sideband string received when a different "
                        "type was expected: %i: %s\n",
                        data_num,
                        value->string);
                data->had_error = true;
                return;
        }

        if (data_num != message->destination) {
                fprintf(stderr,
                        "Received sideband string data_num %i but %i "
                        "was expected\n",
                        data_num,
                        message->destination);
                data->had_error = true;
                return;
        }

        if (strcmp(value->string, message->message)) {
                fprintf(stderr,
                        "Sideband string does not match expected string.\n"
                        "Received: %s\n"
                        "Expected: %s\n",
                        value->string,
                        message->message);
                data->had_error = true;
                return;
        }

        if (force) {
                fprintf(stderr,
                        "Received sideband string with the force argument set "
                        "but the test harness does not support this: %i: %s\n",
                        data_num,
                        value->string);
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static void
send_message_cb(const struct pcx_game_message *message,
                void *user_data)
{
        struct test_message_data *data = user_data;

        if (message->target == -1)
                handle_public_message(data, message);
        else
                handle_private_message(data, message);
}

static void
game_over_cb(void *user_data)
{
        struct test_message_data *data = user_data;

        if (pcx_list_empty(&data->queue)) {
                fprintf(stderr, "Unexpected game over\n");
                data->had_error = true;
                return;
        }

        struct test_message *message =
                pcx_container_of(data->queue.next,
                                 struct test_message,
                                 link);

        if (message->type != TEST_MESSAGE_TYPE_GAME_OVER) {
                fprintf(stderr,
                        "Game over received when a different "
                        "type was expected\n");
                data->had_error = true;
                return;
        }

        pcx_list_remove(&message->link);
        free_message(message);
}

static struct pcx_class_store *
get_class_store_cb(void *user_data)
{
        struct test_message_data *data = user_data;

        return data->class_store;
}

const struct pcx_game_callbacks
test_message_callbacks = {
        .send_message = send_message_cb,
        .set_sideband_data = set_sideband_data_cb,
        .game_over = game_over_cb,
        .get_class_store = get_class_store_cb,
};

static void
timeout_cb(struct pcx_main_context_source *source,
           void *user_data)
{
        struct test_message_data *data = user_data;

        assert(data->check_timeout_source == source);

        data->check_timeout_source = NULL;

        fprintf(stderr, "Timeout while waiting for a message\n");

        data->had_error = true;
}

struct test_message *
test_message_queue(struct test_message_data *data,
                   enum test_message_type type)
{
        struct test_message *message = pcx_calloc(sizeof *message);

        message->type = type;
        pcx_list_insert(data->queue.prev, &message->link);

        return message;
}

void
test_message_enable_check_buttons(struct test_message *message)
{
        assert(message->check_buttons == false);

        pcx_list_init(&message->buttons);

        message->check_buttons = true;
}

bool
test_message_run_queue(struct test_message_data *data)
{
        assert(data->check_timeout_source == NULL);

        data->check_timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             10000,
                                             timeout_cb,
                                             data);

        while (!data->had_error &&
               !pcx_list_empty(&data->queue)) {
                pcx_main_context_poll(NULL);
        }

        if (data->check_timeout_source) {
                pcx_main_context_remove_source(data->check_timeout_source);
                data->check_timeout_source = NULL;
        }

        return !data->had_error;
}

void
test_message_data_init(struct test_message_data *data)
{
        memset(data, 0, sizeof *data);

        pcx_list_init(&data->queue);

        data->class_store = pcx_class_store_new();
}

void
test_message_data_destroy(struct test_message_data *data)
{
        struct test_message *message, *tmp;

        pcx_list_for_each_safe(message, tmp, &data->queue, link) {
                free_message(message);
        }

        pcx_class_store_free(data->class_store);
}
