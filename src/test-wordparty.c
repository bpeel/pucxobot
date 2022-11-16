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
#include <inttypes.h>

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
        char *typed_word;

        uint32_t letters_used;

        bool had_player_num;

        struct pcx_list messages;
};

struct test_harness {
        char *data_dir;
        struct pcx_class_store *class_store;
        struct pcx_config *config;
        struct pcx_server *server;
        int n_connections;
        struct test_connection connections[16];
        bool had_error;

        /* Bitmask of sideband data points that have been modified
         * since the bitmask was last reset.
         */
        uint64_t sideband_modified;

        int start_player;
        int current_player;
        char *current_syllable;
        int result_player;
        int result_value;
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

static const char * const
syllabary[] = {
        "om",
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
create_syllabary(const char *data_dir,
                 const char * const *syllables,
                 int n_syllables)
{
        char *full_filename = pcx_strconcat(data_dir,
                                            "/syllabary-eo.bin",
                                            NULL);
        FILE *out = fopen(full_filename, "wb");
        bool ret = true;

        if (out == NULL) {
                fprintf(stderr, "%s: %s\n", full_filename, strerror(errno));
                ret = false;
        } else {
                for (int i = 0; i < n_syllables; i++) {
                        uint32_t hit_count_le = PCX_UINT32_TO_LE(i + 1);
                        fwrite(&hit_count_le, 1, sizeof hit_count_le, out);

                        fputs(syllables[i], out);

                        for (int j = strlen(syllables[i]); j < 16; j++)
                                fputc(0, out);
                }

                fclose(out);
        }

        pcx_free(full_filename);

        return ret;
}

static char *
create_data_dir(const char *const * words,
                int n_words,
                const char *const *syllables,
                int n_syllables)
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
            !create_syllabary(data_dir, syllables, n_syllables)) {
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

                if (i == 15) {
                        pcx_buffer_append_string(&buf,
                                                 "La ludo nun estas plena kaj "
                                                 "tuj komenciĝos.");
                } else {
                        if (i == 0) {
                                pcx_buffer_append_string(&buf,
                                                         "Atendu pliajn "
                                                         "ludantojn por "
                                                         "komenci la ludon.");
                        } else {
                                pcx_buffer_append_string(&buf,
                                                         "Vi povas atendi "
                                                         "pliajn ludantojn aŭ "
                                                         "alklaki la suban "
                                                         "butonon por "
                                                         "komenci.");
                        }

                        pcx_buffer_append_string(&buf,
                                                 "\n"
                                                 "\n"
                                                 "La aktualaj ludantoj "
                                                 "estas:\n");

                        for (int j = 0; j <= i; j++) {
                                if (j > 0) {
                                        pcx_buffer_append_string(&buf,
                                                                 j == i ?
                                                                 " kaj " :
                                                                 ", ");
                                }

                                pcx_buffer_append_c(&buf, j + 'A');
                        }
                }

                pcx_buffer_append_c(&buf, '\0');

                if (!expect_message(harness, (const char *) buf.data)) {
                        ret = false;
                        break;
                }

                if (!harness->connections[i].had_player_num) {
                        fprintf(stderr,
                                "Missing PLAYER_NUM message\n");
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

                pcx_free(connection->typed_word);
        }

        if (harness->server)
                pcx_server_free(harness->server);

        if (harness->class_store)
                pcx_class_store_free(harness->class_store);

        if (harness->config)
                pcx_config_free(harness->config);

        if (harness->data_dir)
                free_data_dir(harness->data_dir);

        pcx_free(harness->current_syllable);

        pcx_free(harness);
}

static struct test_harness *
create_harness_with_dictionary(const char *const *words,
                               int n_words,
                               const char *const *syllables,
                               int n_syllables)
{
        struct test_harness *harness = pcx_calloc(sizeof *harness);

        harness->start_player = -1;
        harness->current_player = -1;
        harness->result_player = -1;
        harness->result_value = -1;

        for (int i = 0; i < PCX_N_ELEMENTS(harness->connections); i++) {
                harness->connections[i].fd = -1;
                harness->connections[i].harness = harness;
                pcx_list_init(&harness->connections[i].messages);
        }

        harness->data_dir = create_data_dir(words, n_words,
                                            syllables, n_syllables);

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
                                              PCX_N_ELEMENTS(valid_words),
                                              syllabary,
                                              PCX_N_ELEMENTS(syllabary));
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
                harness->had_error = true;
                return false;
        }

        if (harness->start_player == -1)
                harness->start_player = current_player;

        harness->current_player = current_player;

        return true;
}

