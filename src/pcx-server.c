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

#include "pcx-server.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-config.h"
#include "pcx-buffer.h"
#include "pcx-log.h"
#include "pcx-json.h"

struct pcx_error_domain
pcx_server_error;

struct pcx_server_response {
        struct pcx_list link;
        struct json_object *object;
};

struct pcx_server_event {
        struct pcx_list link;

        /* -1 if the event is for everyone */
        int target_person;

        struct json_object *object;
};

struct pcx_server_person {
        int64_t id;

        char *name;

        /* Pointer back to the owning game */
        struct pcx_server_game *game;
        /* List of connections watching this person */
        struct pcx_list watching_connections;
};

struct pcx_server_game {
        struct pcx_list link;

        char *room_name;
        const struct pcx_game *game_type;
        enum pcx_text_language language;

        void *game;

        bool is_finished;

        int n_people;
        struct pcx_server_person people[PCX_GAME_MAX_PLAYERS];

        struct pcx_list events;
};

struct pcx_server_connection {
        struct pcx_list link;

        struct pcx_server *server;

        int sock;
        struct pcx_main_context_source *source;

        /* This becomes true when we've received something from the
         * client that we don't understand and we're ignoring any
         * further input
         */
        bool had_bad_input;
        /* This becomes true when the client has closed its end of the
         * connection
         */
        bool read_finished;
        /* This becomes true when we've stopped writing data. This
         * will only happen after the client closes its connection or
         * we've had bad input and we're ignoring further data
         */
        bool write_finished;
        /* Set to true if we’ve sent some data to the tokener without
         * receiving a complete object. In that case it is invalid for
         * the client to close their writing end.
         */
        bool partial_object;

        /* Queue of pcx_server_response to write to the connection */
        struct pcx_list response_queue;
        /* How much data is in write_buf */
        size_t output_length;
        /* If we try to fill the buffer with a JSON object but there
         * isn’t enough space, then this will hold a reference to the
         * object so we can write more of it.
         */
        struct json_object *partially_written_object;
        size_t partially_written_amount;
        /* Updating the poll flags is done on idle because it can
         * cause the connection to be closed.
         */
        struct pcx_main_context_source *update_poll_source;

        uint8_t write_buf[1024];

        struct json_tokener *tokener;

        /* When this is not NULL, there will be an entry for this
         * connection in the person’s connection list. This should end
         * up keeping the person alive.
         */
        struct pcx_server_person *watching_person;
        struct pcx_list person_link;
};

struct pcx_server {
        const struct pcx_config *config;

        bool abstract_address;
        char *address;

        struct pcx_main_context_source *server_source;

        int sock;
        bool socket_bound;

        struct pcx_list connections;

        /* Temporary buffer for reading data to feed into the tokener */
        char read_buf[1024];

        /* Each pcx_server_game is owned by this list */
        struct pcx_list games;
};

static bool
set_nonblock(int sock,
             struct pcx_error **error)
{
        int flags;

        flags = fcntl(sock, F_GETFL, 0);

        if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error setting non-blocking mode: %s",
                              strerror(errno));
                return false;
        }

        return true;
}

static void
remove_response(struct pcx_server_response *response)
{
        json_object_put(response->object);
        pcx_list_remove(&response->link);
        pcx_free(response);
}

static void
remove_responses(struct pcx_server_connection *connection)
{
        struct pcx_server_response *response, *tmp;

        pcx_list_for_each_safe(response, tmp,
                               &connection->response_queue,
                               link) {
                remove_response(response);
        }
}

static void
destroy_person(struct pcx_server_person *person)
{
        assert(pcx_list_empty(&person->watching_connections));
        pcx_free(person->name);
}

static void
remove_events(struct pcx_server_game *game)
{
        struct pcx_server_event *event, *tmp;

        pcx_list_for_each_safe(event, tmp, &game->events, link) {
                json_object_put(event->object);
                pcx_free(event);
        }
}

static void
free_game_game(struct pcx_server_game *game)
{
        if (game->game == NULL)
                return;

        game->game_type->free_game_cb(game->game);
        game->game = NULL;
}

