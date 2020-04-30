/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2013, 2015, 2020  Neil Roberts
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

#ifndef PCX_CONNECTION_H
#define PCX_CONNECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "pcx-error.h"
#include "pcx-netaddress.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"
#include "pcx-signal.h"
#include "pcx-player.h"

enum pcx_connection_event_type {
        PCX_CONNECTION_EVENT_ERROR,

        PCX_CONNECTION_EVENT_NEW_PLAYER,
        PCX_CONNECTION_EVENT_RECONNECT,
        PCX_CONNECTION_EVENT_LEAVE,
        PCX_CONNECTION_EVENT_BUTTON,
        PCX_CONNECTION_EVENT_SEND_MESSAGE,
};

struct pcx_connection_event {
        enum pcx_connection_event_type type;
        struct pcx_connection *connection;
};

struct pcx_connection_new_player_event {
        struct pcx_connection_event base;
        const char *name;
        const struct pcx_game *game_type;
        enum pcx_text_language language;
};

struct pcx_connection_reconnect_event {
        struct pcx_connection_event base;
        uint64_t player_id;
        uint16_t n_messages_received;
};

struct pcx_connection_button_event {
        struct pcx_connection_event base;
        const char *button_data;
};

struct pcx_connection_send_message_event {
        struct pcx_connection_event base;
        const char *text;
};

struct pcx_connection;

struct pcx_connection *
pcx_connection_accept(int server_sock,
                      struct pcx_error **error);

void
pcx_connection_free(struct pcx_connection *conn);

struct pcx_signal *
pcx_connection_get_event_signal(struct pcx_connection *conn);

const char *
pcx_connection_get_remote_address_string(struct pcx_connection *conn);

const struct pcx_netaddress *
pcx_connection_get_remote_address(struct pcx_connection *conn);

struct pcx_player *
pcx_connection_get_player(struct pcx_connection *conn);

void
pcx_connection_set_player(struct pcx_connection *conn,
                          struct pcx_player *player,
                          bool from_reconnect,
                          int n_messages_received);

uint64_t
pcx_connection_get_last_update_time(struct pcx_connection *conn);

#endif /* PCX_CONNECTION_H */
