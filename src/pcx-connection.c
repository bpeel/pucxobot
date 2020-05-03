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

#include "config.h"

#include "pcx-connection.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <stdarg.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-buffer.h"
#include "pcx-log.h"
#include "pcx-file-error.h"
#include "pcx-socket.h"
#include "pcx-main-context.h"
#include "pcx-ws-parser.h"
#include "pcx-base64.h"
#include "pcx-proto.h"
#include "sha1.h"

struct pcx_connection {
        struct pcx_netaddress remote_address;
        char *remote_address_string;
        struct pcx_main_context_source *socket_source;
        int sock;

        uint8_t read_buf[1024];
        size_t read_buf_pos;

        uint8_t write_buf[1024];
        size_t write_buf_pos;

        struct pcx_player *player;
        struct pcx_listener conversation_listener;

        bool sent_conversation_details;

        /* If pong_queued is non-zero then pong_data then we need to
         * send a pong control frame with the payload given payload.
         */
        bool pong_queued;
        _Static_assert(PCX_PROTO_MAX_CONTROL_FRAME_PAYLOAD <= UINT8_MAX,
                       "The max pong data length is too for a uint8_t");
        uint8_t pong_data_length;
        uint8_t pong_data[PCX_PROTO_MAX_CONTROL_FRAME_PAYLOAD];

        /* If message_data_length is non-zero then we are part way
         * through reading a message whose payload is stored in
         * message_data.
         */
        _Static_assert(PCX_PROTO_MAX_PAYLOAD_SIZE <= UINT16_MAX,
                       "The message size is too long for a uint16_t");
        uint16_t message_data_length;
        uint8_t message_data[PCX_PROTO_MAX_PAYLOAD_SIZE];

        struct pcx_signal event_signal;

        /* Last monotonic clock time when data was received on this
         * connection. This is used for garbage collection.
         */
        uint64_t last_update_time;

        /* This is freed and becomes NULL once the headers have all
         * been parsed.
         */
        struct pcx_ws_parser *ws_parser;
        /* This is allocated temporarily between getting the WebSocket
         * key header and finishing all of the headers.
         */
        SHA1_CTX *sha1_ctx;

        /* A pointer to the list node of the last message that was
         * sent from the conversation. This point to the header node
         * if no messages have been sent yet.
         */
        struct pcx_list *last_message_sent;
};

static const char
ws_sec_key_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static const char
ws_header_prefix[] =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: ";

static const char
ws_header_postfix[] = "\r\n\r\n";

static bool
emit_event(struct pcx_connection *conn,
           enum pcx_connection_event_type type,
           struct pcx_connection_event *event)
{
        event->type = type;
        event->connection = conn;
        return pcx_signal_emit(&conn->event_signal, event);
}

static void
remove_sources(struct pcx_connection *conn)
{
        if (conn->socket_source) {
                pcx_main_context_remove_source(conn->socket_source);
                conn->socket_source = NULL;
        }
}

static void
set_error_state(struct pcx_connection *conn)
{
        struct pcx_connection_event event;

        /* Stop polling for further events */
        remove_sources(conn);

        emit_event(conn,
                   PCX_CONNECTION_EVENT_ERROR,
                   &event);
}

static void
set_last_update_time(struct pcx_connection *conn)
{
        uint64_t now = pcx_main_context_get_monotonic_clock(NULL);

        conn->last_update_time = now;

        if (conn->player)
                conn->player->last_update_time = now;
}

static void
handle_error(struct pcx_connection *conn)
{
        int value;
        unsigned int value_len = sizeof(value);

        if (getsockopt(conn->sock,
                       SOL_SOCKET,
                       SO_ERROR,
                       &value,
                       &value_len) == -1 ||
            value_len != sizeof(value) ||
            value == 0) {
                pcx_log("Unknown error on socket for %s",
                        conn->remote_address_string);
        } else {
                pcx_log("Error on socket for %s: %s",
                        conn->remote_address_string,
                        strerror(value));
        }

        set_error_state(conn);
}

static bool
connection_is_ready_to_write(struct pcx_connection *conn)
{
        if (conn->write_buf_pos > 0)
                return true;

        if (conn->pong_queued)
                return true;

        if (conn->player) {
                if (!conn->sent_conversation_details)
                        return true;

                /* If the last message we sent isn’t the last one then
                 * we have messages to send.
                 */
                if (!conn->player->has_left &&
                    conn->last_message_sent->next !=
                    &conn->player->conversation->messages)
                        return true;
        }

        return false;
}

