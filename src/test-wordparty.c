/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2022  Neil Roberts
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <sys/socket.h>

#include "pcx-server.h"
#include "pcx-util.h"
#include "pcx-netaddress.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "pcx-list.h"
#include "pcx-hat.h"
#include "pcx-utf8.h"
#include "test-time-hack.h"

struct test_message {
        struct pcx_list link;

        /* Over-allocated */
        char text[1];
};

struct test_connection {
        char name[2];
        int fd;
        struct pcx_main_context_source *read_source;
        struct test_harness *harness;
        uint8_t read_buf[1024];
        int read_pos;

        int n_lives;

        struct pcx_list messages;
};

struct test_harness {
        char *data_dir;
        struct pcx_class_store *class_store;
        struct pcx_config *config;
        struct pcx_server *server;
        int n_connections;
        struct test_connection connections[4];
        bool had_error;

        int start_player;
        int current_player;
};

#define TEST_PORT 6636

static const char
config_file[] =
        "[server]\n"
        "address=" PCX_STRINGIFY(TEST_PORT) "\n"
        "\n"
        "[general]\n"
        "log_file=/dev/stderr\n";

static const char * const
valid_words[] = {
        "terpomo",
        "sako",
};

static const uint8_t
syllabary[] = {
        /* One syllable ‘OM’ */
        1, 0, 0, 0, /* hit count in LE */
        'o', 'm', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char * const
alphabet[] = {
        "a", "b", "c", "ĉ", "d", "e", "f", "g", "ĝ", "h", "ĥ", "i", "j", "ĵ",
        "k", "l", "m", "n", "o", "p", "r", "s", "ŝ", "t", "u", "ŭ", "v", "z",
};

static bool
write_all(int fd, const uint8_t *data, size_t size)
{
        size_t pos = 0;

        while (pos < size) {
                int wrote = write(fd, data + pos, size - pos);

                if (wrote == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        return false;
                }

                pos += wrote;
        }

        return true;
}

static const char *
get_temp_dir(void)
{
        const char *temp_dir = getenv("TMPDIR");

        if (temp_dir == NULL)
                return "/tmp";
        else
                return temp_dir;
}

static void
free_data_dir(char *data_dir)
{
        DIR *dir = opendir(data_dir);

        if (dir == NULL) {
                fprintf(stderr, "%s: %s\n", data_dir, strerror(errno));
        } else {
                struct dirent *dirent;

                while ((dirent = readdir(dir))) {
                        if (dirent->d_name[0] != '.') {
                                char *full_name = pcx_strconcat(data_dir,
                                                                "/",
                                                                dirent->d_name,
                                                                NULL);
                                unlink(full_name);
                                pcx_free(full_name);
                        }
                }

                closedir(dir);
        }

        rmdir(data_dir);

        pcx_free(data_dir);
}

static FILE *
open_dictionary_builder(const char *dictionary_file)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf, "$(dirname '");
        pcx_buffer_ensure_size(&buf, buf.length + 1024);

        ssize_t link_size = readlink("/proc/self/exe",
                                     (char *) buf.data + buf.length,
                                     buf.size - buf.length);

        FILE *ret = NULL;

        if (link_size == -1) {
                fprintf(stderr, "readlink failed: %s\n", strerror(errno));
        } else {
                buf.length += link_size;

                pcx_buffer_append_string(&buf, "')/make-dictionary '");
                pcx_buffer_append_string(&buf, dictionary_file);
                pcx_buffer_append_string(&buf, "'");

                ret = popen((const char *) buf.data, "w");

                if (ret == NULL) {
                        fprintf(stderr,
                                "popen make-dictionary failed: %s\n",
                                strerror(errno));
                }
        }

        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
create_dictionary(const char *data_dir,
                  const char * const *words,
                  int n_words)
{
        char *full_filename = pcx_strconcat(data_dir,
                                            "/dictionary-eo.bin",
                                            NULL);
        FILE *out = open_dictionary_builder(full_filename);

        pcx_free(full_filename);

        if (out == NULL)
                return false;

        for (int i = 0; i < n_words; i++)
                fprintf(out, "%s\n", words[i]);

        if (pclose(out) != 0) {
                fprintf(stderr, "make-dictionary failed\n");
                return false;
        }

        return true;
}

