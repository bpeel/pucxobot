/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2015, 2020  Neil Roberts
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

#ifndef PCX_PROTO_H
#define PCX_PROTO_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "pcx-util.h"

#define PCX_PROTO_DEFAULT_PORT 3468

/* Maximum number of bytes allowed in a payload. The server keeps a
 * buffer of this size around for each connection, so we don’t want it
 * to be too large.
 */
#define PCX_PROTO_MAX_PAYLOAD_SIZE 1024

/* The WebSocket protocol says that a control frame payload can not be
 * longer than 125 bytes.
 */
#define PCX_PROTO_MAX_CONTROL_FRAME_PAYLOAD 125

#define PCX_PROTO_MAX_FRAME_HEADER_LENGTH (1 + 1 + 8 + 4)

#define PCX_PROTO_NEW_PLAYER 0x80
#define PCX_PROTO_NEW_PRIVATE_PLAYER 0x86
#define PCX_PROTO_RECONNECT 0x81
#define PCX_PROTO_BUTTON 0x82
#define PCX_PROTO_KEEP_ALIVE 0x83
#define PCX_PROTO_LEAVE 0x84
#define PCX_PROTO_SEND_MESSAGE 0x85

#define PCX_PROTO_PLAYER_ID 0x00
#define PCX_PROTO_MESSAGE 0x01
#define PCX_PROTO_GAME_TYPE 0x02

enum pcx_proto_type {
        PCX_PROTO_TYPE_UINT8,
        PCX_PROTO_TYPE_UINT16,
        PCX_PROTO_TYPE_UINT32,
        PCX_PROTO_TYPE_UINT64,
        PCX_PROTO_TYPE_BLOB,
        PCX_PROTO_TYPE_STRING,
        PCX_PROTO_TYPE_NONE
};

enum pcx_proto_message_type {
        PCX_PROTO_MESSAGE_TYPE_PUBLIC,
        PCX_PROTO_MESSAGE_TYPE_PRIVATE,
        PCX_PROTO_MESSAGE_TYPE_CHAT_OTHER,
        PCX_PROTO_MESSAGE_TYPE_CHAT_YOU,
};

static inline void
pcx_proto_write_uint8_t(uint8_t *buffer,
                        uint8_t value)
{
        *buffer = value;
}

static inline void
pcx_proto_write_uint16_t(uint8_t *buffer,
                         uint16_t value)
{
        value = PCX_UINT16_TO_LE(value);
        memcpy(buffer, &value, sizeof value);
}

static inline void
pcx_proto_write_uint32_t(uint8_t *buffer,
                         uint32_t value)
{
        value = PCX_UINT32_TO_LE(value);
        memcpy(buffer, &value, sizeof value);
}

static inline void
pcx_proto_write_uint64_t(uint8_t *buffer,
                         uint64_t value)
{
        value = PCX_UINT64_TO_LE(value);
        memcpy(buffer, &value, sizeof value);
}

int
pcx_proto_write_command_v(uint8_t *buffer,
                          size_t buffer_length,
                          uint8_t command,
                          va_list ap);

int
pcx_proto_write_command(uint8_t *buffer,
                        size_t buffer_length,
                        uint8_t command,
                        ...);

static inline uint8_t
pcx_proto_read_uint8_t(const uint8_t *buffer)
{
        return *buffer;
}

static inline uint16_t
pcx_proto_read_uint16_t(const uint8_t *buffer)
{
        uint16_t value;
        memcpy(&value, buffer, sizeof value);
        return PCX_UINT16_FROM_LE(value);
}

static inline uint32_t
pcx_proto_read_uint32_t(const uint8_t *buffer)
{
        uint32_t value;
        memcpy(&value, buffer, sizeof value);
        return PCX_UINT32_FROM_LE(value);
}

static inline uint64_t
pcx_proto_read_uint64_t(const uint8_t *buffer)
{
        uint64_t value;
        memcpy(&value, buffer, sizeof value);
        return PCX_UINT64_FROM_LE(value);
}

bool
pcx_proto_read_payload(const uint8_t *buffer,
                       size_t length,
                       ...);

size_t
pcx_proto_get_frame_header_length(size_t payload_length);

void
pcx_proto_write_frame_header(uint8_t *buffer,
                             size_t payload_length);

#endif /* PCX_PROTO_H */
