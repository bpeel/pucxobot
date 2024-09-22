/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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

#ifndef PCX_MESSAGE_QUEUE_H
#define PCX_MESSAGE_QUEUE_H

#include <json_object.h>
#include <stdbool.h>
#include <stdint.h>

struct pcx_message_queue;

struct pcx_message_queue *
pcx_message_queue_new(void);

/* This takes a reference on args */
void
pcx_message_queue_add(struct pcx_message_queue *mq,
                      int64_t chat_id,
                      struct json_object *args);

/* If there is a message that is ready to send immediately, then it is
 * returned. The caller owns a reference on the object and is expected
 * to unref it later. Otherwise if no message is ready then
 * *has_delayed_message will be set to true and *send_delay will be
 * the number of millisecnds wait until the next message will be
 * ready.
 */
struct json_object *
pcx_message_queue_get(struct pcx_message_queue *mq,
                      bool *has_delayed_message,
                      uint64_t *send_delay);

void
pcx_message_queue_free(struct pcx_message_queue *mq);

#endif /* PCX_MESSAGE_QUEUE_H */