static bool
create_data_file(const char *data_dir,
                 const char *filename,
                 const uint8_t *data,
                 size_t size)
{
        char *full_filename = pcx_strconcat(data_dir, "/", filename, NULL);
        FILE *out = fopen(full_filename, "wb");
        bool ret = true;

        if (out == NULL) {
                fprintf(stderr, "%s: %s\n", full_filename, strerror(errno));
                ret = false;
        } else {
                fwrite(data, 1, size, out);
                fclose(out);
        }

        pcx_free(full_filename);

        return ret;
}

static char *
create_data_dir(const char *const * words,
                int n_words)
{
        const char *temp_dir = get_temp_dir();

        char *data_dir = pcx_strconcat(temp_dir,
                                       "/test-wordparty-data-XXXXXX",
                                       NULL);

        if (mkdtemp(data_dir) == NULL) {
                fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
                return NULL;
        }

        if (!create_dictionary(data_dir, words, n_words) ||
            !create_data_file(data_dir,
                              "syllabary-eo.bin",
                              syllabary,
                              sizeof syllabary)) {
                free_data_dir(data_dir);
                return NULL;
        }

        return data_dir;
}

static struct pcx_config *
create_config(const char *data_dir)
{
        const char *temp_dir = get_temp_dir();

        struct pcx_config *ret = NULL;

        char *template = pcx_strconcat(temp_dir,
                                       "/test-wordparty-XXXXXX",
                                       NULL);

        int fd = mkstemp(template);

        if (fd == -1) {
                fprintf(stderr, "mkstemp failed: %s\n", strerror(errno));
        } else {
                char *data_line = pcx_strconcat("data_dir = ",
                                                data_dir,
                                                "\n",
                                                NULL);

                if (write_all(fd,
                              (const uint8_t *) config_file,
                              sizeof config_file - 1) &&
                    write_all(fd,
                              (const uint8_t *) data_line,
                              strlen(data_line))) {
                        struct pcx_error *error = NULL;

                        ret = pcx_config_load(template, &error);

                        if (ret == NULL) {
                                fprintf(stderr, "%s\n", error->message);
                                pcx_error_free(error);
                        }
                }

                pcx_free(data_line);

                pcx_close(fd);

                unlink(template);
        }

        pcx_free(template);

        return ret;
}

static void
clear_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct pcx_main_context_source **timeout_ptr = user_data;

        assert(source == *timeout_ptr);

        *timeout_ptr = NULL;
}

static bool
sync_with_server(struct test_harness *harness)
{
        /* We will call pcx_main_context_poll in a loop until it hits
         * a short timeout. Presumably at that point any actions will
         * have been handled and the timeout process would normally be
         * idle.
         */

        int poll_count = 0;

        while (true) {
                struct pcx_main_context_source *timeout =
                        pcx_main_context_add_timeout(NULL,
                                                     poll_count < 2 ? 0 : 16,
                                                     clear_timeout_cb,
                                                     &timeout);

                pcx_main_context_poll(NULL);

                if (timeout)
                        pcx_main_context_remove_source(timeout);
                else if (poll_count >= 2)
                        break;

                poll_count++;
        }

        return !harness->had_error;
}

static void
free_message(struct test_message *message)
{
        pcx_list_remove(&message->link);
        pcx_free(message);
}

static void
flush_messages(struct test_connection *connection)
{
        struct test_message *message, *tmp;

        pcx_list_for_each_safe(message, tmp, &connection->messages, link) {
                free_message(message);
        }
}

static bool
expect_message_on_connection(struct test_connection *connection,
                             const char *text)
{
        struct test_harness *harness = connection->harness;

        if (!sync_with_server(harness))
                return false;

        if (pcx_list_empty(&connection->messages)) {
                fprintf(stderr,
                        "No message received while expecting: %s\n",
                        text);
                return false;
        }

        struct test_message *message =
                pcx_container_of(connection->messages.next,
                                 struct test_message,
                                 link);

        if (strcmp(text, message->text)) {
                fprintf(stderr,
                        "Message not as expected.\n"
                        " Expected: %s\n"
                        " Received: %s\n",
                        text,
                        message->text);
                return false;
        }

        free_message(message);

        return true;
}