static void
remove_game(struct pcx_server_game *game)
{
        for (unsigned i = 0; i < game->n_people; i++)
                destroy_person(game->people + i);

        pcx_free(game->room_name);

        free_game_game(game);

        remove_events(game);

        pcx_list_remove(&game->link);

        pcx_free(game);
}

static void
unwatch_person(struct pcx_server_connection *connection)
{
        if (connection->watching_person) {
                pcx_list_remove(&connection->person_link);
                connection->watching_person = NULL;
        }
}

static void
remove_connection(struct pcx_server_connection *connection)
{
        unwatch_person(connection);

        remove_responses(connection);

        if (connection->partially_written_object)
                json_object_put(connection->partially_written_object);

        if (connection->tokener)
                json_tokener_free(connection->tokener);
        if (connection->source)
                pcx_main_context_remove_source(connection->source);
        if (connection->sock != -1)
                close(connection->sock);

        if (connection->update_poll_source)
                pcx_main_context_remove_source(connection->update_poll_source);

        pcx_list_remove(&connection->link);

        pcx_free(connection);
}

static void
update_poll(struct pcx_server_connection *connection)
{
        enum pcx_main_context_poll_flags flags = 0;

        if (!connection->read_finished)
                flags |= PCX_MAIN_CONTEXT_POLL_IN;

        /* Shutdown the socket if we've finished writing */
        if (!connection->write_finished &&
            (connection->read_finished || connection->had_bad_input) &&
            pcx_list_empty(&connection->response_queue) &&
            connection->output_length == 0 &&
            connection->partially_written_object == NULL &&
            connection->watching_person == NULL) {
                if (shutdown(connection->sock, SHUT_WR) == -1) {
                        remove_connection(connection);
                        return;
                }

                connection->write_finished = true;
        }

        /* If both ends of the connection are closed then we can
         * abandon this connection
         */
        if (connection->read_finished && connection->write_finished) {
                remove_connection(connection);
                return;
        }

        if (!connection->write_finished) {
                if (connection->output_length > 0 ||
                    !pcx_list_empty(&connection->response_queue) ||
                    connection->partially_written_object != NULL)
                        flags |= PCX_MAIN_CONTEXT_POLL_OUT;
        }

        pcx_main_context_modify_poll(connection->source, flags);
}

static void
queue_update_poll_cb(struct pcx_main_context_source *source,
                     void *user_data)
{
        struct pcx_server_connection *connection = user_data;

        assert(source == connection->update_poll_source);

        connection->update_poll_source = NULL;

        update_poll(connection);
}

static void
queue_update_poll(struct pcx_server_connection *connection)
{
        if (connection->update_poll_source)
                return;

        connection->update_poll_source =
                pcx_main_context_add_timeout(NULL,
                                             0, /* ms */
                                             queue_update_poll_cb,
                                             connection);
}

static void
queue_response(struct pcx_server_connection *connection,
               struct json_object *obj)
{
        struct pcx_server_response *response = pcx_alloc(sizeof *response);

        response->object = json_object_get(obj);

        pcx_list_insert(connection->response_queue.prev, &response->link);

        queue_update_poll(connection);
}

static void
queue_result_response(struct pcx_server_connection *connection,
                      const char *type,
                      const char *message)
{
        json_object *obj = json_object_new_object();

        if (message) {
                json_object *str = json_object_new_string(message);
                json_object_object_add(obj, "description", str);
        }

        json_object *str = json_object_new_string(type);
        json_object_object_add(obj, "result", str);

        queue_response(connection, obj);

        json_object_put(obj);
}

static void
queue_error_response(struct pcx_server_connection *connection,
                     const char *message)
{
        queue_result_response(connection, "error", message);
}

static void
queue_error_response_text(struct pcx_server_connection *connection,
                          enum pcx_text_language language,
                          enum pcx_text_string string)
{
        const char *s = pcx_text_get(language, string);
        queue_result_response(connection, "error", s);
}

