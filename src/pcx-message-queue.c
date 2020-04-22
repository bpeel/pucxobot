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

#include "pcx-message-queue.h"

#include "pcx-util.h"
#include "pcx-list.h"
#include "pcx-main-context.h"

/* Number of microsecends for each period in which to limit messages */
#define LIMIT_PERIOD 60000000
/* Maximum number of messages that we will send during that period
 * before waiting until the next one. The Telegram docs say that for
 * group messages the limit is 20 per minute, so this gives a bit of
 * leeway.
 */
#define LIMIT_AMOUNT 18

struct pcx_message_queue_message {
        struct pcx_list link;
        struct json_object *args;
};

struct pcx_message_queue_chat {
        struct pcx_list link;
        int64_t chat_id;

        /* Queue of pcx_message_queue_messages for this chat */
        struct pcx_list queue;

        /* Timestamp of the first message sent in the last one-minute
         * period.
         */
        uint64_t period_start;
        int n_messages_in_period;
};

struct pcx_message_queue {
        struct pcx_list chats;
};

static void
remove_message(struct pcx_message_queue_message *message)
{
        json_object_put(message->args);
        pcx_list_remove(&message->link);
        pcx_free(message);
}

static void
remove_chat(struct pcx_message_queue_chat *chat)
{
        struct pcx_message_queue_message *message, *tmp;

        pcx_list_for_each_safe(message, tmp, &chat->queue, link) {
                remove_message(message);
        }

        pcx_list_remove(&chat->link);
        pcx_free(chat);
}

struct pcx_message_queue *
pcx_message_queue_new(void)
{
        struct pcx_message_queue *mq = pcx_calloc(sizeof *mq);

        pcx_list_init(&mq->chats);

        return mq;
}

void
pcx_message_queue_add(struct pcx_message_queue *mq,
                      int64_t chat_id,
                      struct json_object *args)
{
        struct pcx_message_queue_chat *chat, *tmp;
        uint64_t now = pcx_main_context_get_monotonic_clock(NULL);

        pcx_list_for_each_safe(chat, tmp, &mq->chats, link) {
                if (chat->chat_id == chat_id)
                        goto found_chat;

                /* If the chat is empty then we can remove it in order
                 * to avoid keeping dead chats around forever.
                 */
                if (now - chat->period_start >= LIMIT_PERIOD &&
                    pcx_list_empty(&chat->queue))
                        remove_chat(chat);
        }

        chat = pcx_calloc(sizeof *chat);
        chat->chat_id = chat_id;
        pcx_list_init(&chat->queue);
        pcx_list_insert(&mq->chats, &chat->link);

found_chat: (void) 0;

        struct pcx_message_queue_message *message = pcx_calloc(sizeof *message);

        message->args = json_object_get(args);
        pcx_list_insert(chat->queue.prev, &message->link);
}

static struct json_object *
unqueue_message(struct pcx_message_queue_chat *chat)
{
        struct pcx_message_queue_message *message =
                pcx_container_of(chat->queue.next,
                                 struct pcx_message_queue_message,
                                 link);

        struct json_object *args = json_object_get(message->args);

        remove_message(message);

        return args;
}

struct json_object *
pcx_message_queue_get(struct pcx_message_queue *mq,
                      bool *has_delayed_message,
                      uint64_t *send_delay)
{
        struct pcx_message_queue_chat *chat;

        uint64_t now = pcx_main_context_get_monotonic_clock(NULL);

        uint64_t next_send_time = UINT64_MAX;

        pcx_list_for_each(chat, &mq->chats, link) {
                if (pcx_list_empty(&chat->queue))
                        continue;

                if (chat->n_messages_in_period < LIMIT_AMOUNT ||
                    now - chat->period_start >= LIMIT_PERIOD) {
                        struct json_object *args = unqueue_message(chat);

                        if (chat->n_messages_in_period <= 0 ||
                            now - chat->period_start >= LIMIT_PERIOD) {
                                chat->period_start = now;
                                chat->n_messages_in_period = 1;
                        } else {
                                chat->n_messages_in_period++;
                        }

                        return args;
                }

                uint64_t this_next_send_time =
                        chat->period_start + LIMIT_PERIOD;

                if (this_next_send_time < next_send_time)
                        next_send_time = this_next_send_time;
        }

        if (next_send_time < UINT64_MAX) {
                *has_delayed_message = true;
                *send_delay = (next_send_time - now) / 1000 + 1;
        } else {
                *has_delayed_message = false;
        }

        return NULL;
}

void
pcx_message_queue_free(struct pcx_message_queue *mq)
{
        struct pcx_message_queue_chat *chat, *tmp;

        pcx_list_for_each_safe(chat, tmp, &mq->chats, link) {
                remove_chat(chat);
        }

        pcx_free(mq);
}