static bool
handle_n_lives(struct test_harness *harness,
               int player_num,
               int n_lives)
{
        harness->connections[player_num].n_lives = n_lives;

        return true;
}

static bool
handle_syllable(struct test_harness *harness,
                const char *syllable,
                int syllable_length)
{
        const char *end = memchr(syllable, '\0', syllable_length);

        if (end == NULL) {
                fprintf(stderr,
                        "Missing zero terminator in current syllable "
                        "command.\n");
                harness->had_error = true;
                return false;
        }

        pcx_free(harness->current_syllable);
        harness->current_syllable = pcx_strdup(syllable);

        return true;
}

static bool
handle_word_result(struct test_harness *harness,
                   uint8_t data)
{
        int player = data & 0x3f;

        if (player >= harness->n_connections) {
                fprintf(stderr,
                        "Received invalid player number in word result: %i\n",
                        player);
                harness->had_error = true;
                return false;
        }

        int result = data >> 6;

        if (result > 2) {
                fprintf(stderr,
                        "Received invalid word result: %i\n",
                        result);
                harness->had_error = true;
                return false;
        }

        harness->result_player = player;
        harness->result_value = result;

        return true;
}

static bool
handle_typed_word(struct test_harness *harness,
                  int player_num,
                  const char *word,
                  int word_length)
{
        const char *end = memchr(word, '\0', word_length);

        if (end == NULL) {
                fprintf(stderr,
                        "Missing zero terminator in typed word "
                        "command.\n");
                harness->had_error = true;
                return false;
        }

        struct test_connection *connection = harness->connections + player_num;

        pcx_free(connection->typed_word);
        connection->typed_word = pcx_strdup(word);

        return true;
}

static bool
handle_letters_used(struct test_harness *harness,
                    int player_num,
                    const char *data,
                    int data_length)
{
        uint32_t letters_used_le;

        if (data_length != sizeof letters_used_le) {
                fprintf(stderr,
                        "Invalid letters used sideband data.\n");
                harness->had_error = true;
                return false;
        }

        memcpy(&letters_used_le, data, sizeof letters_used_le);

        harness->connections[player_num].letters_used =
                PCX_UINT32_FROM_LE(letters_used_le);

        return true;
}

static bool
handle_sideband(struct test_connection *connection,
                const uint8_t *command,
                size_t command_length)
{
        if (command_length < 3)
                goto error;

        connection->harness->sideband_modified |= UINT64_C(1) << command[1];

        int command_num = command[1];

        if (command_num == 0)
                return handle_current_player(connection->harness, command[2]);

        command_num--;

        if (command_num < connection->harness->n_connections) {
                return handle_n_lives(connection->harness,
                                      command_num,
                                      command[2]);
        }

        command_num -= connection->harness->n_connections;

        if (command_num == 0) {
                return handle_syllable(connection->harness,
                                       (const char *) command + 2,
                                       command_length - 2);
        }

        command_num--;

        if (command_num == 0) {
                return handle_word_result(connection->harness,
                                          command[2]);
        }

        command_num--;

        if (command_num < connection->harness->n_connections) {
                return handle_typed_word(connection->harness,
                                         command_num,
                                         (const char *) command + 2,
                                         command_length - 2);
        }

        command_num -= connection->harness->n_connections;

        if (command_num < connection->harness->n_connections) {
                return handle_letters_used(connection->harness,
                                           command_num,
                                           (const char *) command + 2,
                                           command_length - 2);
        }

error:
        fprintf(stderr,
                "Invalid sideband command received\n");
        connection->harness->had_error = true;

        return false;
}

