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
#include <string.h>

#include "pcx-log.h"
#include "pcx-util.h"
#include "pcx-proto.h"

struct pcx_conversation *
pcx_conversation_new(const struct pcx_config *config,
                     const struct pcx_game *game_type)
{
        struct pcx_conversation *conv = pcx_calloc(sizeof *conv);

        conv->ref_count = 1;
        conv->game_type = game_type;
        conv->config = config;
        conv->language = PCX_TEXT_LANGUAGE_ESPERANTO;

        pcx_list_init(&conv->messages);
        pcx_signal_init(&conv->event_signal);

        return conv;
}

static bool
emit_event_with_data(struct pcx_conversation *conv,
                     enum pcx_conversation_event_type type,
                     struct pcx_conversation_event *event)
{
        event->type = type;
        event->conversation = conv;

        pcx_conversation_ref(conv);

        bool ret = pcx_signal_emit(&conv->event_signal, event);

        pcx_conversation_unref(conv);

        return ret;
}

static bool
emit_event(struct pcx_conversation *conv,
           enum pcx_conversation_event_type type)
{
        struct pcx_conversation_event event;

        return emit_event_with_data(conv, type, &event);
}

static void
add_string(uint8_t **p, const char *s)
{
        int len = strlen(s) + 1;
        memcpy(*p, s, len);
        *p += len;
}

static void
queue_message(struct pcx_conversation *conv,
              const struct pcx_game_message *message)
{
        size_t payload_length = 1 + strlen(message->text) + 1;

        for (unsigned i = 0; i < message->n_buttons; i++) {
                payload_length +=
                        strlen(message->buttons[i].text) + 1 +
                        strlen(message->buttons[i].data) + 1;
        }

        uint8_t *buf = pcx_alloc(payload_length);
        uint8_t *p = buf;

        *p = message->format == PCX_GAME_MESSAGE_FORMAT_HTML ? 1 : 0;

        if (message->target != -1)
                *p |= 2;

        p++;

        add_string(&p, message->text);

        size_t no_buttons_length = p - buf;

        for (unsigned i = 0; i < message->n_buttons; i++) {
                add_string(&p, message->buttons[i].text);
                add_string(&p, message->buttons[i].data);
        }

        assert(p - buf == payload_length);

        struct pcx_conversation_message *cmessage =
                pcx_calloc(sizeof *cmessage);

        cmessage->target_player = message->target;
        cmessage->button_players = message->button_players;
        cmessage->data = buf;
        cmessage->length = payload_length;
        cmessage->no_buttons_length = no_buttons_length;

        pcx_list_insert(conv->messages.prev, &cmessage->link);

        emit_event(conv, PCX_CONVERSATION_EVENT_NEW_MESSAGE);
}

static void
send_message_cb(const struct pcx_game_message *message,
                void *user_data)
{
        struct pcx_conversation *conv = user_data;

        assert(message->target >= -1 && message->target < conv->n_players);

        queue_message(conv, message);
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
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
append_current_players_message(struct pcx_conversation *conv,
                               struct pcx_buffer *buf)
{
        pcx_buffer_append_string(buf,
                                 pcx_text_get(conv->language,
                                              PCX_TEXT_STRING_CURRENT_PLAYERS));
        pcx_buffer_append_string(buf, "\n");

        const char *final_separator =
                pcx_text_get(conv->language,
                             PCX_TEXT_STRING_FINAL_CONJUNCTION);

        for (unsigned i = 0; i < conv->n_players; i++) {
                if (i > 0) {
                        if (i == conv->n_players - 1)
                                pcx_buffer_append_string(buf, final_separator);
                        else
                                pcx_buffer_append_string(buf, ", ");
                }
                pcx_buffer_append_string(buf, conv->player_names[i]);
        }
}

static void
send_welcome_message(struct pcx_conversation *conv,
                     int new_player_num)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        enum pcx_text_string welcome_note =
                conv->n_players < conv->game_type->min_players ?
                PCX_TEXT_STRING_WELCOME_BUTTONS_TOO_FEW :
                conv->n_players < conv->game_type->max_players ?
                PCX_TEXT_STRING_WELCOME_BUTTONS :
                PCX_TEXT_STRING_WELCOME_BUTTONS_FULL;

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(conv->language,
                                              welcome_note),
                                 conv->player_names[new_player_num]);

        if (conv->n_players < conv->game_type->max_players) {
                pcx_buffer_append_string(&buf, "\n\n");
                append_current_players_message(conv, &buf);
        }

        struct pcx_game_button start_button = {
                .text = pcx_text_get(conv->language,
                                     PCX_TEXT_STRING_START_BUTTON),
                .data = "start",
        };

        int n_buttons = ((conv->n_players >= conv->game_type->min_players &&
                          conv->n_players < conv->game_type->max_players) ?
                         1 :
                         0);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.n_buttons = n_buttons;
        message.buttons = &start_button;

        queue_message(conv, &message);

        pcx_buffer_destroy(&buf);
}

int
pcx_conversation_add_player(struct pcx_conversation *conv,
                            const char *name)
{
        assert(conv->n_players < conv->game_type->max_players);
        assert(!conv->started);

        int player_num = conv->n_players++;

        conv->player_names[player_num] = pcx_strdup(name);

        pcx_conversation_ref(conv);

        emit_event(conv, PCX_CONVERSATION_EVENT_PLAYER_ADDED);

        send_welcome_message(conv, player_num);

        if (conv->n_players >= conv->game_type->max_players)
                pcx_conversation_start(conv);

        pcx_conversation_unref(conv);

        return player_num;
}

void
pcx_conversation_remove_player(struct pcx_conversation *conv,
                               int player_num)
{
        struct pcx_conversation_player_removed_event event = {
                .player_num = player_num
        };

        emit_event_with_data(conv,
                             PCX_CONVERSATION_EVENT_PLAYER_REMOVED,
                             &event.base);
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

        conv->game =
                conv->game_type->create_game_cb(conv->config,
                                                &game_callbacks,
                                                conv,
                                                conv->language,
                                                conv->n_players,
                                                (const char * const *)
                                                conv->player_names);

        pcx_conversation_unref(conv);
}

void
pcx_conversation_push_button(struct pcx_conversation *conv,
                             int player_num,
                             const char *button_data)
{
        assert(player_num >= 0 && player_num < conv->n_players);

        if (!strcmp(button_data, "start")) {
                pcx_conversation_start(conv);
        } else if (conv->game != NULL) {
                pcx_conversation_ref(conv);

                conv->game_type->handle_callback_data_cb(conv->game,
                                                         player_num,
                                                         button_data);

                pcx_conversation_unref(conv);
        }
}

void
pcx_conversation_ref(struct pcx_conversation *conv)
{
        conv->ref_count++;
}

static void
free_message(struct pcx_conversation_message *message)
{
        pcx_free(message->data);
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

        for (int i = 0; i < conv->n_players; i++)
                pcx_free(conv->player_names[i]);

        pcx_free(conv);
}