static bool
expect_message(struct test_harness *harness,
               const char *text)
{
        for (int i = 0; i < harness->n_connections; i++) {
                if (!expect_message_on_connection(harness->connections + i,
                                                  text))
                        return false;
        }

        return true;
}

static bool
expect_join_messages(struct test_harness *harness)
{
        bool ret = true;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (int i = 0; i < harness->n_connections; i++) {
                pcx_buffer_set_length(&buf, 0);

                pcx_buffer_append_printf(&buf,
                                         "%s aliĝis al la ludo. ",
                                         harness->connections[i].name);

                if (i == 0) {
                        pcx_buffer_append_string(&buf,
                                                 "Atendu pliajn "
                                                 "ludantojn por komenci "
                                                 "la ludon.");
                } else {
                        pcx_buffer_append_string(&buf,
                                                 "Vi povas atendi pliajn "
                                                 "ludantojn aŭ alklaki la "
                                                 "suban butonon por komenci.");
                }

                pcx_buffer_append_string(&buf,
                                         "\n"
                                         "\n"
                                         "La aktualaj ludantoj estas:\n");

                for (int j = 0; j <= i; j++) {
                        if (j > 0) {
                                pcx_buffer_append_string(&buf,
                                                         j == i ?
                                                         " kaj " :
                                                         ", ");
                        }

                        pcx_buffer_append_c(&buf, j + 'A');
                }

                pcx_buffer_append_c(&buf, '\0');

                if (!expect_message(harness, (const char *) buf.data)) {
                        ret = false;
                        break;
                }
        }

        pcx_buffer_destroy(&buf);

        return ret;
}

static struct test_connection *
add_connection(struct test_harness *harness)
{
        assert(harness->n_connections < PCX_N_ELEMENTS(harness->connections));

        struct test_connection *connection =
                harness->connections + harness->n_connections;

        connection->fd = socket(PF_INET, SOCK_STREAM, 0);

        if (connection->fd == -1) {
                fprintf(stderr,
                        "error creating socket: %s\n",
                        strerror(errno));
                goto error;
        }

        struct pcx_netaddress local_address;

        if (!pcx_netaddress_from_string(&local_address,
                                        "127.0.0.1",
                                        TEST_PORT)) {
                fprintf(stderr, "error getting local address\n");
                goto error;
        }

        struct pcx_netaddress_native native_local_address;

        pcx_netaddress_to_native(&local_address, &native_local_address);

        if (connect(connection->fd,
                    &native_local_address.sockaddr,
                    native_local_address.length) == -1) {
                fprintf(stderr,
                        "failed to connect to server: %s\n",
                        strerror(errno));
                goto error;
        }

        connection->name[0] = harness->n_connections + 'A';
        connection->name[1] = '\0';

        harness->n_connections++;

        return connection;

error:
        if (connection->fd != -1)
                pcx_close(connection->fd);

        return NULL;
}

static void
free_harness(struct test_harness *harness)
{
        for (int i = 0; i < PCX_N_ELEMENTS(harness->connections); i++) {
                struct test_connection *connection = harness->connections + i;

                if (connection->fd != -1)
                        pcx_close(connection->fd);
                if (connection->read_source)
                        pcx_main_context_remove_source(connection->read_source);

                flush_messages(connection);
        }

        if (harness->server)
                pcx_server_free(harness->server);

        if (harness->class_store)
                pcx_class_store_free(harness->class_store);

        if (harness->config)
                pcx_config_free(harness->config);

        if (harness->data_dir)
                free_data_dir(harness->data_dir);

        pcx_free(harness);
}

