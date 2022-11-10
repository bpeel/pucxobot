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
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "pcx-chameleon-list.h"
#include "pcx-util.h"
#include "pcx-file-error.h"

struct group_test {
        const char *source;
        const char *const *words;
};

static const struct group_test
group_tests[] = {
        {
                "food\n"
                "potato\n"
                "  courgette  \n"
                "lemon\n"
                "# comment\n"
                "invalid unicode\xC0:/\n"
                "  # comment with spaces\n"
                "\n"
                "\n"
                "# start a new group\n"
                "animals\n"
                "ĝirafo\n"
                "muso\n"
                "elefanto\n"
                "\n"
                "transport\n"
                "trajno\n"
                "buso\n"
                "granda helikoptero\n"
                "\n",
                (const char * const[]) {
                        "food",
                        "potato",
                        "courgette",
                        "lemon",
                        NULL,

                        "animals",
                        "ĝirafo",
                        "muso",
                        "elefanto",
                        NULL,

                        "transport",
                        "trajno",
                        "buso",
                        "granda helikoptero",
                        NULL,

                        NULL,
                },
        },

        {
                /* No trailing empty group */
                "bodily functions\n"
                "poop",
                (const char * const[]) {
                        "bodily functions",
                        "poop",
                        NULL,

                        NULL,
                },
        },

        {
                "long words\n"
                /* Word that is too long */
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                "a\n"
                "b",
                (const char * const[]) {
                        "long words",
                        "b",
                        NULL,

                        NULL,
                },
        },

        {
                "letters\n"
                /* Blank lines after the topic don’t start a new group */
                "\n"
                "a\n"
                "b\n",
                (const char * const[]) {
                        "letters",
                        "a",
                        "b",
                        NULL,

                        NULL,
                },
        },

        {
                "Names\n"
                "Alice\n"
                "Bob\n"
                "Charlie\n"
                "\n"
                /* Trailing group with no words. This will be ignored */
                "Empty\n",
                (const char * const[]) {
                        "Names",
                        "Alice",
                        "Bob",
                        "Charlie",
                        NULL,

                        NULL,
                },
        },
};

static struct pcx_chameleon_list *
load_string(const char *str,
            struct pcx_error **error)
{
        const char *tmp_dir = getenv("TMPDIR");

        if (tmp_dir == NULL)
                tmp_dir = "/tmp";

        char *filename = pcx_strconcat(tmp_dir, "/test-chameleon-XXXXXX", NULL);

        int fd = mkstemp(filename);

        assert(fd >= 0);

        FILE *f = fdopen(fd, "w");

        assert(f != NULL);

        fputs(str, f);

        fclose(f);

        struct pcx_chameleon_list *list = pcx_chameleon_list_new(filename,
                                                                 error);

        unlink(filename);
        pcx_free(filename);

        return list;
}

