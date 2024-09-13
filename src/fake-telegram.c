/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2024  Neil Roberts
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <json_object.h>
#include <json_tokener.h>
#include <linkhash.h>
#include <stdint.h>
#include <assert.h>
#include <inttypes.h>

#include "pcx-main-context.h"
#include "pcx-util.h"
#include "pcx-listen-socket.h"
#include "pcx-buffer.h"
#include "pcx-list.h"
#include "pcx-file-error.h"
#include "pcx-socket.h"

#define LISTEN_PORT 5061

struct server {
        bool quit;
        int ret;
        int listen_socket;
        int64_t next_update_id;
        int64_t next_message_id;
        int64_t next_query_id;
        int64_t current_chat_id;
        int64_t current_user_id;
        bool private_message;
        struct pcx_main_context_source *stdin_source;
        struct pcx_main_context_source *listen_socket_source;
        struct pcx_buffer stdin_buffer;
        struct pcx_list connections;
        struct pcx_list updates;
        struct pcx_list chat_names;
        struct pcx_list user_names;
        struct json_object *last_message;
};

enum connection_state {
        CONNECTION_STATE_READING_LINES,
        CONNECTION_STATE_READING_PAYLOAD,
        CONNECTION_STATE_AWAITING_UPDATES,
        CONNECTION_STATE_WRITING_RESPONSE,
};

enum command {
        COMMAND_UNKNOWN,
        COMMAND_GET_UPDATES,
        COMMAND_OTHER,
};

struct connection {
        struct server *server;
        int sock;
        enum connection_state state;
        enum command command;
        char *command_name;
        struct pcx_netaddress remote_address;
        char *remote_address_string;
        struct pcx_main_context_source *source;
        struct pcx_list link;
        struct pcx_buffer line_buffer;
        struct json_tokener *tokener;
        struct json_object *obj;
        char *response;
        size_t response_length;
        size_t response_wrote;
};

struct update {
        struct pcx_list link;
        int64_t id;
        struct json_object *obj;
};

struct name {
        int64_t id;
        char *value;
        struct pcx_list link;
};

static const struct {
        const char *name;
        enum command command;
} commands[] = {
        { .name = "getUpdates", .command = COMMAND_GET_UPDATES, },
};

static void
free_connection(struct connection *connection)
{
        pcx_main_context_remove_source(connection->source);
        pcx_close(connection->sock);
        pcx_free(connection->remote_address_string);
        pcx_buffer_destroy(&connection->line_buffer);
        if (connection->obj)
                json_object_put(connection->obj);
        if (connection->tokener)
                json_tokener_free(connection->tokener);
        pcx_free(connection->command_name);
        pcx_free(connection->response);
        pcx_list_remove(&connection->link);
        pcx_free(connection);
}

static void
free_update(struct update *update)
{
        json_object_put(update->obj);
        pcx_list_remove(&update->link);
        pcx_free(update);
}

static void
consume_updates(struct server *server, int64_t min_id)
{
        struct update *update, *tmp;

        pcx_list_for_each_safe(update, tmp, &server->updates, link) {
                if (update->id < min_id)
                        free_update(update);
        }
}

static bool
handle_connection_line(struct connection *connection,
                       const char *line,
                       size_t line_length)
{
        /* We only care about the POST lines */
        if (line_length < 5 || memcmp(line, "POST ", 5))
                return true;

        const char *line_end = line + line_length;
        const char *url_start = line + 5;
        const char *url_end = memchr(url_start, ' ', line_end - url_start);

        if (url_end == NULL)
                url_end = line_end;

        const char *command_start;

        for (const char *p = url_end; p > url_start; p--) {
                if (p[-1] == '/') {
                        command_start = p;
                        goto found_command_name;
                }
        }

        fprintf(stderr,
                "no command found in URL from %s",
                connection->remote_address_string);
        free_connection(connection);
        return false;

found_command_name:
        pcx_free(connection->command_name);
        connection->command_name = pcx_strndup(command_start,
                                            url_end - command_start);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(commands); i++) {
                if (url_end - command_start == strlen(commands[i].name) &&
                    !memcmp(commands[i].name,
                            command_start,
                            url_end - command_start)) {
                        connection->command = commands[i].command;
                        return true;
                }
        }

        connection->command = COMMAND_OTHER;

        return true;
}