static struct test_harness *
create_harness_with_dictionary(const char *const *words,
                               int n_words)
{
        struct test_harness *harness = pcx_calloc(sizeof *harness);

        harness->start_player = -1;
        harness->current_player = -1;

        for (int i = 0; i < PCX_N_ELEMENTS(harness->connections); i++) {
                harness->connections[i].fd = -1;
                harness->connections[i].harness = harness;
                pcx_list_init(&harness->connections[i].messages);
        }

        harness->data_dir = create_data_dir(words, n_words);

        if (harness->data_dir == NULL)
                goto error;

        harness->config = create_config(harness->data_dir);

        if (harness->config == NULL)
                goto error;

        if (harness->config->log_file) {
                struct pcx_error *error = NULL;

                if (!pcx_log_set_file(harness->config->log_file, &error)) {
                        fprintf(stderr, "%s\n", error->message);
                        pcx_error_free(error);
                        goto error;
                }

                pcx_log_start();
        }

        harness->class_store = pcx_class_store_new();

        harness->server = pcx_server_new(harness->config,
                                         harness->class_store);

        struct pcx_config_server *server_conf;

        pcx_list_for_each(server_conf, &harness->config->servers, link) {
                struct pcx_error *error = NULL;

                if (!pcx_server_add_config(harness->server,
                                           server_conf,
                                           &error)) {
                        fprintf(stderr, "%s\n", error->message);
                        pcx_error_free(error);
                        goto error;
                }
        }

        return harness;

error:
        free_harness(harness);
        return NULL;
}

static struct test_harness *
create_harness(void)
{
        return create_harness_with_dictionary(valid_words,
                                              PCX_N_ELEMENTS(valid_words));
}

static const uint8_t ws_terminator[] = "\r\n\r\n";

struct ws_closure {
        struct test_connection *connection;
        char buf[sizeof ws_terminator - 1];
        int buf_length;
        bool done;
        bool ret;
};

static void
read_ws_cb(struct pcx_main_context_source *source,
           int fd,
           enum pcx_main_context_poll_flags flags,
           void *user_data)
{
        struct ws_closure *closure = user_data;

        int got = read(closure->connection->fd,
                       closure->buf + closure->buf_length,
                       sizeof closure->buf - closure->buf_length);

        if (got == 0) {
                fprintf(stderr,
                        "Unexpected EOF while looking for WS "
                        "terminator\n");
                closure->ret = false;
                closure->done = true;
        } else if (got < 0) {
                fprintf(stderr,
                        "Error reading from connection: %s\n",
                        strerror(errno));
                closure->ret = false;
                closure->done = true;
        } else {
                closure->buf_length += got;

                if (closure->buf_length >= sizeof closure->buf) {
                        if (!memcmp(closure->buf,
                                    ws_terminator,
                                    sizeof closure->buf)) {
                                closure->done = true;
                        } else {
                                memmove(closure->buf,
                                        closure->buf + 1,
                                        sizeof closure->buf - 1);
                                closure->buf_length--;
                        }
                }
        }
}

static bool
negotiate_ws(struct test_connection *connection)
{
        static const uint8_t ws_request[] =
                "GET / HTTP/1.1\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "\r\n";

        if (!write_all(connection->fd, ws_request, sizeof ws_request - 1))
                return false;

        struct ws_closure closure = {
                .connection = connection,
                .buf_length = 0,
                .done = false,
                .ret = true,
        };

        struct pcx_main_context_source *read_source =
                pcx_main_context_add_poll(NULL,
                                          connection->fd,
                                          PCX_MAIN_CONTEXT_POLL_IN,
                                          read_ws_cb,
                                          &closure);

        while (!closure.done)
                pcx_main_context_poll(NULL);

        pcx_main_context_remove_source(read_source);

        return closure.ret;
}

static void
set_connection_error(struct test_connection *connection)
{
        connection->harness->had_error = true;

        if (connection->read_source) {
                pcx_main_context_remove_source(connection->read_source);
                connection->read_source = NULL;
        }
}

static struct test_message *
add_message(struct test_connection *connection,
            const char *text)
{
        int length = strlen(text);

        struct test_message *message =
                pcx_calloc(offsetof(struct test_message, text) + length + 1);
        memcpy(message->text, text, length + 1);

        pcx_list_insert(connection->messages.prev, &message->link);

        return message;
}

static bool
handle_message(struct test_connection *connection,
               const uint8_t *command,
               size_t command_length)
{
        if (command_length < 3)
                goto error;

        const uint8_t *text_start = command + 2;
        size_t text_size = command - command_length - text_start;

        const uint8_t *text_end =
                memchr(text_start, '\0', text_size);

        if (text_end == NULL)
                goto error;

        add_message(connection, (const char *) text_start);

        return true;

error:
        fprintf(stderr,
                "Invalid message command received\n");
        set_connection_error(connection);
        return false;
}

