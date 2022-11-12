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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pcx-chameleon.h"
#include "pcx-list.h"
#include "pcx-main-context.h"
#include "pcx-log.h"
#include "test-message.h"
#include "test-time-hack.h"

struct test_data {
        struct pcx_chameleon *chameleon;
        struct test_message_data message_data;
        struct pcx_config *config;
        char *data_dir;
        int random_override;
};

static const char
basic_word_list[] =
        "Drinks\n"
        "Beer\n"
        "Tea\n"
        "Coffee\n"
        "Ovaltine\n"
        "\n"
        "Colors\n"
        "Red\n"
        "Green\n"
        "Blue\n"
        "Pink\n"
        "\n"
        "Animals\n"
        "Dog\n"
        "Cat\n"
        "Wolf\n"
        "Elephant\n"
        "\n"
        "Fruit\n"
        "Apple\n"
        "Pear\n"
        "Banana\n"
        "Orange\n";

static int
fake_random_number_generator(void *user_data)
{
        struct test_data *data = pcx_container_of(user_data,
                                                  struct test_data,
                                                  message_data);

        return data->random_override;
}

static char *
make_data_dir(void)
{
        const char *temp_dir = getenv("TMPDIR");

        if (temp_dir == NULL)
                temp_dir = "/tmp";

        char *data_dir = pcx_strconcat(temp_dir,
                                       "/test-chameleon-XXXXXX",
                                       NULL);

        if (mkdtemp(data_dir) == NULL) {
                fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
                pcx_free(data_dir);
                return NULL;
        }

        return data_dir;
}

static void
remove_data_dir(const char *name)
{
        DIR *dir = opendir(name);

        if (dir == NULL)
                return;

        while (true) {
                struct dirent *dirent = readdir(dir);

                if (dirent == NULL)
                        break;

                char *full_name =
                        pcx_strconcat(name, "/", dirent->d_name, NULL);
                struct stat statbuf;

                if (stat(full_name, &statbuf) == 0 &&
                    (statbuf.st_mode & S_IFMT) != S_IFDIR)
                        unlink(full_name);

                pcx_free(full_name);
        }

        closedir(dir);

        rmdir(name);
}

static void
free_test_data(struct test_data *data)
{
        if (data->chameleon)
                pcx_chameleon_game.free_game_cb(data->chameleon);

        if (data->config)
                pcx_config_free(data->config);

        test_message_data_destroy(&data->message_data);

        if (data->data_dir) {
                remove_data_dir(data->data_dir);
                pcx_free(data->data_dir);
        }

        pcx_free(data);
}

static bool
create_word_list(struct test_data *data,
                 const char *word_list)
{
        bool ret = true;

        char *filename = pcx_strconcat(data->data_dir,
                                       "/chameleon-word-list-eo.txt",
                                       NULL);

        FILE *f = fopen(filename, "w");

        if (f == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        filename,
                        strerror(errno));
                ret = false;
        } else {
                fputs(word_list, f);
                fclose(f);
        }

        pcx_free(filename);

        return ret;
}

static bool
create_config(struct test_data *data)
{
        bool ret = true;

        char *config_filename = pcx_strconcat(data->data_dir,
                                              "/config.txt",
                                              NULL);

        FILE *f = fopen(config_filename, "w");

        if (f == NULL) {
                fprintf(stderr, "%s: %s\n", config_filename, strerror(errno));
                ret = false;
        } else {
                fprintf(f,
                        "[general]\n"
                        "data_dir = %s\n"
                        "[server]\n",
                        data->data_dir);
                fclose(f);

                struct pcx_error *error = NULL;

                data->config = pcx_config_load(config_filename, &error);

                if (data->config == NULL) {
                        fprintf(stderr,
                                "error loading config: %s\n",
                                error->message);
                        pcx_error_free(error);
                        ret = false;
                }
        }

        pcx_free(config_filename);

        return ret;
}

