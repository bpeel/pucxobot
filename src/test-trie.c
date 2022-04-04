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

#include "pcx-trie.h"

#include <stdlib.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        struct pcx_error *error = NULL;

        if (argc < 2) {
                fprintf(stderr, "usage: test-trie <dictionary> [word]...\n");
                return EXIT_FAILURE;
        }

        struct pcx_trie *trie = pcx_trie_new(argv[1], &error);

        if (trie == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                ret = EXIT_FAILURE;
        } else {
                for (int i = 2; i < argc; i++) {
                        printf("%s: %s\n",
                               argv[i],
                               pcx_trie_contains_word(trie, argv[i]) ?
                               "yes" :
                               "no");
                }

                pcx_trie_free(trie);
        }

        return ret;
}