static bool
handle_current_player(struct test_harness *harness,
                      int current_player)
{
        if (current_player >= harness->n_connections) {
                fprintf(stderr,
                        "Server tried to set current player to non-existant "
                        "player %i\n",
                        current_player);
                return false;
        }

        if (harness->start_player == -1)
                harness->start_player = current_player;

        harness->current_player = current_player;

        return true;
}

static bool
handle_sideband(struct test_connection *connection,
                const uint8_t *command,
                size_t command_length)
{
        if (command_length < 3)
                goto error;

        if (command[1] == 0)
                return handle_current_player(connection->harness, command[2]);

        int player_num = command[1] - 1;

        if (player_num >= connection->harness->n_connections)
                goto error;

        connection->harness->connections[player_num].n_lives = command[2];

        return true;

error:
        fprintf(stderr,
                "Invalid sideband command received\n");
        set_connection_error(connection);
        return false;
}

static void
process_commands(struct test_connection *connection)
{
        int pos = 0;

        while (pos + 2 <= connection->read_pos) {
                int length = connection->read_buf[pos + 1];
                int data_start = pos + 2;

                if (length == 126) {
                        if (data_start + 2 > connection->read_pos)
                                break;

                        length = ((connection->read_buf[data_start] << 8) |
                                  connection->read_buf[data_start + 1]);

                        data_start += 2;
                }

                if (data_start + length > connection->read_pos)
                        break;

                if (length >= 1) {
                        switch (connection->read_buf[data_start]) {
                        case 0x01:
                                if (!handle_message(connection,
                                                    connection->read_buf +
                                                    data_start,
                                                    length))
                                        return;
                                break;
                        case 0x06:
                                if (!handle_sideband(connection,
                                                     connection->read_buf +
                                                     data_start,
                                                     length))
                                        return;
                                break;
                        }
                }

                pos = data_start + length;
        }

        memmove(connection->read_buf,
                connection->read_buf + pos,
                connection->read_pos - pos);

        connection->read_pos -= pos;
}

static void
connection_read_cb(struct pcx_main_context_source *source,
                   int fd,
                   enum pcx_main_context_poll_flags flags,
                   void *user_data)
{
        struct test_connection *connection = user_data;

        int got = read(connection->fd,
                       connection->read_buf + connection->read_pos,
                       sizeof connection->read_buf - connection->read_pos);

        if (got == 0) {
                fprintf(stderr, "Unexpected EOF on socket\n");
                set_connection_error(connection);
        } else if (got == -1) {
                fprintf(stderr, "Read error on socket: %s\n", strerror(errno));
                set_connection_error(connection);
        } else {
                connection->read_pos += got;
                process_commands(connection);
        }
}

static bool
check_start_condition(struct test_harness *harness)
{
        if (!sync_with_server(harness))
                return false;

        if (harness->current_player != harness->start_player) {
                fprintf(stderr,
                        "First player is not starting player\n");
                return false;
        }

        for (int i = 0; i < harness->n_connections; i++) {
                struct test_connection *connection = harness->connections + i;

                if (connection->n_lives != 2) {
                        fprintf(stderr,
                                "Player %i starting with wrong number of lives "
                                "(%i)\n",
                                i + 1,
                                connection->n_lives);
                        return false;
                }
        }

        return true;
}