static void
queue_error_response_textf(struct pcx_server_connection *connection,
                           enum pcx_text_language language,
                           enum pcx_text_string string,
                           ...)
{
        const char *s = pcx_text_get(language, string);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        va_list ap;

        va_start(ap, string);
        pcx_buffer_append_vprintf(&buf, s, ap);
        va_end(ap);

        queue_result_response(connection,
                              "error",
                              (const char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static void
queue_ok_response(struct pcx_server_connection *connection,
                  const char *message)
{
        queue_result_response(connection, "ok", message);
}

static void
set_bad_input(struct pcx_server_connection *connection)
{
        /* Remove all other responses */
        remove_responses(connection);
        unwatch_person(connection);

        queue_error_response(connection, "Invalid request");

        connection->had_bad_input = true;
}

static struct pcx_server_game *
add_game(struct pcx_server *server,
         const struct pcx_game *game_type,
         const char *room_name,
         enum pcx_text_language language)
{
        struct pcx_server_game *game;

        /* Try to find an existing game with matching details */
        pcx_list_for_each(game, &server->games, link) {
                if (game->game_type == game_type &&
                    game->language == language &&
                    !strcmp(game->room_name, room_name) &&
                    game->game == NULL &&
                    !game->is_finished &&
                    game->n_people < game_type->max_players)
                        return game;
        }

        /* Otherwise start a new game */
        game = pcx_calloc(sizeof *game);

        game->room_name = pcx_strdup(room_name);
        game->game_type = game_type;
        game->language = language;
        pcx_list_init(&game->events);

        pcx_list_insert(&server->games, &game->link);

        return game;
}

static int64_t
generate_id(void)
{
        /* TODO: this should be less predictable */
        int64_t id = 0;

        for (unsigned i = 0; i < (sizeof id) / sizeof (int16_t); i++) {
                int16_t part = rand();
                id = (id << ((sizeof part) * 8)) | part;
        }

        return id;
}

static struct pcx_server_person *
find_person(struct pcx_server *server,
            int64_t id)
{
        /* FIXME: this should really be a hash table */

        struct pcx_server_game *game;

        pcx_list_for_each(game, &server->games, link) {
                for (int i = 0; i < game->n_people; i++) {
                        struct pcx_server_person *person = game->people + i;

                        if (person->id == id)
                                return person;
                }
        }

        return NULL;
}

static struct pcx_server_person *
add_person(struct pcx_server *server,
           const char *player_name,
           struct pcx_server_game *game)
{
        struct pcx_server_person *person =
                game->people + game->n_people;

        do {
                person->id = generate_id();
        } while (find_person(server, person->id));

        person->name = pcx_strdup(player_name);

        person->game = game;

        pcx_list_init(&person->watching_connections);

        game->n_people++;

        return person;
}

static struct pcx_server_person *
get_person_for_request(struct pcx_server_connection *connection,
                       struct json_object *object)
{
        int64_t person_id;

        if (!pcx_json_get(object,
                          "id", json_type_int, &person_id,
                          NULL)) {
                queue_error_response(connection,
                                     "Missing person ID in request");
                return NULL;
        }

        struct pcx_server_person *person =
                find_person(connection->server, person_id);

        if (person == NULL) {
                queue_error_response(connection,
                                     "Unknown person ID in request");
                return NULL;
        }

        return person;
}

static void
broadcast_event_to_person(struct pcx_server_person *person,
                          struct pcx_server_event *event)
{
        struct pcx_server_connection *connection;

        pcx_list_for_each(connection,
                          &person->watching_connections,
                          person_link) {
                queue_response(connection, event->object);
        }
}

static void
broadcast_event(struct pcx_server_game *game,
                struct pcx_server_event *event)
{
        if (event->target_person == -1) {
                for (int i = 0; i < game->n_people; i++)
                        broadcast_event_to_person(game->people + i, event);
        } else {
                broadcast_event_to_person(game->people + event->target_person,
                                          event);
        }
}

static void
queue_message(struct pcx_server_game *game,
              int user_num,
              enum pcx_game_message_format format,
              const char *message,
              size_t n_buttons,
              const struct pcx_game_button *buttons)
{
        struct json_object *obj = json_object_new_object();

        if (user_num >= 0) {
                struct json_object *t = json_object_new_boolean(true);
                json_object_object_add(obj, "private", t);
        }

        struct json_object *type;

        switch (format) {
        case PCX_GAME_MESSAGE_FORMAT_HTML:
                type = json_object_new_string("html");
                goto found_type;
        case PCX_GAME_MESSAGE_FORMAT_PLAIN:
                type = json_object_new_string("plain");
                goto found_type;
        }

        assert(!"unknown message format type");

found_type:
        json_object_object_add(obj, "type", type);

        struct json_object *s = json_object_new_string(message);
        json_object_object_add(obj, "message", s);

        struct pcx_server_event *event = pcx_calloc(sizeof *event);

        event->target_person = user_num;
        event->object = obj;

        pcx_list_insert(game->events.prev, &event->link);

        broadcast_event(game, event);
}

static void
send_private_message_cb(int user_num,
                        enum pcx_game_message_format format,
                        const char *message,
                        size_t n_buttons,
                        const struct pcx_game_button *buttons,
                        void *user_data)
{
        struct pcx_server_game *game = user_data;

        assert(user_num >= 0 && user_num < game->n_people);

        queue_message(game,
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
        struct pcx_server_game *game = user_data;

        queue_message(game,
                      -1, /* target */
                      format,
                      message,
                      n_buttons,
                      buttons);
}

static void
game_over_cb(void *user_data)
{
        struct pcx_server_game *game = user_data;

        pcx_log("game finished successfully");

        game->is_finished = true;
        free_game_game(game);
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_private_message = send_private_message_cb,
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
start_watch(struct pcx_server_connection *connection,
            struct pcx_server_person *person,
            int64_t first_event)
{
        unwatch_person(connection);

        connection->watching_person = person;
        pcx_list_insert(&person->watching_connections,
                        &connection->person_link);

        struct pcx_list *link = &person->game->events;

        while (link->next != &person->game->events) {
                const struct pcx_server_event *event =
                        pcx_container_of(link->next,
                                         struct pcx_server_event,
                                         link);

                if (event->target_person == -1 ||
                    person->game->people + event->target_person == person) {
                        if (first_event > 0)
                                first_event--;
                        else
                                queue_response(connection, event->object);
                }

                link = link->next;
        }
}

static void
handle_join(struct pcx_server_connection *connection,
            struct json_object *object)
{
        const char *player_name, *game_name, *room_name, *language_code;

        if (!pcx_json_get(object,
                          "name", json_type_string, &player_name,
                          "game", json_type_string, &game_name,
                          "room_name", json_type_string, &room_name,
                          "language", json_type_string, &language_code,
                          NULL)) {
                queue_error_response(connection, "Invalid join request");
                return;
        }

        const struct pcx_game *game_type;

        for (unsigned i = 0; pcx_game_list[i]; i++) {
                if (!strcmp(pcx_game_list[i]->name, game_name)) {
                        game_type = pcx_game_list[i];
                        goto found_game_type;
                }
        }

        queue_error_response(connection, "Unknown game type");
        return;

found_game_type: (void) 0;

        enum pcx_text_language language;

        if (!pcx_text_lookup_language(language_code, &language)) {
                queue_error_response(connection, "Unknown language code");
                return;
        }

        struct pcx_server_game *game = add_game(connection->server,
                                                game_type,
                                                room_name,
                                                language);
        struct pcx_server_person *person = add_person(connection->server,
                                                      player_name,
                                                      game);

        struct json_object *obj = json_object_new_object();

        struct json_object *id = json_object_new_int64(person->id);
        json_object_object_add(obj, "person_id", id);

        queue_response(connection, obj);

        json_object_put(obj);

        start_watch(connection,
                    person,
                    0 /* first_event */);
}

static void
handle_watch(struct pcx_server_connection *connection,
             struct json_object *object)
{
        struct pcx_server_person *person =
                get_person_for_request(connection,
                                       object);

        if (person == NULL)
                return;

        int64_t first_event;

        if (!pcx_json_get(object,
                          "first_event", json_type_int, &first_event,
                          NULL)) {
                queue_error_response(connection, "Invalid watch request");
                return;
        }

        start_watch(connection,
                    person,
                    first_event);
}

static void
handle_start(struct pcx_server_connection *connection,
             struct json_object *object)
{
        struct pcx_server_person *person =
                get_person_for_request(connection,
                                       object);

        if (person == NULL)
                return;

        struct pcx_server_game *game = person->game;

        if (game->is_finished || game->game) {
                queue_error_response_text(connection,
                                          game->language,
                                          PCX_TEXT_STRING_GAME_ALREADY_STARTED);
                return;
        }

        if (game->n_people < game->game_type->min_players) {
                queue_error_response_textf(connection,
                                           game->language,
                                           PCX_TEXT_STRING_NEED_MIN_PLAYERS,
                                           game->game_type->min_players);
                return;
        }

        const struct pcx_game *game_type = game->game_type;
        struct pcx_server *server = connection->server;
        const char *names[PCX_GAME_MAX_PLAYERS];

        pcx_log("game started with %i players", game->n_people);

        for (int i = 0; i < game->n_people; i++)
                names[i] = game->people[i].name;

        game->game =
                game_type->create_game_cb(server->config,
                                          &game_callbacks,
                                          game,
                                          game->language,
                                          game->n_people,
                                          names);

        queue_ok_response(connection, NULL);
}

struct request_handler {
        const char *name;
        void (* handler)(struct pcx_server_connection *connection,
                         struct json_object *object);
};

static const struct request_handler
request_handlers[] = {
        { .name = "join", .handler = handle_join },
        { .name = "watch", .handler = handle_watch },
        { .name = "start", .handler = handle_start },
};

static void
handle_request(struct pcx_server_connection *connection,
               struct json_object *object)
{
        const char *name;

        if (!pcx_json_get(object,
                          "request", json_type_string, &name,
                          NULL)) {
                queue_error_response(connection, "Missing “request” parameter");
                return;
        }

        for (unsigned i = 0; i < PCX_N_ELEMENTS(request_handlers); i++) {
                if (!strcmp(name, request_handlers[i].name)) {
                        request_handlers[i].handler(connection,
                                                    object);
                        return;
                }
        }

        queue_error_response(connection, "Unsupported request");
}

static void
parse_json_objects(struct pcx_server_connection *connection,
                   size_t buf_size,
                   const char *buf)
{
        while (true) {
                if (!connection->partial_object) {
                        /* Skip any leading whitespace so that if
                         * there was trailing whitespace after an
                         * object then we don’t think it’s a partial
                         * object.
                         */
                        while (buf_size > 0 && strchr("\r\n\t ", buf[0])) {
                                buf_size--;
                                buf++;
                        }
                }

                if (buf_size <= 0)
                        break;

                struct json_object *object =
                        json_tokener_parse_ex(connection->tokener,
                                              buf,
                                              buf_size);

                if (object == NULL) {
                        enum json_tokener_error error =
                                json_tokener_get_error(connection->tokener);

                        if (error == json_tokener_continue) {
                                connection->partial_object = true;
                        } else {
                                pcx_log("Invalid JSON received on %i",
                                        connection->sock);
                                set_bad_input(connection);
                        }
                        break;
                }

                if (!json_object_is_type(object, json_type_object)) {
                        pcx_log("Received JSON object that isn’t an object "
                                "on %i",
                                connection->sock);
                        set_bad_input(connection);
                        break;
                }

                buf_size -= connection->tokener->char_offset;
                buf += connection->tokener->char_offset;

                handle_request(connection, object);

                json_object_put(object);

                json_tokener_reset(connection->tokener);

                connection->partial_object = false;
        }
}

static void
fill_write_buf(struct pcx_server_connection *connection)
{
        if (connection->partially_written_object) {
                struct json_object *obj = connection->partially_written_object;
                const char *str = json_object_to_json_string(obj);
                size_t len = strlen(str);
                size_t to_write =
                        MIN((sizeof connection->write_buf) -
                            connection->output_length,
                            len - connection->partially_written_amount);

                memcpy(connection->write_buf +
                       connection->output_length,
                       str + connection->partially_written_amount,
                       to_write);

                connection->output_length += to_write;

                if (to_write + connection->partially_written_amount >= len) {
                        json_object_put(obj);
                        connection->partially_written_object = NULL;
                        connection->partially_written_amount = 0;
                } else {
                        connection->partially_written_amount += to_write;
                        return;
                }
        }

        while (connection->output_length < sizeof connection->write_buf &&
               !pcx_list_empty(&connection->response_queue)) {
                struct pcx_server_response *response =
                        pcx_container_of(connection->response_queue.next,
                                         struct pcx_server_response,
                                         link);

                struct json_object *obj = json_object_get(response->object);

                remove_response(response);

                const char *str = json_object_to_json_string(obj);
                size_t len = strlen(str);
                size_t to_write =
                        MIN((sizeof connection->write_buf) -
                            connection->output_length,
                            len);

                memcpy(connection->write_buf +
                       connection->output_length,
                       str,
                       to_write);

                connection->output_length += to_write;

                if (to_write >= len) {
                        json_object_put(obj);
                } else {
                        connection->partially_written_amount = to_write;
                        connection->partially_written_object = obj;
                        break;
                }
        }
}

static void
connection_cb(struct pcx_main_context_source *source,
              int fd,
              enum pcx_main_context_poll_flags flags,
              void *user_data)
{
        struct pcx_server_connection *connection = user_data;
        struct pcx_server *server = connection->server;

        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR)) {
                int value;
                unsigned int value_len = sizeof (value);

                if (getsockopt(connection->sock,
                               SOL_SOCKET,
                               SO_ERROR,
                               &value,
                               &value_len) == -1 ||
                    value_len != sizeof (value) ||
                    value == 0) {
                        pcx_log("Unknown error on socket for %i",
                                connection->sock);
                } else {
                        pcx_log("Error on socket for %i: %s",
                                connection->sock,
                                strerror(value));
                }

                remove_connection (connection);
                return;
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_IN)) {
                int got = read(fd,
                               server->read_buf,
                               sizeof server->read_buf);

                if (got == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                pcx_log("Error on %i: %s",
                                        connection->sock,
                                        strerror(errno));
                                remove_connection(connection);
                                return;
                        }
                } else if (got == 0) {
                        connection->read_finished = true;
                        if (!connection->had_bad_input &&
                            connection->partial_object) {
                                pcx_log("Socket shutdown with partial JSON "
                                        "object on %i",
                                        connection->sock);
                                set_bad_input(connection);
                        }
                        queue_update_poll(connection);
                } else {
                        if (!connection->had_bad_input) {
                                parse_json_objects(connection,
                                                   got,
                                                   server->read_buf);
                        }
                }
        }

        if ((flags & PCX_MAIN_CONTEXT_POLL_OUT)) {
                fill_write_buf(connection);

                int wrote = write(fd,
                                  connection->write_buf,
                                  connection->output_length);

                if (wrote == -1) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                pcx_log("Error on %i: %s",
                                        connection->sock,
                                        strerror(errno));
                                remove_connection(connection);
                                return;
                        }
                } else {
                        memmove(connection->write_buf,
                                connection->write_buf + wrote,
                                connection->output_length - wrote);
                        connection->output_length -= wrote;

                        queue_update_poll(connection);
                }
        }
}