static struct test_data *
create_test_data(const char *word_list)
{
        struct test_data *data = pcx_calloc(sizeof *data);

        test_message_data_init(&data->message_data);

        data->data_dir = make_data_dir();

        if (data->data_dir == NULL)
                goto error;

        if (!create_word_list(data, word_list))
                goto error;

        if (!create_config(data))
                goto error;

        return data;

error:
        free_test_data(data);
        return NULL;
}

static struct test_message *
queue_global_message(struct test_data *data,
                     const char *message_text)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_GLOBAL);

        message->message = pcx_strdup(message_text);

        return message;
}

static struct test_message *
queue_private_message(struct test_data *data,
                      int destination,
                      const char *message_text)
{
        struct test_message *message =
                test_message_queue(&data->message_data,
                                   TEST_MESSAGE_TYPE_PRIVATE);

        message->destination = destination;
        message->message = pcx_strdup(message_text);

        return message;
}

static PCX_NULL_TERMINATED void
queue_sideband_word_list(struct test_data *data,
                         ...)
{
        va_list ap;

        va_start(ap, data);

        int data_num = 0;

        while (true) {
                const char *word = va_arg(ap, const char *);

                if (word == NULL)
                        break;

                struct test_message *message =
                        test_message_queue(&data->message_data,
                                           TEST_MESSAGE_TYPE_SIDEBAND_STRING);

                message->destination = data_num;
                message->message = pcx_strdup(word);

                data_num++;
        }

        va_end(ap);
}

static struct test_message *
queue_wait_round(struct test_data *data,
                 const char *text)
{
        struct test_message *message = queue_global_message(data, text);

        test_message_enable_check_buttons(message);

        test_message_add_button(message,
                                "start_round",
                                "Komenci sekvan raŭndon");

        return message;
}

static bool
start_game(struct test_data *data,
           int n_players)
{
        struct pcx_chameleon_debug_overrides overrides = {
                .rand_func = fake_random_number_generator,
        };

        data->chameleon = pcx_chameleon_new(data->config,
                                            &test_message_callbacks,
                                            &data->message_data,
                                            PCX_TEXT_LANGUAGE_ESPERANTO,
                                            n_players,
                                            test_message_player_names,
                                            &overrides);


        return test_message_run_queue(&data->message_data);
}

static bool
start_round(struct test_data *data)
{
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0, /* player_num */
                                                   "start_round");

        return test_message_run_queue(&data->message_data);
}

static bool
send_message(struct test_data *data,
             int player_num,
             const char *message)
{
        pcx_chameleon_game.handle_message_cb(data->chameleon,
                                             player_num,
                                             message);

        return test_message_run_queue(&data->message_data);
}

static void
queue_clue_question(struct test_data *data,
                    int player_num)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "<b>%s</b>, bonvolu tajpi vian indikon",
                                 test_message_player_names[player_num]);

        queue_global_message(data, (const char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static PCX_NULL_TERMINATED bool
send_clues(struct test_data *data,
           const char *clue,
           ...)
{
        bool ret = true;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf,
                                 "Nun vi devas debati pri kiun vi kredas "
                                 "esti la kameleono. Kiam vi estos pretaj vi "
                                 "povos voĉdoni.\n");

        va_list ap;

        va_start(ap, clue);

        int i = 0;

        while (true) {
                const char *next = va_arg(ap, const char *);

                pcx_buffer_append_printf(&buf,
                                         "\n"
                                         "<b>%s</b>: %s",
                                         test_message_player_names[i],
                                         clue);

                if (next) {
                        queue_clue_question(data, i + 1);
                } else {
                        queue_global_message(data, (const char *) buf.data);
                }

                if (!send_message(data, i, clue)) {
                        ret = false;
                        break;
                }

                if (next == NULL)
                        break;

                clue = next;
                i++;
        }

        va_end(ap);

        pcx_buffer_destroy(&buf);

        return ret;
}

static bool
send_vote(struct test_data *data,
          int voter,
          int votee)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf, "vote:%i", votee);

        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   voter,
                                                   (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return test_message_run_queue(&data->message_data);
}