static bool
run_test(const struct group_test *test)
{
        struct pcx_error *error = NULL;
        struct pcx_chameleon_list *list = load_string(test->source, &error);

        if (list == NULL) {
                fprintf(stderr,
                        "unexpected error for test: %s\n"
                        "source:\n"
                        "%s\n",
                        error->message,
                        test->source);
                pcx_error_free(error);
                return false;
        }

        bool ret = true;

        size_t n_groups = pcx_chameleon_list_get_n_groups(list);

        const char *const *word_p = test->words;
        int group_num = 0;

        while (*word_p) {
                if (group_num >= n_groups) {
                        fprintf(stderr,
                                "not enough groups (%zu) for test:\n"
                                "%s\n",
                                n_groups,
                                test->source);
                        ret = false;
                        goto out;
                }

                const struct pcx_chameleon_list_group *group =
                        pcx_chameleon_list_get_group(list, group_num);

                if (strcmp(group->topic, *word_p)) {
                        fprintf(stderr,
                                "topic does not match\n"
                                " expected: %s\n"
                                " received: %s\n"
                                "source:\n"
                                "%s\n",
                                *word_p,
                                group->topic,
                                test->source);
                        ret = false;
                        goto out;
                }

                word_p++;

                const struct pcx_chameleon_list_word *word;

                pcx_list_for_each(word, &group->words, link) {
                        if (*word_p == NULL) {
                                fprintf(stderr,
                                        "too many words in group for:\n"
                                        "%s\n",
                                        test->source);
                                ret = false;
                                goto out;
                        }

                        if (strcmp(*word_p, word->word)) {
                                fprintf(stderr,
                                        "words do not match:\n"
                                        " expected: %s\n"
                                        " received: %s\n"
                                        "source:\n"
                                        "%s\n",
                                        *word_p,
                                        word->word,
                                        test->source);
                                ret = false;
                                goto out;
                        }

                        word_p++;
                }

                if (*word_p != NULL) {
                        fprintf(stderr,
                                "not enough words in group:\n"
                                "source:\n"
                                "%s\n",
                                test->source);
                        ret = false;
                        goto out;
                }

                word_p++;

                group_num++;
        }

        if (group_num < n_groups) {
                fprintf(stderr,
                        "too many groups:\n"
                        " expected: %i\n"
                        " received: %zu\n"
                        "source:\n"
                        "%s\n",
                        group_num,
                        n_groups,
                        test->source);
                ret = false;
                goto out;
        }

out:
        pcx_chameleon_list_free(list);

        return ret;
}

static bool
run_tests(void)
{
        bool ret = true;

        for (unsigned i = 0; i < PCX_N_ELEMENTS(group_tests); i++) {
                if (!run_test(group_tests + i))
                        ret = false;
        }

        return ret;
}

static bool
check_error_with_filename(const struct pcx_error *error,
                          struct pcx_error_domain *domain,
                          int code,
                          const char *message)
{
        if (error->domain != domain || error->code != code) {
                fprintf(stderr,
                        "unexpected error code in error message: %s\n",
                        error->message);
                return false;
        }

        /* Skip the filename */
        const char *colon = strchr(error->message, ':');

        if (colon == NULL) {
                fprintf(stderr,
                        "error message does not contain a colon: %s\n",
                        error->message);
                return false;
        }

        if (colon[1] != ' ') {
                fprintf(stderr,
                        "no space after colon in error message: %s\n",
                        error->message);
                return false;
        }

        if (strcmp(colon + 2, message)) {
                fprintf(stderr,
                        "error message does not match\n"
                        " expected: %s\n"
                        " received: %s\n",
                        message,
                        colon + 2);
                return false;
        }

        return true;
}

static bool
test_empty(const char *source)
{
        struct pcx_error *error = NULL;
        struct pcx_chameleon_list *list = load_string(source, &error);

        if (list != NULL) {
                fprintf(stderr,
                        "loading empty string did not report an error\n");
                pcx_chameleon_list_free(list);
                return false;
        }

        bool ret = check_error_with_filename(error,
                                             &pcx_chameleon_list_error,
                                             PCX_CHAMELEON_LIST_ERROR_EMPTY,
                                             "file contains no words");

        pcx_error_free(error);

        return ret;
}

static bool
test_no_file(void)
{
        struct pcx_error *error = NULL;
        struct pcx_chameleon_list *list =
                pcx_chameleon_list_new("this-file-does-not-exist", &error);

        if (list != NULL) {
                fprintf(stderr,
                        "loading a non-existant file did not report "
                        "an error\n");
                pcx_chameleon_list_free(list);
                return false;
        }

        bool ret = check_error_with_filename(error,
                                             &pcx_file_error,
                                             PCX_FILE_ERROR_NOENT,
                                             "No such file or directory");

        pcx_error_free(error);

        return ret;
}

int
main(int argc, char **argv)
{
        bool ret = true;

        if (!run_tests())
                ret = false;

        if (!test_empty(""))
                ret = false;

        if (!test_empty("only topic"))
                ret = false;

        if (!test_no_file())
                ret = false;

        return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