static void
server_socket_cb(struct pcx_main_context_source *source,
                 int fd,
                 enum pcx_main_context_poll_flags flags,
                 void *user_data)
{
        struct pcx_server *server = user_data;

        assert(server->server_source == source);
        assert(server->sock == fd);

        int client_fd = accept(fd, NULL, NULL);

        if (client_fd == -1) {
                switch (errno) {
                case EAGAIN:
#if EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                        break;
                case EMFILE:
                        pcx_log("Accept failed due to too many open fds. "
                                "Waiting for a client to disconnect.");
                        /* Run out of file descriptors. Stop listening
                         * until someone disconnects.
                         */
                        pcx_main_context_modify_poll(source, 0);
                        break;
                default:
                        /* Some other error that is probably going to
                         * mean the poll is broken. Let’s give up
                         * listening on the socket.
                         */
                        pcx_log("Accept failed: %s", strerror(errno));
                        pcx_main_context_remove_source(source);
                        server->server_source = NULL;
                        break;
                }

                return;
        }

        struct pcx_error *error = NULL;

        pcx_log("Accepted connection on %i", client_fd);

        if (!set_nonblock(client_fd, &error)) {
                pcx_log("On client fd: %s", error->message);
                pcx_error_free(error);
                close(client_fd);
                return;
        }

        struct pcx_server_connection *connection =
                pcx_calloc(sizeof *connection);

        connection->server = server;
        connection->sock = client_fd;
        connection->tokener = json_tokener_new();
        pcx_list_init(&connection->response_queue);
        connection->source =
                pcx_main_context_add_poll(NULL,
                                          client_fd,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          connection_cb,
                                          connection);

        pcx_list_insert(&server->connections, &connection->link);
}