static bool
send_simple_vote(struct test_data *data,
                 int voter,
                 int votee)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "%s voĉdonis",
                                 test_message_player_names[voter]);

        queue_global_message(data, (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return send_vote(data, voter, votee);
}

static bool
send_guess(struct test_data *data,
           int guesser,
           int guess)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf, "guess:%i", guess);

        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   guesser,
                                                   (const char *) buf.data);

        pcx_buffer_destroy(&buf);

        return test_message_run_queue(&data->message_data);
}

static void
zero_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        bool *timeout_hit = user_data;

        *timeout_hit = true;
}

static bool
check_idle(struct test_data *data)
{
        bool timeout_hit = false;

        struct pcx_main_context_source *timeout =
                pcx_main_context_add_timeout(NULL,
                                             0, /* milliseconds */
                                             zero_timeout_cb,
                                             &timeout_hit);

        pcx_main_context_poll(NULL);

        if (!timeout_hit)
                pcx_main_context_remove_source(timeout);

        return test_message_run_queue(&data->message_data);
}

static struct test_data *
start_basic_game(void)
{
        struct test_data *data = create_test_data(basic_word_list);

        if (data == NULL)
                return NULL;

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Colors</b>\n"
                             "\n"
                             "Red\n"
                             "Green\n"
                             "Blue\n"
                             "Pink");

        queue_sideband_word_list(data,
                                 "Colors",
                                 "Red",
                                 "Green",
                                 "Blue",
                                 "Pink",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>Red</b>");
        }

        queue_clue_question(data, 0);

        if (!start_game(data, 4)) {
                free_test_data(data);
                return NULL;
        }

        return data;
}