static void
update_poll_flags(struct pcx_connection *conn)
{
        enum pcx_main_context_poll_flags flags = PCX_MAIN_CONTEXT_POLL_IN;

        if (connection_is_ready_to_write(conn))
                flags |= PCX_MAIN_CONTEXT_POLL_OUT;

        pcx_main_context_modify_poll(conn->socket_source, flags);
}

static int
write_command(struct pcx_connection *conn,
              uint16_t command,
              ...)
{
        int ret;
        va_list ap;

        va_start(ap, command);

        ret = pcx_proto_write_command_v(conn->write_buf +
                                        conn->write_buf_pos,
                                        sizeof conn->write_buf -
                                        conn->write_buf_pos,
                                        command,
                                        ap);

        va_end(ap);

        return ret;
}

static bool
write_messages(struct pcx_connection *conn)
{
        struct pcx_conversation *conv = conn->player->conversation;

        for (; conn->last_message_sent->next != &conv->messages;
             conn->last_message_sent = conn->last_message_sent->next) {
                struct pcx_conversation_message *message =
                        pcx_container_of(conn->last_message_sent->next,
                                         struct pcx_conversation_message,
                                         link);

                if (message->target_player != -1 &&
                    message->target_player != conn->player->player_num)
                        continue;

                size_t length = (message->target_player == -1 &&
                                 ((UINT32_C(1) << conn->player->player_num) &
                                  message->button_players) == 0 ?
                                 message->no_buttons_length :
                                 message->length);

                size_t header_length =
                        pcx_proto_get_frame_header_length(length + 1);

                if (header_length + length + 1 + conn->write_buf_pos >
                    sizeof conn->write_buf)
                        return false;

                uint8_t *p = conn->write_buf + conn->write_buf_pos;

                pcx_proto_write_frame_header(p, length + 1);
                p += header_length;

                *(p++) = PCX_PROTO_MESSAGE;

                memcpy(p, message->data, length);

                /* Tweak the message type depending on who it’s being
                 * sent to
                 */
                if (message->sending_player != -1) {
                        enum pcx_proto_message_type message_type =
                                message->sending_player ==
                                conn->player->player_num ?
                                PCX_PROTO_MESSAGE_TYPE_CHAT_YOU :
                                PCX_PROTO_MESSAGE_TYPE_CHAT_OTHER;
                        *p |= message_type << 1;
                }

                p += length;

                conn->write_buf_pos = p - conn->write_buf;
        }

        return true;
}

static bool
write_conversation_details(struct pcx_connection *conn)
{
        size_t old_write_buf_pos = conn->write_buf_pos;
        int wrote;

        struct pcx_conversation *conv = conn->player->conversation;

        wrote = write_command(conn,

                              PCX_PROTO_PLAYER_ID,

                              PCX_PROTO_TYPE_UINT64,
                              conn->player->id,

                              PCX_PROTO_TYPE_NONE);

        if (wrote == -1)
                goto failed;

        conn->write_buf_pos += wrote;

        wrote = write_command(conn,

                              PCX_PROTO_GAME_TYPE,

                              PCX_PROTO_TYPE_STRING,
                              conv->game_type->name,

                              PCX_PROTO_TYPE_NONE);

        if (wrote == -1)
                goto failed;

        conn->write_buf_pos += wrote;

        if (conv->is_private) {
                wrote = write_command(conn,

                                      PCX_PROTO_PRIVATE_GAME_ID,

                                      PCX_PROTO_TYPE_UINT64,
                                      conv->private_game_id,

                                      PCX_PROTO_TYPE_NONE);
                if (wrote == -1)
                        goto failed;

                conn->write_buf_pos += wrote;
        }

        conn->sent_conversation_details = true;

        return true;

failed:
        conn->write_buf_pos = old_write_buf_pos;
        return false;
}