static void
set_response(struct connection *connection,
             struct json_object *obj)
{
        const char *str = json_object_to_json_string(obj);
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "HTTP/1.1 200 OK\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Content-Type: application/json\r\n"
                                 "\r\n"
                                 "%s",
                                 strlen(str),
                                 str);

        pcx_main_context_modify_poll(connection->source,
                                     PCX_MAIN_CONTEXT_POLL_OUT |
                                     PCX_MAIN_CONTEXT_POLL_ERROR);

        connection->state = CONNECTION_STATE_WRITING_RESPONSE;

        assert(connection->response == NULL);
        connection->response = (char *) buf.data;
        connection->response_length = buf.length;
}

static void
set_ok_response(struct connection *connection)
{
        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "ok", json_object_new_boolean(true));
        set_response(connection, obj);
        json_object_put(obj);
}

static bool
get_object_int(struct json_object *obj,
               const char *field,
               int64_t *result)
{
        if (!json_object_is_type(obj, json_type_object))
                return false;

        struct json_object *value;

        if (!json_object_object_get_ex(obj, field, &value))
                return false;

        if (!json_object_is_type(value, json_type_int))
                return false;

        *result = json_object_get_int64(value);

        return true;
}

static void
set_updates_response(struct connection *connection)
{
        struct json_object *obj = json_object_new_object();

        json_object_object_add(obj, "ok", json_object_new_boolean(true));

        struct json_object *result = json_object_new_array();

        struct update *update;

        pcx_list_for_each(update, &connection->server->updates, link) {
                json_object_array_add(result, json_object_get(update->obj));
        }

        json_object_object_add(obj, "result", result);

        set_response(connection, obj);

        json_object_put(obj);
}

static void
handle_get_updates(struct connection *connection)
{
        int64_t min_id;

        if (get_object_int(connection->obj, "offset", &min_id))
                consume_updates(connection->server, min_id);

        if (pcx_list_empty(&connection->server->updates)) {
                pcx_main_context_modify_poll(connection->source,
                                             PCX_MAIN_CONTEXT_POLL_ERROR);
                connection->state = CONNECTION_STATE_AWAITING_UPDATES;
        } else {
                set_updates_response(connection);
        }
}

static int
sort_object_key_cb(const void *ap, const void *bp)
{
        const char *a = *(const char **) ap;
        const char *b = *(const char **) bp;

        return strcmp(a, b);
}

static char **
collect_object_keys(struct json_object *obj)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        json_object_object_foreach(obj, key, val) {
                char *key_copy = pcx_strdup(key);
                pcx_buffer_append(&buf, &key_copy, sizeof key_copy);
        }

        /* Sort the keys so that the object will always be displayed
         * in a consistent way
         */
        qsort(buf.data,
              buf.length / sizeof (char *),
              sizeof (char *),
              sort_object_key_cb);

        char *terminator = NULL;

        pcx_buffer_append(&buf, &terminator, sizeof terminator);

        return (char **) buf.data;
}

static void
free_object_keys(char **keys)
{
        for (char **k = keys; *k; k++)
                pcx_free(*k);

        pcx_free(keys);
}

static void
print_indent(int indent_level)
{
        for (int i = 0; i < indent_level; i++)
                fputs("  ", stdout);
}

static bool
is_simple_object(struct json_object *obj)
{
        return !json_object_is_type(obj, json_type_array) &&
                !json_object_is_type(obj, json_type_object);
}