static bool
test_basic(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_wait_round(data,
                         "Ĉiu voĉdonis!\n"
                         "\n"
                         "<b>Alice</b>: Bob\n"
                         "<b>Bob</b>: Bob\n"
                         "<b>Charles</b>: Bob\n"
                         "<b>David</b>: Bob\n"
                         "\n"
                         "La elektita ludanto estas <b>Bob</b>.\n"
                         "\n"
                         "Vi fuŝe elektis normalan homon!\n"
                         "\n"
                         "<b>Alice</b> gajnas 2 poentojn kaj ĉiu alia "
                         "gajnas nenion.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 2\n"
                         "<b>Bob</b>: 0\n"
                         "<b>Charles</b>: 0\n"
                         "<b>David</b>: 0");

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Animals</b>\n"
                             "\n"
                             "Dog\n"
                             "Cat\n"
                             "Wolf\n"
                             "Elephant");

        queue_sideband_word_list(data,
                                 "Animals",
                                 "Dog",
                                 "Cat",
                                 "Wolf",
                                 "Elephant",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>Dog</b>");
        }

        queue_clue_question(data, 0);

        if (!start_round(data)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "cute",
                        "friend",
                        "hunter",
                        "bark",
                        NULL)) {
                ret = false;
                goto out;
        }

        if (!send_simple_vote(data, 0, 0) ||
            !send_simple_vote(data, 1, 0) ||
            !send_simple_vote(data, 2, 0) ||
            /* try changing the vote */
            !send_simple_vote(data, 2, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Alice\n"
                             "<b>Bob</b>: Alice\n"
                             "<b>Charles</b>: Bob\n"
                             "<b>David</b>: Bob\n"
                             "\n"
                             "Estas egala rezulto! <b>Alice</b> havas la "
                             "decidan voĉdonon. "
                             "La elektita ludanto estas <b>Alice</b>.\n"
                             "\n"
                             "Vi sukcese trovis la kameleonon! 🦎\n"
                             "\n"
                             "<b>Alice</b>, nun provu diveni la sekretan "
                             "vorton.");

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

        queue_wait_round(data,
                         "La kameleono divenis <b>Dog</b>.\n"
                         "\n"
                         "Tio estis la ĝusta vorto!\n"
                         "\n"
                         "<b>Alice</b> gajnas unu poenton kaj ĉiu "
                         "alia gajnas nenion.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 3\n"
                         "<b>Bob</b>: 0\n"
                         "<b>Charles</b>: 0\n"
                         "<b>David</b>: 0");


        if (!send_guess(data, 0, 0)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Fruit</b>\n"
                             "\n"
                             "Apple\n"
                             "Pear\n"
                             "Banana\n"
                             "Orange");

        queue_sideband_word_list(data,
                                 "Fruit",
                                 "Apple",
                                 "Pear",
                                 "Banana",
                                 "Orange",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>Apple</b>");
        }

        queue_clue_question(data, 0);

        if (!start_round(data)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "clockwork",
                        "computer",
                        "steve",
                        "jobs",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Bob\n"
                             "<b>Bob</b>: Bob\n"
                             "<b>Charles</b>: Bob\n"
                             "<b>David</b>: Bob\n"
                             "\n"
                             "La elektita ludanto estas <b>Bob</b>.\n"
                             "\n"
                             "Vi fuŝe elektis normalan homon!\n"
                             "\n"
                             "<b>Alice</b> gajnas 2 poentojn kaj ĉiu alia "
                             "gajnas nenion.\n"
                             "\n"
                             "Poentoj:\n"
                             "\n"
                             "<b>Alice</b>: 5\n"
                             "<b>Bob</b>: 0\n"
                             "<b>Charles</b>: 0\n"
                             "<b>David</b>: 0");

        queue_global_message(data, "🏆 Alice gajnis la partion!");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_empty_word_list(void)
{
        struct test_data *data = create_test_data("");

        if (data == NULL)
                return false;

        bool ret = true;

        queue_global_message(data, "🏆 Alice gajnis la partion!");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!start_game(data, 4)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_one_group(void)
{
        struct test_data *data = create_test_data("Famous people\n"
                                                  "Ada Lovelace");

        if (data == NULL)
                return false;

        bool ret = true;

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Famous people</b>\n"
                             "\n"
                             "Ada Lovelace");

        queue_sideband_word_list(data,
                                 "Famous people",
                                 "Ada Lovelace",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: "
                                      "<b>Ada Lovelace</b>");
        }

        queue_clue_question(data, 0);

        if (!start_game(data, 4)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "programming",
                        "charles",
                        "cabbage",
                        "hair",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Bob\n"
                             "<b>Bob</b>: Bob\n"
                             "<b>Charles</b>: Bob\n"
                             "<b>David</b>: Bob\n"
                             "\n"
                             "La elektita ludanto estas <b>Bob</b>.\n"
                             "\n"
                             "Vi fuŝe elektis normalan homon!\n"
                             "\n"
                             "<b>Alice</b> gajnas 2 poentojn kaj ĉiu alia "
                             "gajnas nenion.\n"
                             "\n"
                             "Poentoj:\n"
                             "\n"
                             "<b>Alice</b>: 2\n"
                             "<b>Bob</b>: 0\n"
                             "<b>Charles</b>: 0\n"
                             "<b>David</b>: 0");

        queue_global_message(data, "🏆 Alice gajnis la partion!");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_wrong_guess(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Alice\n"
                             "<b>Bob</b>: Alice\n"
                             "<b>Charles</b>: Alice\n"
                             "<b>David</b>: Alice\n"
                             "\n"
                             "La elektita ludanto estas <b>Alice</b>.\n"
                             "\n"
                             "Vi sukcese trovis la kameleonon! 🦎\n"
                             "\n"
                             "<b>Alice</b>, nun provu diveni la sekretan "
                             "vorton.");

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

        queue_wait_round(data,
                         "La kameleono divenis <b>Green</b>.\n"
                         "\n"
                         "La ĝusta sekreta vorto estas <b>Red</b>.\n"
                         "\n"
                         "Ĉiu krom <b>Alice</b> gajnas 2 poentojn.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 0\n"
                         "<b>Bob</b>: 2\n"
                         "<b>Charles</b>: 2\n"
                         "<b>David</b>: 2");

        if (!send_guess(data, 0, 1)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_nonzero_dealer(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        /* Change the random number so that the chamelon will be
         * someone else
         */
        data->random_override = 19;

        queue_wait_round(data,
                         "Ĉiu voĉdonis!\n"
                         "\n"
                         "<b>Alice</b>: Bob\n"
                         "<b>Bob</b>: Bob\n"
                         "<b>Charles</b>: Bob\n"
                         "<b>David</b>: Bob\n"
                         "\n"
                         "La elektita ludanto estas <b>Bob</b>.\n"
                         "\n"
                         "Vi fuŝe elektis normalan homon!\n"
                         "\n"
                         "<b>Alice</b> gajnas 2 poentojn kaj ĉiu alia "
                         "gajnas nenion.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 2\n"
                         "<b>Bob</b>: 0\n"
                         "<b>Charles</b>: 0\n"
                         "<b>David</b>: 0");

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Animals</b>\n"
                             "\n"
                             "Dog\n"
                             "Cat\n"
                             "Wolf\n"
                             "Elephant");

        queue_sideband_word_list(data,
                                 "Animals",
                                 "Dog",
                                 "Cat",
                                 "Wolf",
                                 "Elephant",
                                 NULL);

        for (int i = 0; i < 3; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: "
                                      "<b>Elephant</b>");
        }

        queue_private_message(data,
                              3,
                              "Vi estas la kameleono 🦎");

        queue_clue_question(data, 0);

        if (!start_round(data)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "cute",
                        "friend",
                        "hunter",
                        "bark",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_wait_round(data,
                         "Ĉiu voĉdonis!\n"
                         "\n"
                         "<b>Alice</b>: Bob\n"
                         "<b>Bob</b>: Bob\n"
                         "<b>Charles</b>: Bob\n"
                         "<b>David</b>: Bob\n"
                         "\n"
                         "La elektita ludanto estas <b>Bob</b>.\n"
                         "\n"
                         "Vi fuŝe elektis normalan homon!\n"
                         "\n"
                         "<b>David</b> gajnas 2 poentojn kaj ĉiu alia "
                         "gajnas nenion.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 2\n"
                         "<b>Bob</b>: 0\n"
                         "<b>Charles</b>: 0\n"
                         "<b>David</b>: 2");

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Fruit</b>\n"
                             "\n"
                             "Apple\n"
                             "Pear\n"
                             "Banana\n"
                             "Orange");

        queue_sideband_word_list(data,
                                 "Fruit",
                                 "Apple",
                                 "Pear",
                                 "Banana",
                                 "Orange",
                                 NULL);

        for (int i = 0; i < 3; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>Orange</b>");
        }

        queue_private_message(data,
                              3,
                              "Vi estas la kameleono 🦎");

        /* David should now be the dealer and the first person that is
         * asked.
         */
        queue_clue_question(data, 3);

        if (!start_round(data)) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 0);

        if (!send_message(data, 3, "david-clue")) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 1);

        if (!send_message(data, 0, "alice-clue")) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 2);

        if (!send_message(data, 1, "bob-clue")) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Nun vi devas debati pri kiun vi kredas "
                             "esti la kameleono. Kiam vi estos pretaj vi "
                             "povos voĉdoni.\n"
                             "\n"
                             "<b>Alice</b>: alice-clue\n"
                             "<b>Bob</b>: bob-clue\n"
                             "<b>Charles</b>: charles-clue\n"
                             "<b>David</b>: david-clue");

        if (!send_message(data, 2, "charles-clue")) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_vote_message(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 1; i++) {
                /* Make time pass and make sure we don’t get a message */
                test_time_hack_add_time(i == 0 ? 59 : 50);

                if (!check_idle(data)) {
                        ret = false;
                        goto out;
                }

                /* Make enough extra time pass to get the message */
                test_time_hack_add_time(i == 0 ? 2 : 11);

                struct test_message *message =
                        queue_global_message(data,
                                             "Se vi jam finis la debaton, vi "
                                             "povas voĉdoni por la ludanto "
                                             "kiun vi suspektas esti la "
                                             "kameleono.");

                test_message_enable_check_buttons(message);
                test_message_add_button(message, "vote:0", "Alice");
                test_message_add_button(message, "vote:1", "Bob");
                test_message_add_button(message, "vote:2", "Charles");
                test_message_add_button(message, "vote:3", "David");

                if (!test_message_run_queue(&data->message_data)) {
                        ret = false;
                        goto out;
                }
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_invalid_vote(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        /* Its not time to vote. This should just be ignored */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "vote:0");

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        /* Invalid player number, should be ignored */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "vote:5");

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Alice\n"
                             "<b>Bob</b>: Alice\n"
                             "<b>Charles</b>: Alice\n"
                             "<b>David</b>: Alice\n"
                             "\n"
                             "La elektita ludanto estas <b>Alice</b>.\n"
                             "\n"
                             "Vi sukcese trovis la kameleonon! 🦎\n"
                             "\n"
                             "<b>Alice</b>, nun provu diveni la sekretan "
                             "vorton.");

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

        /* Everybody has already voted, this should be ignored */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "vote:0");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_invalid_guess(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        /* It’s not time to vote, this should be ignored */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:0");

        if (!send_clues(data,
                        "lemon",
                        "blood",
                        "tomato",
                        "china",
                        NULL)) {
                ret = false;
                goto out;
        }

        /* It’s not time to vote, this should be ignored */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:0");

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Alice\n"
                             "<b>Bob</b>: Alice\n"
                             "<b>Charles</b>: Alice\n"
                             "<b>David</b>: Alice\n"
                             "\n"
                             "La elektita ludanto estas <b>Alice</b>.\n"
                             "\n"
                             "Vi sukcese trovis la kameleonon! 🦎\n"
                             "\n"
                             "<b>Alice</b>, nun provu diveni la sekretan "
                             "vorton.");

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

        /* Wrong person voting */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   1,
                                                   "guess:0");
        /* Invalid word number */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:4");

        /* Missing colon */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess3");

        /* Garbage number */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:"
                                                   "18446744073709551616");

        /* Training data */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:3three");

        /* Negative number */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "guess:-1");

        /* Unknown command */
        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0,
                                                   "sgues:0");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_invalid_clues(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        /* Send clue for the wrong player */
        pcx_chameleon_game.handle_message_cb(data->chameleon,
                                             1,
                                             "potato");

        /* Clue with newlines in it */
        pcx_chameleon_game.handle_message_cb(data->chameleon,
                                             0,
                                             "Hi\nThere");

        /* Clue with only spaces in it */
        pcx_chameleon_game.handle_message_cb(data->chameleon,
                                             0,
                                             "  ");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 1);

        if (!send_message(data, 0, "   alice-clue    ")) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 2);

        if (!send_message(data, 1, "bob-clue")) {
                ret = false;
                goto out;
        }

        queue_clue_question(data, 3);

        if (!send_message(data, 2, "charles-clue")) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "Nun vi devas debati pri kiun vi kredas "
                             "esti la kameleono. Kiam vi estos pretaj vi "
                             "povos voĉdoni.\n"
                             "\n"
                             "<b>Alice</b>: alice-clue\n"
                             "<b>Bob</b>: bob-clue\n"
                             "<b>Charles</b>: charles-clue\n"
                             "<b>David</b>: david-clue");

        if (!send_message(data, 3, "david-clue")) {
                ret = false;
                goto out;
        }

        /* Not time for clues */
        pcx_chameleon_game.handle_message_cb(data->chameleon,
                                             0,
                                             "alice-clue");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_ignore_game_over(void)
{
        struct test_data *data = create_test_data("");

        if (data == NULL)
                return false;

        queue_global_message(data, "🏆 Alice gajnis la partion!");

        /* The word list is empty so this should queue a game over
         * message immediately.
         */
        data->chameleon = pcx_chameleon_new(data->config,
                                            &test_message_callbacks,
                                            &data->message_data,
                                            PCX_TEXT_LANGUAGE_ESPERANTO,
                                            4, /* n_players */
                                            test_message_player_names,
                                            NULL /* overrides */);

        /* Destroy the test data before letting the game over callback
         * be invoked.
         */
        free_test_data(data);

        bool timeout_hit = false;

        /* Run the main context. If the game over callback ends up
         * getting invoked this will probably cause a crash to fail
         * the test.
         */
        struct pcx_main_context_source *timeout =
                pcx_main_context_add_timeout(NULL,
                                             0, /* milliseconds */
                                             zero_timeout_cb,
                                             &timeout_hit);

        pcx_main_context_poll(NULL);

        if (!timeout_hit)
                pcx_main_context_remove_source(timeout);

        return true;
}