static bool
handle_player_num(struct test_connection *connection,
                  const uint8_t *command,
                  size_t command_length)
{
        if (command_length != 2) {
                fprintf(stderr,
                        "Invalid sideband command received\n");
                connection->harness->had_error = true;
                return false;
        }

        if (connection->had_player_num) {
                fprintf(stderr,
                        "Player num received more than once\n");
                connection->harness->had_error = true;
                return false;
        }

        int received_player_num = command[1];
        int expected_player_num = connection - connection->harness->connections;

        if (received_player_num != expected_player_num) {
                fprintf(stderr,
                        "Incorrect player num\n"
                        " Expected: %i\n"
                        " Received: %i\n",
                        expected_player_num,
                        received_player_num);
                connection->harness->had_error = true;
                return false;
        }

        connection->had_player_num = true;

        return true;
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
                        case 0x07:
                                if (!handle_player_num(connection,
                                                       connection->read_buf +
                                                       data_start,
                                                       length))
                                        return;
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

        if (n_players < 16) {
                static const uint8_t start_command[] = {
                        0x82, 7, 0x82, 's', 't', 'a', 'r', 't', '\0',
                };

                if (!write_all(harness->connections[0].fd,
                               start_command,
                               sizeof start_command))
                        return false;

                if (!check_start_condition(harness))
                        return false;
        }

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
send_sideband_data(struct test_connection *connection,
                   int data_num,
                   const char *text)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        int text_length = strlen(text);

        pcx_buffer_append_c(&buf, 0x82);
        pcx_buffer_append_c(&buf, text_length + 3);
        pcx_buffer_append_c(&buf, 0x88);
        pcx_buffer_append_c(&buf, data_num);
        pcx_buffer_append(&buf, text, text_length + 1);

        bool ret = write_all(connection->fd, buf.data, buf.length);

        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
send_typed_word(struct test_connection *connection,
                const char *word)
{
        return send_sideband_data(connection, 0 /* data_num */, word);
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
check_word_result(struct test_harness *harness,
                  int player,
                  int result)
{
        if (harness->result_player != player ||
            harness->result_value != result) {
                fprintf(stderr,
                        "Unexpected word result\n"
                        " Expected: %i:%i\n"
                        " Received: %i:%i\n",
                        player, result,
                        harness->result_player, harness->result_value);
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
check_typed_word(struct test_connection *connection,
                 const char *word)
{
        if (connection->typed_word == NULL ||
            strcmp(word, connection->typed_word)) {
                fprintf(stderr,
                        "Typed word does not match.\n"
                        " Expected: %s\n"
                        " Received: %s\n",
                        word,
                        connection->typed_word);
                return false;
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

        if (!check_word_result(harness, harness->start_player, 0)) {
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

        if (!check_word_result(harness, harness->start_player, 1)) {
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

        /* Send the same word again. This should send the same result.
         * We want to ensure we still get the command for the result
         * even though it’s actually the same value.
         */
        harness->sideband_modified = 0;

        if (!send_word(harness->connections + harness->start_player,
                       "notawordomo")) {
                ret = false;
                goto out;
        }

        if (!expect_message(harness, "👎️ notawordomo")) {
                ret = false;
                goto out;
        }

        if ((harness->sideband_modified &
             (UINT64_C(1) << (harness->n_connections + 2))) == 0) {
                fprintf(stderr,
                        "Same result not resent after sending the same "
                        "wrong word twice.\n");
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

        if (!check_word_result(harness, harness->start_player, 0)) {
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

        if (!check_word_result(harness, harness->start_player ^ 1, 2)) {
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

        if (!check_word_result(harness, harness->start_player ^ 1, 1)) {
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
check_letters_used(struct test_connection *connection,
                   uint32_t expected_letters_used)
{
        uint32_t actual_letters_used = connection->letters_used;

        if (actual_letters_used != expected_letters_used) {
                fprintf(stderr,
                        "letters_used does not match.\n"
                        " Expected 0x%" PRIx32 "\n"
                        " Received 0x%" PRIx32 "\n",
                        expected_letters_used,
                        actual_letters_used);
                return false;
        }

        return true;
}

static int
compare_string(const void *pa, const void *pb)
{
        const char * const *a = pa;
        const char * const *b = pb;

        return strcmp(*a, *b);
}

static int
find_letter(const char *const * sorted_alphabet, const char *letter)
{
        int letter_bit;

        for (letter_bit = 0;
             strcmp(sorted_alphabet[letter_bit], letter);
             letter_bit++);

        return letter_bit;
}

static bool
run_full_alphabet(struct test_harness *harness,
                  int base_letter)
{
        struct pcx_buffer word_buf = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer bonus_buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        const char *sorted_alphabet[PCX_N_ELEMENTS(alphabet)];

        memcpy(sorted_alphabet, alphabet, sizeof alphabet);
        qsort(sorted_alphabet,
              PCX_N_ELEMENTS(alphabet),
              sizeof (const char *),
              compare_string);

        uint32_t expected_letters_used = ((UINT32_C(1) << ('o' - 'a')) |
                                          (UINT32_C(1) << ('m' - 'a')));

        for (int i = 0; i < PCX_N_ELEMENTS(alphabet); i++) {
                const char *letter = alphabet[i];
                int letter_bit = find_letter(sorted_alphabet, letter);

                expected_letters_used |= UINT32_C(1) << letter_bit;

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

                        uint32_t player_expected_letters;

                        if (connection->n_lives < 3) {
                                player_expected_letters =
                                        expected_letters_used |
                                        (UINT32_C(1) <<
                                         (base_letter - 'a' + player));
                        } else {
                                player_expected_letters = UINT32_MAX;
                        }

                        if (!check_letters_used(connection,
                                                player_expected_letters)) {
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
                                               n_words,
                                               syllabary,
                                               PCX_N_ELEMENTS(syllabary));

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

        /* When the life gauge is full the letters used mask should
         * always be full as well.
         */
        for (int i = 0; i < harness->n_connections; i++) {
                if (!check_letters_used(harness->connections + i, UINT32_MAX)) {
                        ret = false;
                        goto out;
                }
        }

        /* Check that losing a life resets the letters used mask */
        test_time_hack_add_time(61);

        if (!send_ping(harness) ||
            !sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_letters_used(harness->connections +
                                (harness->current_player ^ 1),
                                0)) {
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

        if (!expect_message(harness,
                            "La ludo finiĝis. La venkinto estas…\n"
                            "\n"
                            "🏆 <b>A</b> 🏆")) {
                ret = false;
                goto out;
        }

out:
        free_harness(harness);
        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
syllable_matches(const char *a, const char *b)
{
        while (*a) {
                if (*b == '\0')
                        return false;

                if (pcx_hat_to_lower(pcx_utf8_get_char(a)) !=
                    pcx_hat_to_lower(pcx_utf8_get_char(b)))
                        return false;

                a = pcx_utf8_next(a);
                b = pcx_utf8_next(b);
        }

        return *b == '\0';
}

static int
find_syllable(char **syllables,
              size_t n_syllables,
              const char *syllable)
{
        for (unsigned i = 0; i < n_syllables; i++) {
                if (syllable_matches(syllables[i], syllable))
                        return i;
        }

        return -1;
}

static bool
test_current_syllable(void)
{
        const size_t n_syllables = 16 * 16 * 16;
        char **syllables = pcx_alloc(sizeof (char *) * n_syllables);

        for (int i = 0; i < n_syllables; i++) {
                syllables[i] = pcx_strconcat(alphabet[i & 0xf],
                                             alphabet[(i >> 4) & 0xf],
                                             alphabet[(i >> 8) & 0xf],
                                             NULL);
        }

        const size_t n_players = 8;

        /* Make a word for each player for each syllable so that we
         * never reuse the word even if we get the same syllable
         * twice.
         */
        const size_t n_words = n_syllables * n_players;
        char **words = pcx_alloc(sizeof (char *) * n_words);

        for (int i = 0; i < n_words; i++) {
                words[i] = pcx_strconcat(alphabet[i / n_syllables],
                                         syllables[i % n_syllables],
                                         NULL);
        }

        bool ret = true;

        struct test_harness *harness =
                create_harness_with_dictionary((const char *const *) words,
                                               n_words,
                                               (const char *const *) syllables,
                                               n_syllables);

        if (harness == NULL) {
                ret = false;
                goto out;
        }

        if (!create_game(harness, n_players)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < n_players; i++) {
                if (!sync_with_server(harness)) {
                        ret = false;
                        goto out;
                }

                if (!check_current_player(harness,
                                          (harness->start_player + i) %
                                          n_players)) {
                        ret = false;
                        goto out;
                }

                int syllable_num;

                for (int p = 0; p < n_players; p++) {
                        const struct test_connection *conn =
                                harness->connections + p;

                        if (pcx_list_empty(&conn->messages)) {
                                fprintf(stderr,
                                        "No message received while expecting "
                                        "turn message.\n");
                                ret = false;
                                goto out;
                        }

                        struct test_message *message =
                                pcx_container_of(conn->messages.next,
                                                 struct test_message,
                                                 link);

                        const char *note_marker =
                                "enhavas:\n\n<b>";

                        const char *note = strstr(message->text, note_marker);

                        if (note == NULL) {
                                fprintf(stderr,
                                        "Note doesn’t contain the syllable "
                                        "part\n");
                                ret = false;
                                goto out;
                        }

                        const char *syllable = note + strlen(note_marker);
                        char *syllable_end = strchr(syllable, '<');

                        if (syllable_end == NULL) {
                                fprintf(stderr,
                                        "Missing syllable terminator in "
                                        "message.\n");
                                ret = false;
                                goto out;
                        }

                        *syllable_end = '\0';

                        syllable_num = find_syllable(syllables,
                                                     n_syllables,
                                                     syllable);

                        if (syllable_num == -1) {
                                fprintf(stderr,
                                        "Game reported syllable “%s” which "
                                        "isn’t in the list.\n",
                                        syllable);
                                ret = false;
                                goto out;
                        }

                        if (harness->current_syllable == NULL ||
                            strcmp(harness->current_syllable, syllable)) {
                                fprintf(stderr,
                                        "Game reported current syllable is "
                                        "“%s” but in message it is “%s”\n",
                                        harness->current_syllable,
                                        syllable);
                                ret = false;
                                goto out;
                        }

                        free_message(message);
                }

                if (!send_word(harness->connections +
                               harness->current_player,
                               words[syllable_num +
                                     i * n_syllables])) {
                        ret = false;
                        goto out;
                }
        }

out:
        if (harness)
                free_harness(harness);

        for (int i = 0; i < n_syllables; i++)
                pcx_free(syllables[i]);
        pcx_free(syllables);

        for (int i = 0; i < n_words; i++)
                pcx_free(words[i]);
        pcx_free(words);

        return ret;
}

static bool
test_typed_word(void)
{
        /* Use the full number of players so we can test the large
         * amount of sideband data that will be involved.
         */
        const size_t n_players = 16;

        /* Create a dictionary with a word for each player */
        char **words = pcx_alloc(sizeof (char *) * n_players);

        for (int i = 0; i < n_players; i++) {
                words[i] = pcx_strconcat(alphabet[i],
                                         syllabary[0],
                                         NULL);
        }

        bool ret = true;

        struct test_harness *harness =
                create_harness_with_dictionary((const char *const *) words,
                                               n_players,
                                               syllabary,
                                               1 /* n_syllables */);

        if (harness == NULL) {
                ret = false;
                goto out;
        }

        if (!create_game(harness, n_players)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < n_players; i++) {
                if (!expect_turn_message(harness)) {
                        ret = false;
                        goto out;
                }

                if (!check_current_player(harness,
                                          (harness->start_player + i) %
                                          n_players)) {
                        ret = false;
                        goto out;
                }

                if (!send_word(harness->connections +
                               harness->current_player,
                               words[i])) {
                        ret = false;
                        goto out;
                }

                if (!sync_with_server(harness)) {
                        ret = false;
                        goto out;
                }

                struct test_connection *connection =
                        harness->connections +
                        (harness->start_player + i) %
                        n_players;

                if (!check_typed_word(connection, words[i])) {
                        ret = false;
                        goto out;
                }

                /* Check that we got an update of the letters used for
                 * this player.
                 */
                if (connection->letters_used == 0) {
                        fprintf(stderr,
                                "letters_used is zero\n");
                        ret = false;
                        goto out;
                }
        }

        if (!expect_turn_message(harness)) {
                ret = false;
                goto out;
        }

        /* Check that typing a bad word also updates the typed word */

        if (!send_word(harness->connections + harness->start_player, "zzz")) {
                ret = false;
                goto out;
        }

        if (!expect_message(harness, "👎️ zzz")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections + harness->start_player,
                              "zzz")) {
                ret = false;
                goto out;
        }

        /* Check that typing from another player doesn’t update the
         * typed word.
         */

        if (!send_word(harness->connections +
                       (harness->start_player + 1) % n_players,
                       "zzz")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections +
                              (harness->start_player + 1) % n_players,
                              "bom") ||
            !check_typed_word(harness->connections + harness->start_player,
                              "zzz")) {
                ret = false;
                goto out;
        }

        /* Test updating the in-progress typed word */

        if (!send_typed_word(harness->connections + harness->start_player,
                             " eĤoŜanĝoĉiuĵaŭde ")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections + harness->start_player,
                              "eĥoŝanĝoĉiuĵaŭde")) {
                ret = false;
                goto out;
        }

        /* Check that setting the in-progress word from another player
         * doesn’t update the typed word.
         */

        if (!send_typed_word(harness->connections +
                             (harness->start_player + 1) % n_players,
                             "kato")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections +
                              (harness->start_player + 1) % n_players,
                              "bom") ||
            !check_typed_word(harness->connections + harness->start_player,
                              "eĥoŝanĝoĉiuĵaŭde")) {
                ret = false;
                goto out;
        }

        /* Test that sending an invalid word clears it instead */

        if (!send_typed_word(harness->connections + harness->start_player,
                             "a sentence is not a word")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections + harness->start_player,
                              "")) {
                ret = false;
                goto out;
        }

        /* Test that sending an invalid data num does nothing */

        if (!send_sideband_data(harness->connections + harness->start_player,
                                1,
                                "a")) {
                ret = false;
                goto out;
        }

        if (!sync_with_server(harness)) {
                ret = false;
                goto out;
        }

        if (!check_typed_word(harness->connections + harness->start_player,
                              "")) {
                ret = false;
                goto out;
        }

out:
        if (harness)
                free_harness(harness);

        for (int i = 0; i < n_players; i++)
                pcx_free(words[i]);
        pcx_free(words);

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

        if (!test_current_syllable())
                ret = EXIT_FAILURE;

        if (!test_typed_word())
                ret = EXIT_FAILURE;

        pcx_log_close();

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
