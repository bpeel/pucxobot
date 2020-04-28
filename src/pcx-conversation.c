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

#include "pcx-conversation.h"

#include <assert.h>

#include "pcx-log.h"
#include "pcx-util.h"

struct pcx_conversation *
pcx_conversation_new(const struct pcx_config *config)
{
        struct pcx_conversation *conv = pcx_calloc(sizeof *conv);

        conv->ref_count = 1;
        conv->game_type = pcx_game_list[0];
        conv->config = config;

        pcx_list_init(&conv->messages);
        pcx_signal_init(&conv->event_signal);

        return conv;
}

static bool
emit_event(struct pcx_conversation *conv,
           enum pcx_conversation_event_type type)
{
        struct pcx_conversation_event event;

        event.type = type;
        event.conversation = conv;

        pcx_conversation_ref(conv);

        bool ret = pcx_signal_emit(&conv->event_signal, &event);

        pcx_conversation_unref(conv);

        return ret;
}

static void
queue_message(struct pcx_conversation *conv,
              int user_num,
              enum pcx_game_message_format format,
              const char *text,
              size_t n_buttons,
              const struct pcx_game_button *buttons)
{
        struct pcx_conversation_message *message = pcx_calloc(sizeof *message);

        message->target_player = user_num;
        message->format = format;
        message->text = pcx_strdup(text);
        message->n_buttons = n_buttons;

        if (n_buttons > 0) {
                message->buttons = pcx_alloc(n_buttons *
                                             sizeof message->buttons[0]);

                for (unsigned i = 0; i < n_buttons; i++) {
                        message->buttons[i].text = pcx_strdup(buttons[i].text);
                        message->buttons[i].data = pcx_strdup(buttons[i].data);
                }
        }

        pcx_list_insert(conv->messages.prev, &message->link);

        emit_event(conv, PCX_CONVERSATION_EVENT_NEW_MESSAGE);
}

static void
send_private_message_cb(int user_num,
                        enum pcx_game_message_format format,
                        const char *message,
                        size_t n_buttons,
                        const struct pcx_game_button *buttons,
                        void *user_data)
{
        struct pcx_conversation *conv = user_data;

        assert(user_num >= 0 && user_num < conv->n_players);

        queue_message(conv,
                      user_num,
                      format,
                      message,
                      n_buttons,
                      buttons);
}

static void
send_message_cb(enum pcx_game_message_format format,
                const char *message,
                size_t n_buttons,
                const struct pcx_game_button *buttons,
                void *user_data)
{
        struct pcx_conversation *conv = user_data;

        queue_message(conv,
                      -1, /* target */
                      format,
                      message,
                      n_buttons,
                      buttons);
}

static void
game_over_cb(void *user_data)
{
        struct pcx_conversation *conv = user_data;

        pcx_log("game finished successfully");

        assert(conv->game);

        conv->game_type->free_game_cb(conv->game);

        conv->game = NULL;
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_private_message = send_private_message_cb,
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

int
pcx_conversation_add_player(struct pcx_conversation *conv)
{
        assert(conv->n_players < conv->game_type->max_players);
        assert(!conv->started);

        int player_num = conv->n_players++;

        emit_event(conv, PCX_CONVERSATION_EVENT_PLAYER_ADDED);

        return player_num;
}

void
pcx_conversation_start(struct pcx_conversation *conv)
{
        if (conv->started)
                return;
        if (conv->n_players < conv->game_type->min_players)
                return;

        assert(conv->game == NULL);

        conv->started = true;

        pcx_conversation_ref(conv);

        emit_event(conv, PCX_CONVERSATION_EVENT_STARTED);

        /* FIXME */
        static const char * const names[] = {
                "Alice",
                "Bob",
                "Charlie",
                "David",
                "Edith",
                "Fred",
        };

        _Static_assert(PCX_N_ELEMENTS(names) == PCX_GAME_MAX_PLAYERS);

        conv->game =
                conv->game_type->create_game_cb(conv->config,
                                                &game_callbacks,
                                                conv,
                                                PCX_TEXT_LANGUAGE_ESPERANTO,
                                                conv->n_players,
                                                names);

        pcx_conversation_unref(conv);
}

void
pcx_conversation_push_button(struct pcx_conversation *conv,
                             int player_num,
                             const char *button_data)
{
        if (conv->game == NULL)
                return;

        assert(player_num >= 0 && player_num < conv->n_players);

        pcx_conversation_ref(conv);

        conv->game_type->handle_callback_data_cb(conv->game,
                                                 player_num,
                                                 button_data);

        pcx_conversation_unref(conv);
}

void
pcx_conversation_ref(struct pcx_conversation *conv)
{
        conv->ref_count++;
}

static void
destroy_button(struct pcx_game_button *button)
{
        pcx_free((char *) button->text);
        pcx_free((char *) button->data);
}

static void
free_message(struct pcx_conversation_message *message)
{
        for (unsigned i = 0; i < message->n_buttons; i++)
                destroy_button(message->buttons + i);

        pcx_free(message->text);
        pcx_free(message->buttons);
        pcx_free(message);
}

void
pcx_conversation_unref(struct pcx_conversation *conv)
{
        if (--conv->ref_count > 0)
                return;

        if (conv->game)
                conv->game_type->free_game_cb(conv->game);

        struct pcx_conversation_message *message, *tmp;

        pcx_list_for_each_safe(message, tmp, &conv->messages, link) {
                free_message(message);
        }

        pcx_free(conv);
}