static void
print_json_object(struct json_object *obj, int indent_level)
{
        if (json_object_is_type(obj, json_type_array)) {
                print_indent(indent_level);
                fputc('[', stdout);

                int length = json_object_array_length(obj);

                for (int i = 0; i < length; i++) {
                        struct json_object *child =
                                json_object_array_get_idx(obj, i);
                        if (i > 0)
                                fputc(',', stdout);
                        fputc('\n', stdout);
                        print_json_object(child, indent_level + 1);
                }

                fputc('\n', stdout);
                print_indent(indent_level);
                fputc(']', stdout);
        } else if (json_object_is_type(obj, json_type_object)) {
                print_indent(indent_level);
                fputc('{', stdout);

                char **keys = collect_object_keys(obj);
                bool first_key = true;

                for (char **k = keys; *k; k++) {
                        struct json_object *child;

                        if (json_object_object_get_ex(obj, *k, &child)) {
                                if (first_key)
                                        first_key = false;
                                else
                                        fputc(',', stdout);
                                fputc('\n', stdout);

                                print_indent(indent_level + 1);
                                printf("\"%s\":", *k);

                                if (is_simple_object(child)) {
                                        struct json_object *c = child;
                                        printf(" %s",
                                               json_object_to_json_string(c));
                                } else {
                                        fputc('\n', stdout);
                                        print_json_object(child,
                                                          indent_level + 2);
                                }
                        }
                }

                free_object_keys(keys);

                fputc('\n', stdout);
                print_indent(indent_level);
                printf("}");
        } else {
                print_indent(indent_level);
                printf("%s", json_object_to_json_string(obj));
        }
}

static void
handle_other_command(struct connection *connection)
{
        printf("%s:\n", connection->command_name);

        print_json_object(connection->obj, 1);
        fputc('\n', stdout);

        set_ok_response(connection);
}

static void
handle_request(struct connection *connection)
{
        if (connection->obj == NULL) {
                fprintf(stderr,
                        "request without a JSON object from %s\n",
                        connection->remote_address_string);
                free_connection(connection);
                return;
        }

        switch (connection->command) {
        case COMMAND_GET_UPDATES:
                handle_get_updates(connection);
                break;

        case COMMAND_OTHER:
                handle_other_command(connection);
                break;

        case COMMAND_UNKNOWN:
                assert(!"unknown command should have been handled earlier");
                break;
        }
}

static void
handle_payload(struct connection *connection,
               const char *payload,
               size_t payload_length)
{
        struct json_object *obj =
                json_tokener_parse_ex(connection->tokener,
                                      payload,
                                      payload_length);

        if (obj) {
                if (connection->obj == NULL) {
                        connection->obj = obj;
                        handle_request(connection);
                } else {
                        fprintf(stderr,
                                "got second json object from %s\n",
                                connection->remote_address_string);
                        json_object_put(obj);
                        free_connection(connection);
                }
        } else {
                enum json_tokener_error error =
                        json_tokener_get_error(connection->tokener);

                if (error != json_tokener_continue) {
                        fprintf(stderr,
                                "error parsing JSON from %s: %s\n",
                                connection->remote_address_string,
                                json_tokener_error_desc(error));
                        free_connection(connection);
                }
        }
}

static void
handle_connection_lines(struct connection *connection)
{
        const char *buf_start = (const char *) connection->line_buffer.data;
        const char *buf_end = (const char *) buf_start +
                connection->line_buffer.length;
        const char *end;

        while ((end = memchr(buf_start, '\n', buf_end - buf_start))) {
                size_t length = end - buf_start;

                if (length > 0 && buf_start[length - 1] == '\r')
                        length--;

                if (length == 0) {
                        if (connection->command == COMMAND_UNKNOWN) {
                                fprintf(stderr,
                                        "no command in request from %s\n",
                                        connection->remote_address_string);
                                free_connection(connection);
                        } else {
                                connection->state =
                                        CONNECTION_STATE_READING_PAYLOAD;
                                connection->tokener = json_tokener_new();
                                handle_payload(connection,
                                               end + 1,
                                               buf_end - end - 1);
                        }
                        return;
                }

                if (!handle_connection_line(connection, buf_start, length))
                        return;

                buf_start = end + 1;
        }

        connection->line_buffer.length = buf_end - buf_start;

        memmove(connection->line_buffer.data,
                buf_start,
                connection->line_buffer.length);
}