static bool
write_pong(struct pcx_connection *conn)
{
        if (conn->write_buf_pos + conn->pong_data_length + 2 >
            sizeof (conn->write_buf))
                return false;

        /* FIN bit + opcode 0xa (pong) */
        conn->write_buf[conn->write_buf_pos++] = 0x8a;
        conn->write_buf[conn->write_buf_pos++] = conn->pong_data_length;
        memcpy(conn->write_buf + conn->write_buf_pos,
               conn->pong_data,
               conn->pong_data_length);
        conn->write_buf_pos += conn->pong_data_length;
        conn->pong_queued = false;

        return true;
}

static void
fill_write_buf(struct pcx_connection *conn)
{
        if (conn->pong_queued && !write_pong(conn))
                return;

        if (conn->player == NULL)
                return;

        if (!conn->sent_conversation_details &&
            !write_conversation_details(conn))
                return;

        if (!conn->player->has_left &&
            !write_messages(conn))
                return;
}

static bool
process_control_frame(struct pcx_connection *conn,
                      int opcode,
                      const uint8_t *data,
                      size_t data_length)
{
        switch (opcode) {
        case 0x8:
                pcx_log("Client %s sent a close control frame",
                       conn->remote_address_string);
                set_error_state(conn);
                return false;
        case 0x9:
                assert(data_length < sizeof conn->pong_data);
                memcpy(conn->pong_data, data, data_length);
                conn->pong_data_length = data_length;
                conn->pong_queued = true;
                update_poll_flags(conn);
                break;
        case 0xa:
                /* pong, ignore */
                break;
        default:
                pcx_log("Client %s sent an unknown control frame",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        return true;
}

static bool
handle_new_player(struct pcx_connection *conn,
                  bool is_private)
{
        struct pcx_connection_new_player_event event;
        const char *game_name;
        const char *language_code;

        if (!pcx_proto_read_payload(conn->message_data + 1,
                                    conn->message_data_length - 1,

                                    PCX_PROTO_TYPE_STRING,
                                    &event.name,

                                    PCX_PROTO_TYPE_STRING,
                                    &game_name,

                                    PCX_PROTO_TYPE_STRING,
                                    &language_code,

                                    PCX_PROTO_TYPE_NONE)) {
                pcx_log("Invalid new player command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        for (int i = 0; pcx_game_list[i]; i++) {
                if (!strcmp(pcx_game_list[i]->name, game_name)) {
                        event.game_type = pcx_game_list[i];
                        goto found_game;
                }
        }

        pcx_log("Connection %s tried to choose a game type that "
                "doesn’t exist",
                conn->remote_address_string);
        set_error_state(conn);
        return false;

found_game:

        if (!pcx_text_lookup_language(language_code,
                                      &event.language)) {
                pcx_log("Connection %s tried to use a language code that "
                        "doesn’t exist",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        event.is_private = is_private;

        return emit_event(conn,
                          PCX_CONNECTION_EVENT_NEW_PLAYER,
                          &event.base);
}

static bool
handle_reconnect(struct pcx_connection *conn)
{
        struct pcx_connection_reconnect_event event;

        if (!pcx_proto_read_payload(conn->message_data + 1,
                                    conn->message_data_length - 1,

                                    PCX_PROTO_TYPE_UINT64,
                                    &event.player_id,

                                    PCX_PROTO_TYPE_UINT16,
                                    &event.n_messages_received,

                                    PCX_PROTO_TYPE_NONE)) {
                pcx_log("Invalid reconnect command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        return emit_event(conn,
                          PCX_CONNECTION_EVENT_RECONNECT,
                          &event.base);
}

static bool
handle_leave(struct pcx_connection *conn)
{
        struct pcx_connection_event event;

        if (!pcx_proto_read_payload(conn->message_data + 1,
                                    conn->message_data_length - 1,
                                    PCX_PROTO_TYPE_NONE)) {
                pcx_log("Invalid leave command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        return emit_event(conn,
                          PCX_CONNECTION_EVENT_LEAVE,
                          &event);
}

static bool
handle_button(struct pcx_connection *conn)
{
        struct pcx_connection_button_event event;

        if (conn->message_data_length <= 1) {
                pcx_log("Invalid button command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        char *button_data = pcx_strndup((char *) conn->message_data + 1,
                                        conn->message_data_length - 1);
        event.button_data = button_data;

        emit_event(conn, PCX_CONNECTION_EVENT_BUTTON, &event.base);

        pcx_free(button_data);

        return true;
}

static bool
handle_send_message(struct pcx_connection *conn)
{
        struct pcx_connection_send_message_event event;

        if (!pcx_proto_read_payload(conn->message_data + 1,
                                    conn->message_data_length - 1,

                                    PCX_PROTO_TYPE_STRING,
                                    &event.text,

                                    PCX_PROTO_TYPE_NONE)) {
                pcx_log("Invalid send message command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        return emit_event(conn,
                          PCX_CONNECTION_EVENT_SEND_MESSAGE,
                          &event.base);
}

static bool
handle_keep_alive(struct pcx_connection *conn)
{
        if (!pcx_proto_read_payload(conn->message_data + 1,
                                    conn->message_data_length - 1,
                                    PCX_PROTO_TYPE_NONE)) {
                pcx_log("Invalid keep alive command received from %s",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        return true;
}

static bool
process_message(struct pcx_connection *conn)
{
        switch (conn->message_data[0]) {
        case PCX_PROTO_NEW_PLAYER:
                return handle_new_player(conn, false /* is_private */);
        case PCX_PROTO_NEW_PRIVATE_PLAYER:
                return handle_new_player(conn, true /* is_private */);
        case PCX_PROTO_RECONNECT:
                return handle_reconnect(conn);
        case PCX_PROTO_LEAVE:
                return handle_leave(conn);
        case PCX_PROTO_BUTTON:
                return handle_button(conn);
        case PCX_PROTO_SEND_MESSAGE:
                return handle_send_message(conn);
        case PCX_PROTO_KEEP_ALIVE:
                return handle_keep_alive(conn);
        }

        pcx_log("Client %s sent an unknown message ID (0x%u)",
                conn->remote_address_string,
                conn->message_data[0]);
        set_error_state(conn);

        return false;
}

static void
unmask_data(uint32_t mask,
            uint8_t *buffer,
            size_t buffer_length)
{
        uint32_t val;
        int i;

        for (i = 0; i + sizeof mask <= buffer_length; i += sizeof mask) {
                memcpy(&val, buffer + i, sizeof val);
                val ^= mask;
                memcpy(buffer + i, &val, sizeof val);
        }

        for (; i < buffer_length; i++)
                buffer[i] ^= ((uint8_t *) &mask)[i % 4];
}

static void
process_frames(struct pcx_connection *conn)
{
        uint8_t *data = conn->read_buf;
        size_t length = conn->read_buf_pos;
        bool has_mask;
        bool is_fin;
        uint32_t mask;
        uint64_t payload_length;
        uint8_t opcode;

        while (true) {
                if (length < 2)
                        break;

                int header_size = 2;

                is_fin = data[0] & 0x80;
                opcode = data[0] & 0xf;
                has_mask = data[1] & 0x80;

                payload_length = data[1] & 0x7f;

                if (payload_length == 126) {
                        uint16_t word;
                        if (length < header_size + sizeof word)
                                break;
                        memcpy(&word, data + header_size, sizeof word);
                        payload_length = PCX_UINT16_FROM_BE(word);
                        header_size += sizeof word;
                } else if (payload_length == 127) {
                        if (length < header_size + sizeof payload_length)
                                break;
                        memcpy(&payload_length,
                               data + header_size,
                               sizeof payload_length);
                        payload_length = PCX_UINT64_FROM_BE(payload_length);
                        header_size += sizeof payload_length;
                }

                if (has_mask)
                        header_size += sizeof mask;

                /* RSV bits must be zero */
                if (data[0] & 0x70) {
                        pcx_log("Client %s sent a frame with non-zero "
                                "RSV bits",
                                conn->remote_address_string);
                        set_error_state(conn);
                        return;
                }

                if (opcode & 0x8) {
                        /* Control frame */
                        if (payload_length >
                            PCX_PROTO_MAX_CONTROL_FRAME_PAYLOAD) {
                                pcx_log("Client %s sent a control frame (0x%x) "
                                        "that is too long (%" PRIu64 ")",
                                        conn->remote_address_string,
                                        opcode,
                                        payload_length);
                                set_error_state(conn);
                                return;
                        }
                        if (!is_fin) {
                                pcx_log("Client %s sent a fragmented "
                                        "control frame",
                                        conn->remote_address_string);
                                set_error_state(conn);
                                return;
                        }
                } else if (opcode == 0x2 || opcode == 0x0) {
                        if (payload_length + conn->message_data_length >
                            PCX_PROTO_MAX_PAYLOAD_SIZE) {
                                pcx_log("Client %s sent a message (0x%x) "
                                        "that is too long (%" PRIu64 ")",
                                        conn->remote_address_string,
                                        opcode,
                                        payload_length);
                                set_error_state(conn);
                                return;
                        }
                        if (opcode == 0x0 && conn->message_data_length == 0) {
                                pcx_log("Client %s sent a continuation frame "
                                        "without starting a message",
                                        conn->remote_address_string);
                                set_error_state(conn);
                                return;
                        }
                        if (payload_length == 0 && !is_fin) {
                                pcx_log("Client %s sent an empty fragmented "
                                        "message",
                                        conn->remote_address_string);
                                set_error_state(conn);
                                return;
                        }
                } else {
                        pcx_log("Client %s sent a frame opcode (0x%x) which "
                                "the server doesn't understand",
                                conn->remote_address_string,
                                opcode);
                        set_error_state(conn);
                        return;
                }

                if (payload_length + header_size > length)
                        break;

                data += header_size;
                length -= header_size;

                if (has_mask) {
                        memcpy(&mask, data - sizeof mask, sizeof mask);
                        unmask_data(mask, data, payload_length);
                }

                if (opcode & 0x8) {
                        if (!process_control_frame(conn,
                                                   opcode,
                                                   data,
                                                   payload_length))
                                return;
                } else {
                        memcpy(conn->message_data + conn->message_data_length,
                               data,
                               payload_length);
                        conn->message_data_length += payload_length;

                        if (is_fin) {
                                if (!process_message(conn))
                                        return;

                                conn->message_data_length = 0;
                        }
                }

                data += payload_length;
                length -= payload_length;
        }

        memmove(conn->read_buf, data, length);
        conn->read_buf_pos = length;
}

static bool
ws_request_line_received_cb(const char *method,
                            const char *uri,
                            void *user_data)
{
        return true;
}

static bool
ws_header_received_cb(const char *field_name,
                      const char *value,
                      void *user_data)
{
        struct pcx_connection *conn = user_data;

        if (!pcx_ascii_string_case_equal(field_name, "sec-websocket-key"))
                return true;

        if (conn->sha1_ctx != NULL) {
                pcx_log("Client at %s sent a WebSocket header with multiple "
                        "Sec-WebSocket-Key headers",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        conn->sha1_ctx = pcx_alloc(sizeof *conn->sha1_ctx);
        SHA1Init(conn->sha1_ctx);
        SHA1Update(conn->sha1_ctx, (const uint8_t *) value, strlen(value));

        return true;
}

static bool
ws_headers_finished(struct pcx_connection *conn)
{
        uint8_t sha1_hash[SHA1_DIGEST_LENGTH];
        size_t encoded_size;

        if (conn->sha1_ctx == NULL) {
                pcx_log("Client at %s sent a WebSocket header without a "
                        "Sec-WebSocket-Key header",
                        conn->remote_address_string);
                set_error_state(conn);
                return false;
        }

        SHA1Update(conn->sha1_ctx,
                   (const uint8_t *) ws_sec_key_guid,
                   sizeof ws_sec_key_guid - 1);
        SHA1Final(sha1_hash, conn->sha1_ctx);
        pcx_free(conn->sha1_ctx);
        conn->sha1_ctx = NULL;

        /* Send the WebSocket protocol response. This is the first
         * thing we'll send to the client so there should always be
         * enough space in the write buffer.
         */

        {
                _Static_assert(PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH) +
                               sizeof ws_header_prefix - 1 +
                               sizeof ws_header_postfix - 1 <=
                               sizeof conn->write_buf,
                               "The write buffer is too small to contain the "
                               "WebSocket protocol reply");
        }

        memcpy(conn->write_buf,
               ws_header_prefix,
               sizeof ws_header_prefix - 1);
        encoded_size = pcx_base64_encode(sha1_hash,
                                         sizeof sha1_hash,
                                         (char *) conn->write_buf +
                                         sizeof ws_header_prefix - 1);
        assert(encoded_size == PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH));
        memcpy(conn->write_buf + sizeof ws_header_prefix - 1 + encoded_size,
               ws_header_postfix,
               sizeof ws_header_postfix - 1);

        conn->write_buf_pos = (PCX_BASE64_ENCODED_SIZE(SHA1_DIGEST_LENGTH) +
                               sizeof ws_header_prefix - 1 +
                               sizeof ws_header_postfix - 1);

        update_poll_flags(conn);

        return true;
}

static const struct pcx_ws_parser_vtable
ws_parser_vtable = {
        .request_line_received = ws_request_line_received_cb,
        .header_received = ws_header_received_cb
};

static void
handle_ws_data(struct pcx_connection *conn,
               size_t got)
{
        struct pcx_error *error = NULL;
        enum pcx_ws_parser_result result;
        size_t consumed;

        result = pcx_ws_parser_parse_data(conn->ws_parser,
                                          conn->read_buf,
                                          got,
                                          &consumed,
                                          &error);

        switch (result) {
        case PCX_WS_PARSER_RESULT_NEED_MORE_DATA:
                break;
        case PCX_WS_PARSER_RESULT_FINISHED:
                pcx_ws_parser_free(conn->ws_parser);
                conn->ws_parser = NULL;
                memmove(conn->read_buf,
                        conn->read_buf + consumed,
                        got - consumed);
                conn->read_buf_pos = got - consumed;

                if (ws_headers_finished(conn))
                        process_frames(conn);
                break;
        case PCX_WS_PARSER_RESULT_ERROR:
                if (error->domain != &pcx_ws_parser_error ||
                    error->code != PCX_WS_PARSER_ERROR_CANCELLED) {
                        pcx_log("WebSocket protocol error from %s: %s",
                                conn->remote_address_string,
                                error->message);
                        set_error_state(conn);
                }
                pcx_error_free(error);
                break;
        }
}

static void
handle_read_error(struct pcx_connection *conn,
                  size_t got)
{
        if (got == 0) {
                pcx_log("Connection closed for %s",
                        conn->remote_address_string);
                set_error_state(conn);
        } else {
                enum pcx_file_error e = pcx_file_error_from_errno(errno);

                if (e != PCX_FILE_ERROR_AGAIN &&
                    e != PCX_FILE_ERROR_INTR) {
                        pcx_log("Error reading from socket for %s: %s",
                                conn->remote_address_string,
                                strerror(errno));
                        set_error_state(conn);
                }
        }
}

static void
handle_read(struct pcx_connection *conn)
{
        ssize_t got;

        got = read(conn->sock,
                   conn->read_buf + conn->read_buf_pos,
                   sizeof conn->read_buf - conn->read_buf_pos);

        if (got <= 0) {
                handle_read_error(conn, got);
        } else {
                set_last_update_time(conn);

                if (conn->ws_parser) {
                        handle_ws_data(conn, got);
                } else {
                        conn->read_buf_pos += got;

                        process_frames(conn);
                }
        }
}

static void
handle_write(struct pcx_connection *conn)
{
        int wrote;

        fill_write_buf(conn);

        wrote = write(conn->sock,
                      conn->write_buf,
                      conn->write_buf_pos);

        if (wrote == -1) {
                enum pcx_file_error e = pcx_file_error_from_errno(errno);

                if (e != PCX_FILE_ERROR_AGAIN &&
                    e != PCX_FILE_ERROR_INTR) {
                        pcx_log("Error writing to socket for %s: %s",
                                conn->remote_address_string,
                                strerror(errno));
                        set_error_state(conn);
                }
        } else {
                memmove(conn->write_buf,
                        conn->write_buf + wrote,
                        conn->write_buf_pos - wrote);
                conn->write_buf_pos -= wrote;

                update_poll_flags(conn);
        }
}

static void
connection_poll_cb(struct pcx_main_context_source *source,
                   int fd,
                   enum pcx_main_context_poll_flags flags,
                   void *user_data)
{
        struct pcx_connection *conn = user_data;

        if (flags & PCX_MAIN_CONTEXT_POLL_ERROR)
                handle_error(conn);
        else if (flags & PCX_MAIN_CONTEXT_POLL_IN)
                handle_read(conn);
        else if (flags & PCX_MAIN_CONTEXT_POLL_OUT)
                handle_write(conn);
}

void
pcx_connection_free(struct pcx_connection *conn)
{
        remove_sources(conn);

        pcx_free(conn->remote_address_string);
        pcx_close(conn->sock);

        if (conn->player) {
                pcx_list_remove(&conn->conversation_listener.link);
                conn->player->ref_count--;
        }

        if (conn->ws_parser)
                pcx_free(conn->ws_parser);

        if (conn->sha1_ctx)
                pcx_free(conn->sha1_ctx);

        pcx_free(conn);
}

static struct pcx_connection *
new_for_socket(int sock,
               const struct pcx_netaddress *remote_address)
{
        struct pcx_connection *conn;

        conn = pcx_calloc(sizeof *conn);

        conn->sock = sock;
        conn->remote_address = *remote_address;
        conn->remote_address_string = pcx_netaddress_to_string(remote_address);
        conn->ws_parser = pcx_ws_parser_new(&ws_parser_vtable, conn);

        pcx_signal_init(&conn->event_signal);

        conn->socket_source =
                pcx_main_context_add_poll(NULL, /* context */
                                          sock,
                                          PCX_MAIN_CONTEXT_POLL_IN,
                                          connection_poll_cb,
                                          conn);

        set_last_update_time(conn);

        return conn;
}

struct pcx_signal *
pcx_connection_get_event_signal(struct pcx_connection *conn)
{
        return &conn->event_signal;
}

const char *
pcx_connection_get_remote_address_string(struct pcx_connection *conn)
{
        return conn->remote_address_string;
}

const struct pcx_netaddress *
pcx_connection_get_remote_address(struct pcx_connection *conn)
{
        return &conn->remote_address;
}

struct pcx_connection *
pcx_connection_accept(int server_sock,
                      struct pcx_error **error)
{
        struct pcx_netaddress address;
        struct pcx_netaddress_native native_address;
        int sock;

        native_address.length = sizeof native_address.sockaddr_in6;

        sock = accept(server_sock,
                      &native_address.sockaddr,
                      &native_address.length);

        if (sock == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Error accepting connection: %s",
                                   strerror(errno));
                return NULL;
        }

        if (!pcx_socket_set_nonblock(sock, error)) {
                pcx_close(sock);
                return NULL;
        }

        pcx_netaddress_from_native(&address, &native_address);

        return new_for_socket(sock, &address);
}

uint64_t
pcx_connection_get_last_update_time(struct pcx_connection *conn)
{
        return conn->last_update_time;
}

static bool
conversation_event_cb(struct pcx_listener *listener,
                      void *data)
{
        struct pcx_connection *connection =
               pcx_container_of(listener,
                                struct pcx_connection,
                                conversation_listener);
        const struct pcx_conversation_event *event = data;

        switch (event->type) {
        case PCX_CONVERSATION_EVENT_STARTED:
        case PCX_CONVERSATION_EVENT_PLAYER_ADDED:
        case PCX_CONVERSATION_EVENT_PLAYER_REMOVED:
                break;
        case PCX_CONVERSATION_EVENT_NEW_MESSAGE:
                update_poll_flags(connection);
                break;
        }

        return true;
}

void
pcx_connection_set_player(struct pcx_connection *conn,
                          struct pcx_player *player,
                          int n_messages_received)
{
        assert(conn->player == NULL);
        assert(player != NULL);

        player->ref_count++;

        conn->player = player;
        pcx_signal_add(&player->conversation->event_signal,
                       &conn->conversation_listener);
        conn->conversation_listener.notify = conversation_event_cb;

        conn->sent_conversation_details = false;

        conn->last_message_sent = &player->conversation->messages;

        while (n_messages_received > 0 &&
               conn->last_message_sent->next !=
               &player->conversation->messages) {
                conn->last_message_sent = conn->last_message_sent->next;

                struct pcx_conversation_message *message =
                        pcx_container_of(conn->last_message_sent,
                                         struct pcx_conversation_message,
                                         link);

                if (message->target_player == -1 ||
                    message->target_player == player->player_num)
                        n_messages_received--;
        }

        /* This is to update the time on the player */
        set_last_update_time(conn);

        update_poll_flags(conn);
}

struct pcx_player *
pcx_connection_get_player(struct pcx_connection *conn)
{
        return conn->player;
}