static void
delete_existing_socket(const char *address)
{
        struct stat statbuf;

        if (stat(address, &statbuf) == -1)
                return;

        if (!S_ISSOCK(statbuf.st_mode))
                return;

        unlink(address);
}

static void
create_address(struct pcx_server *server,
               size_t *address_size_out,
               struct sockaddr_un **addr_out)
{
        size_t address_size = (offsetof(struct sockaddr_un, sun_path) +
                               strlen(server->address));

        if (server->abstract_address)
                address_size++;

        struct sockaddr_un *addr = pcx_alloc(address_size + 1);
        addr->sun_family = AF_LOCAL;

        if (server->abstract_address) {
                addr->sun_path[0] = '\0';
                strcpy(addr->sun_path + 1, server->address);
        } else {
                strcpy(addr->sun_path, server->address);
        }

        *address_size_out = address_size;
        *addr_out = addr;
}

static bool
create_socket(struct pcx_server *server,
              struct pcx_error **error)
{
        server->sock = socket(AF_LOCAL, SOCK_STREAM, 0);

        if (server->sock == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error creating socket: %s",
                              strerror(errno));
                return false;
        }

        if (!set_nonblock(server->sock, error))
                return false;

        size_t address_size;
        struct sockaddr_un *addr;

        create_address(server, &address_size, &addr);

        int ret;

        ret = bind(server->sock, (struct sockaddr *) addr, address_size);

        pcx_free(addr);

        if (ret == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error binding socket: %s",
                              strerror(errno));
                return false;
        }

        server->socket_bound = true;

        ret = listen(server->sock, 10);

        if (ret == -1) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_IO,
                              "Error making socket listen: %s",
                              strerror(errno));
                return false;
        }

        return true;
}

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               const struct pcx_config_server *server_config,
               struct pcx_error **error)
{
        struct pcx_server *server = pcx_calloc(sizeof *server);

