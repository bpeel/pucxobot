/*
 * Pucxobot - A bot and website to play some card games
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

#ifndef PCX_TRIE_H
#define PCX_TRIE_H

#include <stdbool.h>

struct pcx_trie;

enum pcx_trie_add_result {
        PCX_TRIE_ADD_RESULT_NEW_WORD,
        PCX_TRIE_ADD_RESULT_ALREADY_ADDED,
};

struct pcx_trie *
pcx_trie_new(void);

enum pcx_trie_add_result
pcx_trie_add_word(struct pcx_trie *trie,
                  const char *word);

void
pcx_trie_free(struct pcx_trie *trie);

#endif /* PCX_TRIE_H */
