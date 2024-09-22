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

#include "pcx-trie.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "pcx-buffer.h"
#include "pcx-util.h"

#define MAX_LETTERS_IN_WORD 4
#define N_BITS_PER_LETTER 3

static const char * const
letters[] = {
        "a", "b", "c", "d", "🎩", "Ŝ", "É", "🔚",
};

_Static_assert(PCX_N_ELEMENTS(letters) == (1 << N_BITS_PER_LETTER),
               "There must be one letter string for each possible letter "
               "number");

static int
get_n_words(void)
{
        int n_words_for_length = 1 << N_BITS_PER_LETTER;
        int total_n_words = 0;

        for (int n_letters = 1; n_letters <= MAX_LETTERS_IN_WORD; n_letters++) {
                total_n_words += n_words_for_length;
                n_words_for_length <<= N_BITS_PER_LETTER;
        }

        return total_n_words;
}

static int
get_word_length(uint32_t word_num)
{
        int n_words_for_length = 1 << N_BITS_PER_LETTER;
        int total_n_words = 0;

        for (int n_letters = 1; n_letters <= MAX_LETTERS_IN_WORD; n_letters++) {
                total_n_words += n_words_for_length;

                if (word_num < total_n_words)
                        return n_letters;

                n_words_for_length <<= N_BITS_PER_LETTER;
        }

        assert(!"unknown word_num");
}

static void
get_word(struct pcx_buffer *buf,
         uint32_t word_num)
{
        /* Invert some bits so that the letters aren’t added in order */
        word_num ^= 5;

        pcx_buffer_set_length(buf, 0);

        int length = get_word_length(word_num);

        for (int i = 0; i < length; i++) {
                int letter_num = word_num & ((1 << N_BITS_PER_LETTER) - 1);
                pcx_buffer_append_string(buf, letters[letter_num]);
                word_num >>= N_BITS_PER_LETTER;
        }
}

static bool
check_all_added(struct pcx_trie *trie,
                uint32_t n_words,
                struct pcx_buffer *word_buf)
{
        for (uint32_t word_num = 0; word_num < n_words; word_num++) {
                get_word(word_buf, word_num);

                enum pcx_trie_add_result result =
                        pcx_trie_add_word(trie, (const char *) word_buf->data);

                if (result != PCX_TRIE_ADD_RESULT_ALREADY_ADDED) {
                        fprintf(stderr,
                                "When readding “%s” (%i) result is not "
                                "ALREADY_ADDED\n",
                                (const char *) word_buf->data,
                                word_num);
                        return false;
                }
        }

        return true;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        struct pcx_trie *trie = pcx_trie_new();

        struct pcx_buffer word_buf = PCX_BUFFER_STATIC_INIT;

        int n_words = get_n_words();

        for (uint32_t word_num = 0; word_num < n_words; word_num++) {
                if (!check_all_added(trie, word_num, &word_buf)) {
                        ret = EXIT_FAILURE;
                        break;
                }

                get_word(&word_buf, word_num);

                enum pcx_trie_add_result result =
                        pcx_trie_add_word(trie,
                                          (const char *) word_buf.data);

                if (result != PCX_TRIE_ADD_RESULT_NEW_WORD) {
                        fprintf(stderr,
                                "Added “%s” (%i) but result was not "
                                "PCX_TRIE_ADD_RESULT_NEW_WORD\n",
                                (const char *) word_buf.data,
                                word_num);
                        ret = EXIT_FAILURE;
                        break;
                }
        }

        pcx_buffer_destroy(&word_buf);
        pcx_trie_free(trie);

        return ret;
}