static bool
test_shorter_word_list(void)
{
        static const char word_list[] =
                "One letter\n"
                "A\n"
                "\n"
                "Three numbers\n"
                "One\n"
                "Two\n"
                "Three\n";

        struct test_data *data = create_test_data(word_list);

        if (data == NULL)
                return false;

        bool ret = true;

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Three numbers</b>\n"
                             "\n"
                             "One\n"
                             "Two\n"
                             "Three");

        queue_sideband_word_list(data,
                                 "Three numbers",
                                 "One",
                                 "Two",
                                 "Three",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");

        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>One</b>");
        }

        queue_clue_question(data, 0);

        if (!start_game(data, 4)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "Prime",
                        "6/6",
                        "0+1",
                        "Lonely",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 1)) {
                        ret = false;
                        goto out;
                }
        }

        queue_wait_round(data,
                         "Ĉiu voĉdonis!\n"
                         "\n"
                         "<b>Alice</b>: Bob\n"
                         "<b>Bob</b>: Bob\n"
                         "<b>Charles</b>: Bob\n"
                         "<b>David</b>: Bob\n"
                         "\n"
                         "La elektita ludanto estas <b>Bob</b>.\n"
                         "\n"
                         "Vi fuŝe elektis normalan homon!\n"
                         "\n"
                         "<b>Alice</b> gajnas 2 poentojn kaj ĉiu alia "
                         "gajnas nenion.\n"
                         "\n"
                         "Poentoj:\n"
                         "\n"
                         "<b>Alice</b>: 2\n"
                         "<b>Bob</b>: 0\n"
                         "<b>Charles</b>: 0\n"
                         "<b>David</b>: 0");

        if (!send_vote(data, 3, 1)) {
                ret = false;
                goto out;
        }

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>One letter</b>\n"
                             "\n"
                             "A");

        queue_sideband_word_list(data,
                                 "One letter",
                                 "A",
                                 "",
                                 "",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: <b>A</b>");
        }

        queue_clue_question(data, 0);

        if (!start_round(data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);
        return ret;
}

