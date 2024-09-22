/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pcx-dictionary.h"
#include "pcx-util.h"
#include "pcx-utf8.h"

#define MIN_SYLLABLE_LENGTH 2
#define MAX_SYLLABLE_LENGTH 3

#define N_LENGTHS (MAX_SYLLABLE_LENGTH - MIN_SYLLABLE_LENGTH + 1)

#define MIN_SYLLABLE_COUNT 500

static const uint32_t
letters[] = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'r', 's', 't', 'u', 'v', 'z', 0x0109, 0x011d, 0x0125, 0x0135,
        0x015d, 0x016d,
};

#define N_LETTERS 28

_Static_assert(PCX_N_ELEMENTS(letters) == N_LETTERS,
               "There must be exactly one entry in letters for each letter");

struct syllable {
        /* Number of words in the dictionary that contain this syllable */
        int hit_count;
        /* Number of letters in this syllable */
        int length;
        /* Indexes into the letters array for the letters of this syllable */
        uint8_t letters[MAX_SYLLABLE_LENGTH];
};

struct data {
        /* All of the syllables in a single array so that they can be
         * sorted later.
         */
        struct syllable *syllables;
        /* Pointers into the above array to get the start of the
         * syllables for each length.
         */
        struct syllable *lengths[N_LENGTHS];

        /* The total number of syllables */
        int n_syllables;

        struct pcx_dictionary *dictionary;
};

static int
find_letter(uint32_t ch)
{
        int min = 0, max = N_LETTERS;

        while (max > min) {
                int mid = (max + min) / 2;

                if (letters[mid] == ch)
                        return mid;
                else if (letters[mid] > ch)
                        max = mid;
                else
                        min = mid + 1;
        }

        return -1;
}

static void
hit_syllable(const char *word,
             struct syllable *syllables,
             int length)
{
        int syllable_num = 0;

        for (int i = 0; i < length; i++) {
                int letter_num = find_letter(pcx_utf8_get_char(word));

                /* This will happen if the string is too short and we
                 * hit the null teriminator.
                 */
                if (letter_num == -1)
                        return;

                syllable_num = (syllable_num * N_LETTERS) + letter_num;

                word = pcx_utf8_next(word);
        }

        syllables[syllable_num].hit_count++;
}

static void
word_cb(const char *word,
        void *user_data)
{
        struct data *data = user_data;

        while (*word) {
                for (int i = 0; i < N_LENGTHS; i++) {
                        hit_syllable(word,
                                     data->lengths[i],
                                     i + MIN_SYLLABLE_LENGTH);
                }

                word = pcx_utf8_next(word);
        }
}

static void
allocate_syllables(struct data *data)
{
        int total_n_syllables = 0;
        int n_syllables[N_LENGTHS];
        int count = 1;

        for (int i = 0; i < MIN_SYLLABLE_LENGTH - 1; i++)
                count *= N_LETTERS;

        for (int i = 0; i < N_LENGTHS; i++) {
                count *= N_LETTERS;

                n_syllables[i] = count;
                total_n_syllables += count;
        }

        data->syllables = pcx_alloc(sizeof (struct syllable) *
                                    total_n_syllables);
        data->n_syllables = total_n_syllables;

        struct syllable *s = data->syllables;

        for (int i = 0; i < N_LENGTHS; i++) {
                data->lengths[i] = s;
                s += n_syllables[i];
        }
}

static void
initialize_letters_for_length(struct syllable *syllables,
                              int length)
{
        uint8_t letters[MAX_SYLLABLE_LENGTH] = { 0, };

        while (true) {
                syllables->hit_count = 0;
                syllables->length = length;
                memcpy(syllables->letters, letters, sizeof letters);
                syllables++;

                for (int i = length - 1; i >= 0; i--) {
                        if (++letters[i] >= N_LETTERS)
                                letters[i] = 0;
                        else
                                goto found;
                }

                break;

        found:
                continue;
        }
}

static void
initialize_letters(struct data *data)
{
        for (int i = 0; i < N_LENGTHS; i++) {
                initialize_letters_for_length(data->lengths[i],
                                              i + MIN_SYLLABLE_LENGTH);
        }
}

static int
compare_syllable_count(const void *ap,
                       const void *bp)
{
        const struct syllable *a = ap;
        const struct syllable *b = bp;

        return a->hit_count - b->hit_count;
}

static int
find_first_syllable(struct data *data)
{
        for (int i = 0; i < data->n_syllables; i++) {
                if (data->syllables[i].hit_count >= MIN_SYLLABLE_COUNT)
                        return i;
        }

        return data->n_syllables;
}

static void
dump_syllable(const struct syllable *syllable,
              FILE *out)
{
        uint32_t hit_count_le = PCX_UINT32_TO_LE(syllable->hit_count);

        fwrite(&hit_count_le, sizeof hit_count_le, 1, out);

        int entry_length = sizeof hit_count_le;

        for (int i = 0; i < syllable->length; i++) {
                char ch[PCX_UTF8_MAX_CHAR_LENGTH + 1];

                int len = pcx_utf8_encode(letters[syllable->letters[i]], ch);

                fwrite(ch, len, 1, out);

                entry_length += len;
        }

        for (int i = entry_length; i < 20; i++)
                fputc('\0', out);
}

static bool
dump_syllables(const struct syllable *syllables,
               int n_syllables,
               const char *filename)
{
        FILE *out = fopen(filename, "wb");

        if (out == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        filename,
                        strerror(errno));
                return false;
        }

        for (int i = 0; i < n_syllables; i++)
                dump_syllable(syllables + i, out);

        fclose(out);

        return true;
}

static void
make_cumulative(struct syllable *syllables,
                int n_syllables)
{
        int cumulative_hit_count = 0;

        for (int i = 0; i < n_syllables; i++) {
                cumulative_hit_count += syllables[i].hit_count;
                syllables[i].hit_count = cumulative_hit_count;
        }
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        struct pcx_error *error = NULL;

        if (argc != 3) {
                fprintf(stderr,
                        "usage: make-syllables <dictionary> <output>\n");
                return EXIT_FAILURE;
        }

        struct pcx_dictionary *dictionary = pcx_dictionary_new(argv[1], &error);

        if (dictionary == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                ret = EXIT_FAILURE;
        } else {
                struct data data = {
                        .dictionary = dictionary,
                };

                allocate_syllables(&data);
                initialize_letters(&data);

                pcx_dictionary_iterate(dictionary, word_cb, &data);

                qsort(data.syllables,
                      data.n_syllables,
                      sizeof (struct syllable),
                      compare_syllable_count);

                int first_syllable = find_first_syllable(&data);

                make_cumulative(data.syllables + first_syllable,
                                data.n_syllables - first_syllable);

                if (!dump_syllables(data.syllables + first_syllable,
                                    data.n_syllables - first_syllable,
                                    argv[2]))
                        ret = EXIT_FAILURE;

                pcx_free(data.syllables);

                pcx_dictionary_free(dictionary);
        }

        return ret;
}
