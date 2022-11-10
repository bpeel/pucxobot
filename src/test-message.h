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

#ifndef PCX_TEST_MESSAGE_H
#define PCX_TEST_MESSAGE_H

#include <stdbool.h>

#include "pcx-list.h"
#include "pcx-main-context.h"
#include "pcx-class-store.h"

enum test_message_type {
        TEST_MESSAGE_TYPE_PRIVATE,
        TEST_MESSAGE_TYPE_GLOBAL,
        TEST_MESSAGE_TYPE_GAME_OVER,
};

struct test_message_button {
        struct pcx_list link;
        char *data;
        char *text;
};

struct test_message {
        struct pcx_list link;
        enum test_message_type type;
        int destination;
        char *message;
        bool check_buttons;
        struct pcx_list buttons;
};

struct test_message_data {
        struct pcx_list queue;
        struct pcx_main_context_source *check_timeout_source;
        struct pcx_class_store *class_store;
        bool had_error;
};

extern const struct pcx_game_callbacks
test_message_callbacks;

extern const char *const
test_message_player_names[];

void
test_message_data_init(struct test_message_data *data);

void
test_message_data_destroy(struct test_message_data *data);

struct test_message *
test_message_queue(struct test_message_data *data,
                   enum test_message_type type);

void
test_message_enable_check_buttons(struct test_message *message);

void
test_message_add_button(struct test_message *message,
                        const char *data,
                        const char *text);

bool
test_message_run_queue(struct test_message_data *data);

#endif /* PCX_TEST_MESSAGE_H */
