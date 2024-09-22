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

#include "config.h"

#include "pcx-trie.h"

#include <stdalign.h>
#include <stdint.h>

#include "pcx-slab.h"
#include "pcx-buffer.h"
#include "pcx-utf8.h"

struct pcx_trie_node {
        uint32_t ch;
        struct pcx_trie_node *next_sibling;
        struct pcx_trie_node *first_child;
};

struct pcx_trie {
        struct pcx_slab_allocator node_allocator;
        struct pcx_trie_node *root;
};

static struct pcx_trie_node *
allocate_node(struct pcx_trie *trie,
              uint32_t ch)
{
        struct pcx_trie_node *node =
                pcx_slab_allocate(&trie->node_allocator,
                                  sizeof (struct pcx_trie_node),
                                  alignof (struct pcx_trie_node));

        node->ch = ch;
        node->next_sibling = NULL;
        node->first_child = NULL;

        return node;
}

struct pcx_trie *
pcx_trie_new(void)
{
        struct pcx_trie *trie = pcx_alloc(sizeof *trie);

        pcx_slab_init(&trie->node_allocator);

        trie->root = NULL;

        return trie;
}

enum pcx_trie_add_result
pcx_trie_add_word(struct pcx_trie *trie,
                  const char *word)
{
        struct pcx_trie_node *node = trie->root;
        struct pcx_trie_node **parent_slot = &trie->root;

        while (true) {
                uint32_t ch = pcx_utf8_get_char(word);

                if (node == NULL) {
                        struct pcx_trie_node *child = allocate_node(trie, ch);
                        *parent_slot = child;
                        parent_slot = &child->first_child;

                        if (ch == '\0')
                                return PCX_TRIE_ADD_RESULT_NEW_WORD;

                        word = pcx_utf8_next(word);
                } else if (ch == node->ch) {
                        parent_slot = &node->first_child;
                        node = node->first_child;

                        if (ch == '\0')
                                return PCX_TRIE_ADD_RESULT_ALREADY_ADDED;

                        word = pcx_utf8_next(word);
                } else {
                        parent_slot = &node->next_sibling;
                        node = node->next_sibling;
                }
        }
}

void
pcx_trie_free(struct pcx_trie *trie)
{
        pcx_slab_destroy(&trie->node_allocator);
        pcx_free(trie);
}