static void
read_connection_lines(struct connection *connection)
{
        pcx_buffer_ensure_size(&connection->line_buffer,
                               connection->line_buffer.length + 1024);

        int got = read(connection->sock,
                       connection->line_buffer.data +
                       connection->line_buffer.length,
                       connection->line_buffer.size -
                       connection->line_buffer.length);

        if (got == -1) {
                fprintf(stderr,
                        "error reading from %s: %s\n",
                        connection->remote_address_string,
                        strerror(errno));
                free_connection(connection);
        } else if (got == 0) {
                fprintf(stderr,
                        "EOF on socket while reading lines %s\n",
                        connection->remote_address_string);
                free_connection(connection);
        } else {
                connection->line_buffer.length += got;

                handle_connection_lines(connection);
        }
}

static void
read_payload(struct connection *connection)
{
        pcx_buffer_ensure_size(&connection->line_buffer, 1024);

        int got = read(connection->sock,
                       connection->line_buffer.data,
                       connection->line_buffer.size);

        if (got == -1) {
                fprintf(stderr,
                        "error reading from %s: %s\n",
                        connection->remote_address_string,
                        strerror(errno));
                free_connection(connection);
        } else if (got == 0) {
                handle_request(connection);
        } else {
                handle_payload(connection,
                               (const char *) connection->line_buffer.data,
                               got);
        }
}

static void
handle_write_response(struct connection *connection)
{
        assert(connection->state == CONNECTION_STATE_WRITING_RESPONSE);
        assert(connection->response);

        int wrote = write(connection->sock,
                          connection->response +
                          connection->response_wrote,
                          connection->response_length -
                          connection->response_wrote);

        if (wrote < 0) {
                fprintf(stderr,
                        "error writing to %s: %s\n",
                        connection->remote_address_string,
                        strerror(errno));
                free_connection(connection);
        } else {
                connection->response_wrote += wrote;

                if (connection->response_wrote >= connection->response_length)
                        free_connection(connection);
        }
}

static void
connection_poll_cb(struct pcx_main_context_source *source,
                   int fd,
                   enum pcx_main_context_poll_flags flags,
                   void *user_data)
{
        struct connection *connection = user_data;

        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR)) {
                fprintf(stderr,
                        "error on socket for %s\n",
                        connection->remote_address_string);
                free_connection(connection);
        } else if ((flags & PCX_MAIN_CONTEXT_POLL_IN)) {
                switch (connection->state) {
                case CONNECTION_STATE_READING_LINES:
                        read_connection_lines(connection);
                        break;
                case CONNECTION_STATE_READING_PAYLOAD:
                        read_payload(connection);
                        break;
                case CONNECTION_STATE_AWAITING_UPDATES:
                case CONNECTION_STATE_WRITING_RESPONSE:
                        assert(!"shouldn’t get POLL_IN when writing");
                        break;
                }
        } else if ((flags & PCX_MAIN_CONTEXT_POLL_OUT)) {
                handle_write_response(connection);
        }
}

static struct connection *
accept_connection(int server_sock,
                  struct pcx_error **error)
{
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

        struct connection *connection = pcx_calloc(sizeof *connection);

        connection->sock = sock;
        pcx_netaddress_from_native(&connection->remote_address,
                                   &native_address);
        connection->remote_address_string =
                pcx_netaddress_to_string(&connection->remote_address);

        pcx_buffer_init(&connection->line_buffer);

        connection->state = CONNECTION_STATE_READING_LINES;
        connection->command = COMMAND_UNKNOWN;

        connection->source =
                pcx_main_context_add_poll(NULL,
                                          sock,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          connection_poll_cb,
                                          connection);

        return connection;
}

