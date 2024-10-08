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

#include "pcx-conversation.h"

#include <assert.h>
#include <string.h>

#include "pcx-log.h"
#include "pcx-util.h"
#include "pcx-proto.h"
#include "pcx-html.h"

struct pcx_conversation *
pcx_conversation_new(const struct pcx_config *config,
                     struct pcx_class_store *class_store,
                     const struct pcx_game *game_type,
                     enum pcx_text_language language)
{
        struct pcx_conversation *conv = pcx_calloc(sizeof *conv);

        pcx_buffer_init(&conv->player_names);

        conv->ref_count = 1;
        conv->game_type = game_type;
        conv->language = language;
        conv->config = config;
        conv->class_store = class_store;

        pcx_list_init(&conv->messages);
        pcx_signal_init(&conv->event_signal);

        return conv;
}

const char *
pcx_conversation_get_player_name(struct pcx_conversation *conv,
                                 int player_num)
{
        assert(player_num >= 0 && player_num < conv->n_players);
        return ((const char **) conv->player_names.data)[player_num];
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
              const struct pcx_game_message *message,
              int sending_player)
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
                *p |= PCX_PROTO_MESSAGE_TYPE_PRIVATE << 1;

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
        cmessage->sending_player = sending_player;
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

        queue_message(conv, message, -1 /* sending_player */);
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

static struct pcx_class_store *
get_class_store_cb(void *user_data)
{
        struct pcx_conversation *conv = user_data;

        return conv->class_store;
}

struct pcx_conversation_sideband_data *
pcx_conversation_get_sideband_data(struct pcx_conversation *conv,
                                   int data_num)
{
        return ((struct pcx_conversation_sideband_data *)
                conv->sideband_data.data +
                data_num);
}

static void
destroy_sideband_data(struct pcx_conversation_sideband_data *data)
{
        switch (data->type) {
        case PCX_GAME_SIDEBAND_TYPE_BYTE:
        case PCX_GAME_SIDEBAND_TYPE_UINT32:
                break;
        case PCX_GAME_SIDEBAND_TYPE_STRING:
                pcx_free(data->string);
                break;
        }
}

static struct pcx_conversation_sideband_data *
get_or_create_sideband_data(struct pcx_conversation *conv,
                            int data_num,
                            bool *created)
{
        assert(data_num >= 0 &&
               data_num < 8 * sizeof (conv->available_sideband_data));

        size_t new_length = ((data_num + 1) *
                             sizeof (struct pcx_conversation_sideband_data));

        if (new_length > conv->sideband_data.length)
                pcx_buffer_set_length(&conv->sideband_data, new_length);

        struct pcx_conversation_sideband_data *data =
                pcx_conversation_get_sideband_data(conv, data_num);

        if ((conv->available_sideband_data & (UINT64_C(1) << data_num)) == 0) {
                conv->available_sideband_data |= UINT64_C(1) << data_num;
                *created = true;
        } else {
                *created = false;
        }

        return data;
}

static bool
set_sideband_byte(struct pcx_conversation *conv,
                  int data_num,
                  uint8_t value)
{
        bool created;
        struct pcx_conversation_sideband_data *data =
                get_or_create_sideband_data(conv, data_num, &created);

        if (!created) {
                if (data->type == PCX_GAME_SIDEBAND_TYPE_BYTE) {
                        if (data->byte == value)
                                return false;
                } else {
                        destroy_sideband_data(data);
                }
        }

        data->type = PCX_GAME_SIDEBAND_TYPE_BYTE;
        data->byte = value;

        return true;
}

static bool
set_sideband_uint32(struct pcx_conversation *conv,
                    int data_num,
                    uint32_t value)
{
        bool created;
        struct pcx_conversation_sideband_data *data =
                get_or_create_sideband_data(conv, data_num, &created);

        if (!created) {
                if (data->type == PCX_GAME_SIDEBAND_TYPE_UINT32) {
                        if (data->uint32 == value)
                                return false;
                } else {
                        destroy_sideband_data(data);
                }
        }

        data->type = PCX_GAME_SIDEBAND_TYPE_UINT32;
        data->uint32 = value;

        return true;
}

static bool
set_sideband_string(struct pcx_conversation *conv,
                    int data_num,
                    const char *value)
{
        bool created;
        struct pcx_conversation_sideband_data *data =
                get_or_create_sideband_data(conv, data_num, &created);

        size_t needed_size = strlen(value) + 1;

        bool need_allocate;

        if (created) {
                need_allocate = true;
        } else if (data->type == PCX_GAME_SIDEBAND_TYPE_STRING) {
                if (!strcmp(data->string->text, value))
                        return false;

                if (data->string->size < needed_size) {
                        destroy_sideband_data(data);
                        need_allocate = true;
                } else {
                        need_allocate = false;
                }
        } else {
                destroy_sideband_data(data);
                need_allocate = true;
        }

        if (need_allocate) {
                size_t alloc_size =
                        (offsetof(struct pcx_conversation_sideband_string,
                                  text) +
                         needed_size);
                data->string = pcx_alloc(alloc_size);
                data->string->size = needed_size;
                data->type = PCX_GAME_SIDEBAND_TYPE_STRING;
        }

        memcpy(data->string->text, value, needed_size);

        return true;
}