static bool
test_game_over_after_guess(void)
{
        struct test_data *data = create_test_data("Creators of Esperanto\n"
                                                  "Zamenhof\n");

        if (data == NULL)
                return false;

        bool ret = true;

        queue_global_message(data,
                             "La vortolisto estas:\n"
                             "\n"
                             "<b>Creators of Esperanto</b>\n"
                             "\n"
                             "Zamenhof");

        queue_sideband_word_list(data,
                                 "Creators of Esperanto",
                                 "Zamenhof",
                                 NULL);

        queue_private_message(data,
                              0,
                              "Vi estas la kameleono 🦎");
        for (int i = 1; i < 4; i++) {
                queue_private_message(data,
                                      i,
                                      "La sekreta vorto estas: "
                                      "<b>Zamenhof</b>");
        }

        queue_clue_question(data, 0);

        if (!start_game(data, 4)) {
                ret = false;
                goto out;
        }

        if (!send_clues(data,
                        "beard",
                        "cool",
                        "peaceful",
                        "polyglot",
                        NULL)) {
                ret = false;
                goto out;
        }

        for (int i = 0; i < 3; i++) {
                if (!send_simple_vote(data, i, 0)) {
                        ret = false;
                        goto out;
                }
        }

        queue_global_message(data,
                             "Ĉiu voĉdonis!\n"
                             "\n"
                             "<b>Alice</b>: Alice\n"
                             "<b>Bob</b>: Alice\n"
                             "<b>Charles</b>: Alice\n"
                             "<b>David</b>: Alice\n"
                             "\n"
                             "La elektita ludanto estas <b>Alice</b>.\n"
                             "\n"
                             "Vi sukcese trovis la kameleonon! 🦎\n"
                             "\n"
                             "<b>Alice</b>, nun provu diveni la sekretan "
                             "vorton.");

        if (!send_vote(data, 3, 0)) {
                ret = false;
                goto out;
        }

        struct test_message *message =
                queue_global_message(data,
                                     "La kameleono divenis <b>Zamenhof</b>.\n"
                                     "\n"
                                     "Tio estis la ĝusta vorto!\n"
                                     "\n"
                                     "<b>Alice</b> gajnas unu poenton kaj ĉiu "
                                     "alia gajnas nenion.\n"
                                     "\n"
                                     "Poentoj:\n"
                                     "\n"
                                     "<b>Alice</b>: 1\n"
                                     "<b>Bob</b>: 0\n"
                                     "<b>Charles</b>: 0\n"
                                     "<b>David</b>: 0");

        test_message_enable_check_buttons(message);

        queue_global_message(data, "🏆 Alice gajnis la partion!");

        test_message_queue(&data->message_data, TEST_MESSAGE_TYPE_GAME_OVER);

        if (!send_guess(data, 0, 0)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

static bool
test_invalid_start_round(void)
{
        struct test_data *data = start_basic_game();

        if (data == NULL)
                return false;

        bool ret = true;

        pcx_chameleon_game.handle_callback_data_cb(data->chameleon,
                                                   0, /* player_num */
                                                   "start_round");

        if (!check_idle(data)) {
                ret = false;
                goto out;
        }

out:
        free_test_data(data);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        pcx_log_start();

        if (!test_basic())
                ret = EXIT_FAILURE;

        if (!test_empty_word_list())
                ret = EXIT_FAILURE;

        if (!test_one_group())
                ret = EXIT_FAILURE;

        if (!test_wrong_guess())
                ret = EXIT_FAILURE;

        if (!test_nonzero_dealer())
                ret = EXIT_FAILURE;

        if (!test_vote_message())
                ret = EXIT_FAILURE;

        if (!test_invalid_vote())
                ret = EXIT_FAILURE;

        if (!test_invalid_guess())
                ret = EXIT_FAILURE;

        if (!test_invalid_clues())
                ret = EXIT_FAILURE;

        if (!test_ignore_game_over())
                ret = EXIT_FAILURE;

        if (!test_shorter_word_list())
                ret = EXIT_FAILURE;

        if (!test_game_over_after_guess())
                ret = EXIT_FAILURE;

        if (!test_invalid_start_round())
                ret = EXIT_FAILURE;

        pcx_log_close();

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