static void
listen_socket_cb(struct pcx_main_context_source *source,
                 int fd,
                 enum pcx_main_context_poll_flags flags,
                 void *user_data)
{
        struct server *server = user_data;

        struct pcx_error *error = NULL;

        struct connection *connection = accept_connection(server->listen_socket,
                                                          &error);

        if (connection == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                server->ret = EXIT_FAILURE;
                server->quit = true;
        } else {
                connection->server = server;
                pcx_list_insert(&server->connections, &connection->link);
        }
}

static void
add_update(struct server *server,
           struct json_object *obj)
{
        struct update *update = pcx_alloc(sizeof *update);

        update->id = server->next_update_id++;

        json_object_object_add(obj,
                               "update_id",
                               json_object_new_int(update->id));

        update->obj = obj;

        pcx_list_insert(server->updates.prev, &update->link);

        struct connection *connection;

        pcx_list_for_each(connection, &server->connections, link) {
                if (connection->state == CONNECTION_STATE_AWAITING_UPDATES)
                        set_updates_response(connection);
        }
}

static const char *
get_name(struct pcx_list *list, int64_t id)
{
        struct name *name;

        pcx_list_for_each(name, list, link) {
                if (name->id == id) {
                        return name->value;
                }
        }

        return NULL;
}

static struct json_object *
get_chat(struct server *server)
{
        struct json_object *chat = json_object_new_object();

        json_object_object_add(chat,
                               "id",
                               json_object_new_int(server->current_chat_id));

        const char *type = server->private_message ? "private" : "group";

        json_object_object_add(chat,
                               "type",
                               json_object_new_string(type));

        const char *name = get_name(&server->chat_names,
                                    server->current_chat_id);

        if (name) {
                json_object_object_add(chat,
                                       "username",
                                       json_object_new_string(name));
        }

        return chat;
}

static struct json_object *
get_from(struct server *server)
{
        struct json_object *from = json_object_new_object();

        json_object_object_add(from,
                               "id",
                               json_object_new_int(server->current_user_id));

        const char *name = get_name(&server->user_names,
                                    server->current_user_id);

        if (name) {
                json_object_object_add(from,
                                       "first_name",
                                       json_object_new_string(name));
        }

        return from;
}

static void
add_entities(struct json_object *message,
             const char *data,
             size_t data_length)
{
        if (data_length < 1 || *data != '/')
                return;

        const char *end = memchr(data, ' ', data_length);

        if (end == NULL)
                end = data + data_length;

        if (end - data < 2)
                return;

        struct json_object *entity = json_object_new_object();

        json_object_object_add(entity,
                               "offset",
                               json_object_new_int(0));
        json_object_object_add(entity,
                               "length",
                               json_object_new_int(end - data));
        json_object_object_add(entity,
                               "type",
                               json_object_new_string("bot_command"));

        struct json_object *entities = json_object_new_array();

        json_object_array_add(entities, entity);

        json_object_object_add(message, "entities", entities);
}

static bool
parse_int(const char *data,
          size_t data_length,
          int64_t *result_out)
{
        const char *data_end = data + data_length;
        bool negative = false;

        if (data < data_end && *data == '-') {
                negative = true;
                data++;
        }

        if (data >= data_end) {
                fprintf(stderr, "missing integer argument in command\n");
                return false;
        }

        int64_t result = 0;

        while (data < data_end) {
                if (*data < '0' || *data > '9') {
                        fprintf(stderr, "invalid integer argument\n");
                        return false;
                }

                result = result * 10 + *data - '0';
                data++;
        }

        if (negative)
                result = -result;

        *result_out = result;

        return true;
}

static void
set_name(struct pcx_list *list,
         int64_t id,
         const char *data,
         size_t data_length)
{
        char *value = pcx_strndup(data, data_length);

        struct name *name;

        pcx_list_for_each(name, list, link) {
                if (name->id == id) {
                        pcx_free(name->value);
                        name->value = value;
                        return;
                }
        }

        name = pcx_alloc(sizeof *name);
        name->id = id;
        name->value = value;
        pcx_list_insert(list, &name->link);
}