static bool
create_game(struct test_harness *harness,
            int n_players)
{
        for (int i = 0; i < n_players; i++) {
                struct test_connection *connection = add_connection(harness);

                if (connection == NULL)
                        return false;

                if (!negotiate_ws(connection))
                        return false;

                const uint8_t join_message[] = {
                        0x82, 1 + 2 + 10 + 3, 0x80,
                        'A' + i, '\0',
                        'w', 'o', 'r', 'd', 'p', 'a', 'r', 't', 'y', '\0',
                        'e', 'o', '\0',
                };

                if (!write_all(connection->fd,
                               join_message,
                               sizeof join_message))
                        return false;

                connection->read_source =
                        pcx_main_context_add_poll(NULL,
                                                  connection->fd,
                                                  PCX_MAIN_CONTEXT_POLL_IN,
                                                  connection_read_cb,
                                                  connection);
        }

        if (!expect_join_messages(harness))
                return false;

        static const uint8_t start_command[] = {
                0x82, 7, 0x82, 's', 't', 'a', 'r', 't', '\0',
        };

        if (!write_all(harness->connections[0].fd,
                       start_command,
                       sizeof start_command))
                return false;

        if (!check_start_condition(harness))
                return false;

        return true;
}

static struct test_harness *
create_harness_with_game(int n_players)
{
        struct test_harness *harness = create_harness();

        if (harness == NULL)
                return NULL;

        if (!create_game(harness, n_players)) {
                free_harness(harness);
                return NULL;
        }

        return harness;
}