static void
set_sideband_data_cb(int data_num,
                     const struct pcx_game_sideband_data *value,
                     bool force,
                     void *user_data)
{
        struct pcx_conversation *conv = user_data;

        bool modified;

        switch (value->type) {
        case PCX_GAME_SIDEBAND_TYPE_BYTE:
                modified = set_sideband_byte(conv, data_num, value->byte);
                goto found_type;
        case PCX_GAME_SIDEBAND_TYPE_UINT32:
                modified = set_sideband_uint32(conv, data_num, value->uint32);
                goto found_type;
        case PCX_GAME_SIDEBAND_TYPE_STRING:
                modified = set_sideband_string(conv, data_num, value->string);
                goto found_type;
        }

        assert(!"unknown sideband data type");
        return;

found_type:
        if (!modified && !force)
                return;

        struct pcx_conversation_sideband_data_modified_event event = {
                .data_num = data_num,
        };

        emit_event_with_data(conv,
                             PCX_CONVERSATION_EVENT_SIDEBAND_DATA_MODIFIED,
                             &event.base);
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_message = send_message_cb,
        .game_over = game_over_cb,
        .get_class_store = get_class_store_cb,
        .set_sideband_data = set_sideband_data_cb,
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
                const char *name = pcx_conversation_get_player_name(conv, i);
                pcx_buffer_append_string(buf, name);
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

        const char *name =
                pcx_conversation_get_player_name(conv, new_player_num);

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(conv->language,
                                              welcome_note),
                                 name);

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

        queue_message(conv, &message, -1 /* sending_player */);

        pcx_buffer_destroy(&buf);
}

int
pcx_conversation_add_player(struct pcx_conversation *conv,
                            const char *name)
{
        assert(conv->n_players < conv->game_type->max_players);
        assert(!conv->started);

        int player_num = conv->n_players++;

        char *name_copy = pcx_strdup(name);

        pcx_buffer_append(&conv->player_names, &name_copy, sizeof name_copy);

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
        pcx_conversation_ref(conv);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *name = pcx_conversation_get_player_name(conv, player_num);

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(conv->language,
                                              PCX_TEXT_STRING_PLAYER_LEFT),
                                 name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;

        queue_message(conv, &message, -1 /* sending_player */);

        pcx_buffer_destroy(&buf);

        struct pcx_conversation_player_removed_event event = {
                .player_num = player_num
        };

        emit_event_with_data(conv,
                             PCX_CONVERSATION_EVENT_PLAYER_REMOVED,
                             &event.base);

        pcx_conversation_unref(conv);
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
                                                conv->player_names.data);

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
pcx_conversation_add_chat_message(struct pcx_conversation *conv,
                                  int player_num,
                                  const char *text)
{
        assert(player_num >= 0 && player_num < conv->n_players);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *name = pcx_conversation_get_player_name(conv, player_num);

        pcx_buffer_append_string(&buf, "<b>");
        pcx_html_escape(&buf, name);
        pcx_buffer_append_string(&buf, "</b>\n\n");
        pcx_html_escape(&buf, text);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) buf.data;
        message.format = PCX_GAME_MESSAGE_FORMAT_HTML;

        queue_message(conv, &message, player_num);

        if (conv->game != NULL && conv->game_type->handle_message_cb) {
                pcx_conversation_ref(conv);

                conv->game_type->handle_message_cb(conv->game,
                                                   player_num,
                                                   text);

                pcx_conversation_unref(conv);
        }

        pcx_buffer_destroy(&buf);
}

void
pcx_conversation_set_sideband(struct pcx_conversation *conv,
                              int player_num,
                              int data_num,
                              const char *text)
{
        assert(player_num >= 0 && player_num < conv->n_players);

        if (conv->game != NULL && conv->game_type->handle_sideband_cb) {
                pcx_conversation_ref(conv);

                conv->game_type->handle_sideband_cb(conv->game,
                                                    player_num,
                                                    data_num,
                                                    text);

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

static void
destroy_all_sideband_data(struct pcx_conversation *conv)
{
        uint64_t bits = conv->available_sideband_data;

        while (true) {
                int bit = ffsll(bits);

                if (bit == 0)
                        break;

                bit--;

                struct pcx_conversation_sideband_data *data =
                        (struct pcx_conversation_sideband_data *)
                        conv->sideband_data.data +
                        bit;

                destroy_sideband_data(data);

                bits &= ~(UINT64_C(1) << bit);
        }

        pcx_buffer_destroy(&conv->sideband_data);
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

        char **player_names = (char **) conv->player_names.data;

        for (int i = 0; i < conv->n_players; i++)
                pcx_free(player_names[i]);

        pcx_buffer_destroy(&conv->player_names);

        destroy_all_sideband_data(conv);

        pcx_free(conv);
}