static void
handle_set_private(struct server *server,
                   const char *data,
                   size_t data_length)
{
        server->private_message = true;
}

static void
handle_set_public(struct server *server,
                  const char *data,
                  size_t data_length)
{
        server->private_message = false;
}

static void
handle_change_chat(struct server *server,
                      const char *data,
                      size_t data_length)
{
        parse_int(data, data_length, &server->current_chat_id);
}

static void
handle_change_user(struct server *server,
                   const char *data,
                   size_t data_length)
{
        parse_int(data, data_length, &server->current_user_id);
}

static void
handle_set_user_name(struct server *server,
                     const char *data,
                     size_t data_length)
{
        set_name(&server->user_names,
                 server->current_user_id,
                 data,
                 data_length);
}

static void
handle_set_chat_name(struct server *server,
                     const char *data,
                     size_t data_length)
{
        set_name(&server->chat_names,
                 server->current_chat_id,
                 data,
                 data_length);
}

static void
handle_add_message(struct server *server,
                   const char *data,
                   size_t data_length)
{
        struct json_object *message = json_object_new_object();

        json_object_object_add(message,
                               "text",
                               json_object_new_string_len(data, data_length));
        json_object_object_add(message,
                               "message_id",
                               json_object_new_int(server->next_message_id++));

        json_object_object_add(message, "chat", get_chat(server));
        json_object_object_add(message, "from", get_from(server));

        add_entities(message, data, data_length);

        if (server->last_message)
                json_object_put(server->last_message);
        server->last_message = json_object_get(message);

        struct json_object *update = json_object_new_object();

        json_object_object_add(update, "message", message);

        add_update(server, update);
}

static void
handle_add_callback_data(struct server *server,
                         const char *data,
                         size_t data_length)
{
        struct json_object *query = json_object_new_object();

        json_object_object_add(query,
                               "data",
                               json_object_new_string_len(data, data_length));

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&buf, "%" PRIi64, server->next_query_id++);
        json_object_object_add(query,
                               "id",
                               json_object_new_string((const char *) buf.data));
        pcx_buffer_destroy(&buf);

        json_object_object_add(query, "from", get_from(server));

        if (server->last_message) {
                json_object_object_add(query,
                                       "message",
                                       json_object_get(server->last_message));
        }

        struct json_object *update = json_object_new_object();

        json_object_object_add(update, "callback_query", query);

        add_update(server, update);
}

static struct {
        const char *name;
        void (* func)(struct server *server,
                      const char *data,
                      size_t data_length);
} stdin_commands[] = {
        { "m", handle_add_message },
        { "cb", handle_add_callback_data },
        { "c", handle_change_chat },
        { "u", handle_change_user },
        { "private", handle_set_private },
        { "public", handle_set_public },
        { "user_name", handle_set_user_name },
        { "chat_name", handle_set_chat_name },
};

static void
handle_stdin_command(struct server *server,
                     const char *command,
                     size_t command_length)
{
        if (command_length <= 0 || *command == '#')
                return;

        const char *name_end = memchr(command, ' ', command_length);

        if (name_end == NULL)
                name_end = command + command_length;

        const char *data = name_end;
        size_t data_length = command + command_length - name_end;

        while (data_length > 0 && *data == ' ') {
                data++;
                data_length--;
        }

        for (unsigned i = 0; i < PCX_N_ELEMENTS(stdin_commands); i++) {
                if (strlen(stdin_commands[i].name) == name_end - command &&
                    !memcmp(stdin_commands[i].name,
                            command,
                            name_end - command)) {
                        stdin_commands[i].func(server, data, data_length);
                        return;
                }
        }

        fprintf(stderr,
                "unknown command “%.*s”\n",
                (int) (name_end - command),
                command);
}