static bool
expect_turn_message_with_extra(struct test_harness *harness,
                               const char *extra_message)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        struct test_connection *current_player =
                harness->connections + harness->current_player;

        pcx_buffer_append_printf(&buf,
                                 "<b>%s</b> ",
                                 current_player->name);

        for (int i = 0; i < current_player->n_lives; i++)
                pcx_buffer_append_string(&buf, "❤️");

        pcx_buffer_append_string(&buf, "\n\n");

        if (extra_message) {
                pcx_buffer_append_string(&buf, extra_message);
                pcx_buffer_append_string(&buf, "\n\n");
        }

        pcx_buffer_append_string(&buf,
                                 "Tajpu vorton kiu enhavas:\n"
                                 "\n"
                                 "<b>OM</b>");

        bool ret = expect_message(harness, (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
expect_turn_message(struct test_harness *harness)
{
        return expect_turn_message_with_extra(harness,
                                              NULL /* extra_message */);
}

static bool
send_word(struct test_connection *connection,
          const char *word)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        int word_length = strlen(word);

        pcx_buffer_append_c(&buf, 0x82);
        pcx_buffer_append_c(&buf, word_length + 2);
        pcx_buffer_append_c(&buf, 0x85);
        pcx_buffer_append(&buf, word, word_length + 1);

        bool ret = write_all(connection->fd, buf.data, buf.length);

        if (ret) {
                pcx_buffer_set_length(&buf, 0);

                pcx_buffer_append_printf(&buf,
                                         "<b>%s</b>\n"
                                         "\n"
                                         "%s",
                                         connection->name,
                                         word);

                ret = expect_message(connection->harness,
                                     (const char *) buf.data);
        }

        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
send_ping(struct test_harness *harness)
{
        static const uint8_t ping_command[] = {
                0x82, 0x01, 0x83
        };

        for (int i = 0; i < harness->n_connections; i++) {
                if (!write_all(harness->connections[i].fd,
                               ping_command,
                               sizeof ping_command))
                        return false;
        }

        return true;
}

static bool
check_current_player(struct test_harness *harness,
                     int current_player)
{
        if (harness->current_player != current_player) {
                fprintf(stderr,
                        "Current player is wrong.\n"
                        " Expected: %i\n"
                        " Received: %i\n",
                        current_player,
                        harness->current_player);
                return false;
        }

        return true;
}

static bool
check_lives(struct test_harness *harness,
            int expected_lives)
{
        for (int i = 0; i < harness->n_connections; i++) {
                if (harness->connections[i].n_lives != expected_lives) {
                        fprintf(stderr,
                                "Number of lives for player %i is %i "
                                "but it should be %i.\n",
                                i + 1,
                                harness->connections[i].n_lives,
                                expected_lives);
                        return false;
                }
        }

        return true;
}

static bool
test_correct_word(void)
{
        struct test_harness *harness = create_harness_with_game(2);

        if (harness == NULL)
                return false;

        bool ret = true;

        if (!expect_turn_message(harness)) {
                ret = false;
                goto out;
        }

        if (!send_word(harness->connections + harness->start_player,
                       "terpomo")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_current_player(harness, (harness->start_player ^ 1))) {
                ret = false;
                goto out;
        }

        if (!check_lives(harness, 2)) {
                ret = false;
                goto out;
        }

        if (!expect_turn_message(harness)) {
                ret = false;
                goto out;
        }

out:
        free_harness(harness);
        return ret;
}

static bool
test_wrong_word(void)
{
        struct test_harness *harness = create_harness_with_game(2);

        if (harness == NULL)
                return false;

        bool ret = true;

        if (!expect_turn_message(harness)) {
                ret = false;
                goto out;
        }

        if (!send_word(harness->connections + harness->start_player,
                       "notawordomo")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_current_player(harness, harness->start_player)) {
                ret = false;
                goto out;
        }

        if (!check_lives(harness, 2)) {
                ret = false;
                goto out;
        }

        if (!expect_message(harness, "👎️ notawordomo")) {
                ret = false;
                goto out;
        }

        /* Send the correct word so we can try duplicating it */
        if (!send_word(harness->connections + harness->start_player,
                       "terpomo")) {
                ret = false;
                goto out;
        }

        if (!expect_turn_message(harness)) {
                ret = false;
                goto out;
        }

        if (!send_word(harness->connections + (harness->start_player ^ 1),
                       "terpomo")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_current_player(harness, (harness->start_player ^ 1))) {
                ret = false;
                goto out;
        }

        if (!check_lives(harness, 2)) {
                ret = false;
                goto out;
        }

        if (!expect_message(harness, "♻️ terpomo")) {
                ret = false;
                goto out;
        }

        /* Check a valid word that doesn’t contain the syllable */

        if (!send_word(harness->connections + (harness->start_player ^ 1),
                       "sako")) {
                ret = false;
                goto out;
        }

        if (!expect_message(harness, "👎️ sako")) {
                ret = false;
                goto out;
        }

out:
        free_harness(harness);
        return ret;
}

static void
make_bonus_string(struct pcx_buffer *buf,
                  const char * const *letters,
                  size_t n_letters)
{
        pcx_buffer_set_length(buf, 0);

        pcx_buffer_append_string(buf,
                                 "Uzu la jenajn literojn por regajni "
                                 "vivon: ");

        for (unsigned i = 0; i < n_letters; i++) {
                uint32_t ch = pcx_utf8_get_char(letters[i]);
                pcx_buffer_ensure_size(buf,
                                       buf->length + PCX_UTF8_MAX_CHAR_LENGTH);
                buf->length += pcx_utf8_encode(pcx_hat_to_upper(ch),
                                               (char *) buf->data +
                                               buf->length);
        }

        pcx_buffer_append_c(buf, '\0');
        buf->length--;
}

static bool
expect_bonus_life(struct test_connection *connection)
{
        char *message = pcx_strconcat("➕❤️ ",
                                      connection->name,
                                      " uzis la tutan alfabeton kaj gajnis "
                                      "bonusan vivon.",
                                      NULL);

        bool ret = expect_message(connection->harness, message);

        pcx_free(message);

        return ret;
}

static bool
run_full_alphabet(struct test_harness *harness,
                  int base_letter)
{
        struct pcx_buffer word_buf = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer bonus_buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        for (int i = 0; i < PCX_N_ELEMENTS(alphabet); i++) {
                const char *letter = alphabet[i];

                for (int player = 0; player < 2; player++) {
                        pcx_buffer_set_length(&word_buf, 0);
                        pcx_buffer_append_c(&word_buf, base_letter + player);
                        pcx_buffer_append_string(&word_buf, letter);
                        pcx_buffer_append_string(&word_buf, "om");

                        const char *bonus;

                        struct test_connection *connection =
                                harness->connections +
                                (harness->start_player ^ player);

                        bool can_have_bonus = connection->n_lives < 3;

                        if (can_have_bonus &&
                            i >= PCX_N_ELEMENTS(alphabet) - 8) {
                                make_bonus_string(&bonus_buf,
                                                  alphabet + i,
                                                  PCX_N_ELEMENTS(alphabet) - i);
                                bonus = (const char *) bonus_buf.data;
                        } else {
                                bonus = NULL;
                        }

                        if (!expect_turn_message_with_extra(harness, bonus)) {
                                ret = false;
                                goto out;
                        }

                        if (!send_word(connection,
                                       (const char *) word_buf.data)) {
                                ret = false;
                                goto out;
                        }

                        if (can_have_bonus &&
                            i + 1 >= PCX_N_ELEMENTS(alphabet) &&
                            !expect_bonus_life(connection)) {
                                ret = false;
                                goto out;
                        }
                }
        }

out:
        pcx_buffer_destroy(&bonus_buf);
        pcx_buffer_destroy(&word_buf);

        return ret;
}

static bool
test_full_alphabet(void)
{
        /* Create a dictionary that has four different words for each
         * letter of the alphabet followed by the syllable so that
         * each player can use the full alphabet twice.
         */
        const size_t words_per_letter = 4;
        const size_t n_words = PCX_N_ELEMENTS(alphabet) * words_per_letter;
        char **words = pcx_alloc(sizeof *words * n_words);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(alphabet); i++) {
                for (unsigned j = 0; j < words_per_letter; j++) {
                        char *word = pcx_strconcat(".",
                                                   alphabet[i],
                                                   "om",
                                                   NULL);
                        word[0] = j + 'a';

                        words[i * words_per_letter + j] = word;
                }
        }

        struct test_harness *harness =
                create_harness_with_dictionary((const char *const *) words,
                                               n_words);

        for (unsigned i = 0; i < n_words; i++)
                pcx_free(words[i]);

        pcx_free(words);

        if (harness == NULL)
                return false;

        bool ret = true;

        if (!create_game(harness, 2)) {
                ret = false;
                goto out;
        }

        if (!run_full_alphabet(harness, 'a')) {
                ret = false;
                goto out;
        }

        if (!check_lives(harness, 3)) {
                ret = false;
                goto out;
        }

        if (!run_full_alphabet(harness, 'c')) {
                ret = false;
                goto out;
        }

        /* The second time shouldn’t gain a life because it’s already
         * at the maximum.
         */
        if (!check_lives(harness, 3)) {
                ret = false;
                goto out;
        }

out:
        free_harness(harness);

        return ret;
}

static bool
test_death(void)
{
        struct test_harness *harness = create_harness_with_game(2);

        if (harness == NULL)
                return false;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        for (int i = 0; i < 3; i++) {
                if (!expect_turn_message(harness)) {
                        ret = false;
                        goto out;
                }

                /* There is only one syllable so it should always have
                 * the maximum timeout of 60 seconds. If we just
                 * advance by 50 seconds there should be no message.
                 */
                test_time_hack_add_time(50);

                /* Send a ping so that the connections don’t time out */
                if (!send_ping(harness)) {
                        ret = false;
                        goto out;
                }

                if (!sync_with_server(harness)) {
                        ret = false;
                        goto out;
                }

                if (!pcx_list_empty(&harness->connections[0].messages)) {
                        fprintf(stderr,
                                "After waiting only 50 seconds there is "
                                "already a message queued.\n");
                        ret = false;
                        goto out;
                }

                /* Wait for the rest of the time to trigger a timeout */
                test_time_hack_add_time(11);

                if (!send_ping(harness)) {
                        ret = false;
                        goto out;
                }

                struct test_connection *connection =
                        harness->connections +
                        (harness->start_player ^ (i & 1));

                pcx_buffer_set_length(&buf, 0);

                if (i == 2) {
                        pcx_buffer_append_printf(&buf,
                                                 "💥 %s prenis tro da tempo "
                                                 "kaj perdis sian lastan "
                                                 "vivon.",
                                                 connection->name);
                } else {
                        pcx_buffer_append_printf(&buf,
                                                 "💔 %s prenis tro da tempo "
                                                 "kaj perdis vivon.",
                                                 connection->name);
                }

                if (!expect_message(harness, (const char *) buf.data)) {
                        ret = false;
                        goto out;
                }
        }

out:
        free_harness(harness);
        pcx_buffer_destroy(&buf);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_correct_word())
                ret = EXIT_FAILURE;

        if (!test_wrong_word())
                ret = EXIT_FAILURE;

        if (!test_full_alphabet())
                ret = EXIT_FAILURE;

        if (!test_death())
                ret = EXIT_FAILURE;

        pcx_log_close();

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