        server->config = config;
        server->sock = -1;
        server->address = pcx_strdup(server_config->address);
        server->abstract_address = server_config->abstract;
        pcx_list_init(&server->connections);
        pcx_list_init(&server->games);

        if (!server_config->abstract)
                delete_existing_socket(server_config->address);

        if (!create_socket(server, error))
                goto error;

        server->server_source =
                pcx_main_context_add_poll(NULL,
                                          server->sock,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          server_socket_cb,
                                          server);

        return server;

error:
        pcx_server_free(server);
        return NULL;
}

static void
remove_connections(struct pcx_server *server)
{
        struct pcx_server_connection *connection, *tmp;

        pcx_list_for_each_safe(connection, tmp, &server->connections, link) {
                remove_connection(connection);
        }
}

static void
remove_games(struct pcx_server *server)
{
        struct pcx_server_game *game, *tmp;

        pcx_list_for_each_safe(game, tmp, &server->games, link) {
                remove_game(game);
        }
}

void
pcx_server_free(struct pcx_server *server)
{
        remove_connections(server);
        remove_games(server);

        if (server->server_source)
                pcx_main_context_remove_source(server->server_source);

        if (server->sock != -1)
                close(server->sock);

        if (server->socket_bound &&
            !server->abstract_address) {
                /* Try to clean up the local socket file. It doesn’t
                 * really matter if this fails.
                 */
                unlink(server->address);
        }

        pcx_free(server->address);

        pcx_free(server);
}