static void
handle_stdin_commands(struct server *server)
{
        const char *buf_start = (const char *) server->stdin_buffer.data;
        const char *buf_end = (const char *) buf_start +
                server->stdin_buffer.length;
        const char *end;

        while ((end = memchr(buf_start, '\n', buf_end - buf_start))) {
                handle_stdin_command(server,
                                     buf_start,
                                     end - buf_start);

                buf_start = end + 1;
        }

        server->stdin_buffer.length = buf_end - buf_start;

        memmove(server->stdin_buffer.data,
                buf_start,
                server->stdin_buffer.length);
}

static void
stdin_cb(struct pcx_main_context_source *source,
         int fd,
         enum pcx_main_context_poll_flags flags,
         void *user_data)
{
        struct server *server = user_data;

        pcx_buffer_ensure_size(&server->stdin_buffer,
                               server->stdin_buffer.length + 1024);

        int got = read(fd,
                       server->stdin_buffer.data + server->stdin_buffer.length,
                       server->stdin_buffer.size - server->stdin_buffer.length);

        if (got == -1) {
                fprintf(stderr,
                        "error reading from stdin: %s\n",
                        strerror(errno));
                server->quit = true;
                server->ret = EXIT_FAILURE;
        } else if (got == 0) {
                server->quit = true;
        } else {
                server->stdin_buffer.length += got;

                handle_stdin_commands(server);
        }
}

static void
free_connections(struct server *server)
{
        struct connection *connection, *temp;

        pcx_list_for_each_safe(connection, temp, &server->connections, link) {
                free_connection(connection);
        }
}

static void
free_updates(struct server *server)
{
        struct update *update, *temp;

        pcx_list_for_each_safe(update, temp, &server->updates, link) {
                free_update(update);
        }
}

static void
free_names(struct pcx_list *list)
{
        struct name *name, *temp;

        pcx_list_for_each_safe(name, temp, list, link) {
                pcx_free(name->value);
                pcx_free(name);
        }
}

static void
free_server(struct server *server)
{
        free_connections(server);
        free_updates(server);
        free_names(&server->chat_names);
        free_names(&server->user_names);

        if (server->last_message)
                json_object_put(server->last_message);

        if (server->listen_socket != -1)
                pcx_close(server->listen_socket);

        if (server->stdin_source)
                pcx_main_context_remove_source(server->stdin_source);

        if (server->listen_socket_source)
                pcx_main_context_remove_source(server->listen_socket_source);

        pcx_buffer_destroy(&server->stdin_buffer);

        pcx_free(server);
}

int
main(int argc, char **argv)
{
        struct server *server = pcx_calloc(sizeof (struct server));

        pcx_buffer_init(&server->stdin_buffer);
        pcx_list_init(&server->connections);
        pcx_list_init(&server->updates);
        pcx_list_init(&server->chat_names);
        pcx_list_init(&server->user_names);
        server->next_update_id = 1;
        server->next_message_id = 1;
        server->next_query_id = 1;
        server->current_chat_id = 1;
        server->current_user_id = 1;
        server->ret = EXIT_SUCCESS;

        struct pcx_error *error = NULL;

        server->listen_socket = pcx_listen_socket_create_for_port(LISTEN_PORT,
                                                                  &error);

        if (server->listen_socket == -1) {
                fprintf(stderr,
                        "error creating listen socket: %s\n",
                        error->message);
                pcx_error_free(error);
                server->ret = EXIT_FAILURE;
                goto out;
        }

        server->stdin_source =
                pcx_main_context_add_poll(NULL,
                                          STDIN_FILENO,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          stdin_cb,
                                          server);

        server->listen_socket_source =
                pcx_main_context_add_poll(NULL,
                                          server->listen_socket,
                                          PCX_MAIN_CONTEXT_POLL_IN |
                                          PCX_MAIN_CONTEXT_POLL_ERROR,
                                          listen_socket_cb,
                                          server);

        do
                pcx_main_context_poll(NULL);
        while (!server->quit);

out: (void) 0;

        int ret = server->ret;

        free_server(server);

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
