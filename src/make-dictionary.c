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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdalign.h>
#include <assert.h>

#include "pcx-util.h"
#include "pcx-slab.h"
#include "pcx-utf8.h"
#include "pcx-buffer.h"

/* The trie on disk is stored as a list of trie nodes. A trie node is
 * stored as the following parts:
 *
 * • A byte offset to move to the next sibling, or zero if there is no
 *   next sibling.
 * • A byte offset to move to the first child, or zero if there are no
 *   children of this node.
 * • 1-6 bytes of UTF-8 encoded data to represent the character of
 *   this node.
 *
 * The two byte offsets are always positive and count from the point
 * between the offsets and the character data. The number is stored as
 * a variable-length integer. Each byte contains the next
 * most-significant 7 bits. The topmost bit of the byte determines
 * whether there are more bits to follow.
 *
 * The first entry in the list is the root node. Its character value
 * should be ignored.
 *
 * If the character is "\0" then it means the letters in the chain of
 * parents leading up to this node are a valid word.
 *
 * Duplicate nodes in the list are removed so the trie forms a
 * directed acyclic graph instead of a tree.
 */

struct trie_node {
        uint32_t ch;

        /* If we consider the trie to be a binary tree with the first
         * child as one branch and the next sibling as the other, then
         * this is the number of nodes in the tree starting from this
         * point. Calculated in a separate pass.
         */
        int size;

        /* The byte offset to the end of the file. Calculated in a
         * separate pass.
         */
        size_t offset;

        struct trie_node *first_child;
        struct trie_node *next_sibling;
};

struct trie_builder {
        struct pcx_slab_allocator node_allocator;

        struct trie_node *root;

        /* An array of pointers to all of the nodes so that we can
         * easily sort them.
         */
        struct pcx_buffer all_nodes;
};

struct stack_entry {
        struct trie_node *node;
        /* Next node to visit. 0=first child, 1=next sibling, 2=backtrack */
        int next_node;
};

/* Trie node info calculated on the fly */
struct trie_node_info {
        char encoded_char[PCX_UTF8_MAX_CHAR_LENGTH];
        size_t character_length;
        size_t child_offset;
        size_t sibling_offset;
};

static void
calculate_trie_node_info(const struct trie_node *node,
                         size_t next_offset,
                         struct trie_node_info *info)
{
        info->character_length =
                pcx_utf8_encode(node->ch, info->encoded_char);

        size_t character_offset = next_offset + info->character_length;

        if (node->first_child == NULL) {
                info->child_offset = 0;
        } else {
                info->child_offset = (character_offset -
                                      node->first_child->offset);
        }

        if (node->next_sibling == NULL) {
                info->sibling_offset = 0;
        } else {
                info->sibling_offset = (character_offset -
                                        node->next_sibling->offset);
        }
}

static struct trie_node *
alloc_trie_node(struct trie_builder *builder,
                uint32_t ch)
{
        struct trie_node *node = pcx_slab_allocate(&builder->node_allocator,
                                                   sizeof (struct trie_node),
                                                   alignof (struct trie_node));

        node->ch = ch;
        node->size = 1;
        node->first_child = NULL;
        node->next_sibling = NULL;

        pcx_buffer_append(&builder->all_nodes, &node, sizeof node);

        return node;
}

static void
init_trie_builder(struct trie_builder *builder)
{
        pcx_slab_init(&builder->node_allocator);
        pcx_buffer_init(&builder->all_nodes);

        builder->root = alloc_trie_node(builder, '*');
}

static void
destroy_trie_builder(struct trie_builder *builder)
{
        pcx_slab_destroy(&builder->node_allocator);
        pcx_buffer_destroy(&builder->all_nodes);
}

static struct stack_entry *
get_stack_top(struct pcx_buffer *buffer)
{
        return ((struct stack_entry *) (buffer->data + buffer->length)) - 1;
}

static struct stack_entry *
add_stack_entry(struct pcx_buffer *stack,
                struct trie_node *node)
{
        pcx_buffer_set_length(stack,
                              stack->length +
                              sizeof (struct stack_entry));

        struct stack_entry *entry = get_stack_top(stack);

        entry->node = node;
        entry->next_node = 0;

        return entry;
}

static void
stack_pop(struct pcx_buffer *stack)
{
        stack->length -= sizeof (struct stack_entry);
}

static struct trie_node *
next_node(struct stack_entry *entry)
{
        while (entry->next_node < 2) {
                struct trie_node *node = (entry->next_node == 0 ?
                                          entry->node->first_child :
                                          entry->node->next_sibling);

                entry->next_node++;

                if (node)
                        return node;
        }

        return NULL;
}

static void
calculate_size(struct trie_node *root)
{
        struct pcx_buffer stack = PCX_BUFFER_STATIC_INIT;

        add_stack_entry(&stack, root);

        while (true) {
                struct stack_entry *entry = get_stack_top(&stack);
                struct trie_node *child = next_node(entry);

                /* Have we finished with this node */
                if (child == NULL) {
                        stack_pop(&stack);

                        if (stack.length == 0)
                                break;

                        struct stack_entry *parent_entry =
                                get_stack_top(&stack);

                        parent_entry->node->size += entry->node->size;
                } else {
                        add_stack_entry(&stack, child);
                }
        }

        pcx_buffer_destroy(&stack);
}

static int
compare_node_character_cb(const void *pa, const void *pb)
{
        const struct trie_node *a = *(const struct trie_node **) pa;
        const struct trie_node *b = *(const struct trie_node **) pb;

        if (a->ch < b->ch)
                return -1;
        else if (a->ch > b->ch)
                return 1;
        else
                return 0;
}

static void
sort_pointer_buffer(struct pcx_buffer *buffer,
                    int (*compar)(const void *, const void *))
{
        qsort(buffer->data,
              buffer->length / sizeof (void *),
              sizeof (void *),
              compar);
}

static void
sort_children_by_character(struct trie_node *node,
                           struct pcx_buffer *child_pointers)
{
        child_pointers->length = 0;

        for (const struct trie_node *child = node->first_child;
             child;
             child = child->next_sibling) {
                pcx_buffer_append(child_pointers, &child, sizeof child);
        }

        /* Sort the pointers by character */
        sort_pointer_buffer(child_pointers, compare_node_character_cb);

        /* Put all the children back in the right order */

        node->first_child = NULL;

        size_t n_children = (child_pointers->length /
                             sizeof (struct trie_node *));
        struct trie_node **pointers =
                (struct trie_node **) child_pointers->data;

        for (int i = n_children - 1; i >= 0; i--) {
                pointers[i]->next_sibling = node->first_child;
                node->first_child = pointers[i];
        }
}

static void
sort_all_children(struct trie_builder *builder)
{
        struct pcx_buffer child_pointers = PCX_BUFFER_STATIC_INIT;

        int n_nodes = builder->all_nodes.length / sizeof (struct trie_node **);
        struct trie_node **nodes =
                (struct trie_node **) builder->all_nodes.data;

        for (int i = 0; i < n_nodes; i++)
                sort_children_by_character(nodes[i], &child_pointers);

        pcx_buffer_destroy(&child_pointers);
}

static void
add_word(struct trie_builder *builder,
         const char *word)
{
        struct trie_node *node = builder->root;

        for (const char *p = word; ; p = pcx_utf8_next(p)) {
                uint32_t ch = pcx_utf8_get_char(p);

                struct trie_node *child;

                for (child = node->first_child;
                     child;
                     child = child->next_sibling) {
                        if (child->ch == ch)
                                goto found;
                }

                child = alloc_trie_node(builder, ch);
                child->next_sibling = node->first_child;
                node->first_child = child;

        found:
                if (ch == '\0')
                        break;

                node = child;
        }
}

static bool
add_line(struct trie_builder *builder,
         int line_num,
         struct pcx_buffer *line)
{
        pcx_buffer_append_c(line, '\0');

        const char *word = (const char *) line->data;

        if (!pcx_utf8_is_valid_string(word)) {
                fprintf(stderr,
                        "line %i: invalid UTF-8 encountered\n",
                        line_num);
                return false;
        }

        add_word(builder, word);

        line->length = 0;

        return true;
}

static bool
add_lines(struct trie_builder *builder,
          FILE *fin)
{
        struct pcx_buffer line = PCX_BUFFER_STATIC_INIT;
        int line_num = 1;
        bool ret = true;

        while (true) {
                int ch = fgetc(fin);

                if (ch == EOF) {
                        if (ferror(fin)) {
                                fprintf(stderr,
                                        "%s\n",
                                        strerror(errno));
                                ret = false;
                        }

                        break;
                }

                if (ch == '\n') {
                        if (!add_line(builder, line_num, &line)) {
                                ret = false;
                                break;
                        }

                        line_num++;
                } else {
                        pcx_buffer_append_c(&line, ch);
                }
        }

        if (ret && line.length > 0 && !add_line(builder, line_num, &line))
                ret = false;

        pcx_buffer_destroy(&line);

        return ret;
}

static int
compare_node(const struct trie_node *a,
             const struct trie_node *b)
{
        if (a->size > b->size)
                return -1;
        else if (a->size < b->size)
                return 1;

        int ret = 0;

        struct pcx_buffer stack_a = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer stack_b = PCX_BUFFER_STATIC_INIT;

        add_stack_entry(&stack_a, (struct trie_node *) a);
        add_stack_entry(&stack_b, (struct trie_node *) b);

        while (stack_a.length > 0) {
                struct stack_entry *entry_a = get_stack_top(&stack_a);
                struct stack_entry *entry_b = get_stack_top(&stack_b);

                if (entry_a->node->ch != entry_b->node->ch) {
                        ret = (entry_a->node->ch <
                               entry_b->node->ch ?
                               -1 :
                               1);
                        break;
                }

                if (!!entry_a->node->first_child !=
                    !!entry_b->node->first_child) {
                        ret = entry_a->node->first_child ? 1 : -1;
                        break;
                }

                if (!!entry_a->node->next_sibling !=
                    !!entry_b->node->next_sibling) {
                        ret = entry_a->node->next_sibling ? 1 : -1;
                        break;
                }

                struct trie_node *child_a = entry_a->node->first_child;
                struct trie_node *child_b = entry_b->node->first_child;
                struct trie_node *sibling_a = entry_a->node->next_sibling;
                struct trie_node *sibling_b = entry_b->node->next_sibling;

                stack_pop(&stack_a);
                stack_pop(&stack_b);

                if (sibling_a) {
                        assert(sibling_b);
                        add_stack_entry(&stack_a, sibling_a);
                        add_stack_entry(&stack_b, sibling_b);
                } else {
                        assert(sibling_b == NULL);
                }

                if (child_a) {
                        assert(child_b);

                        add_stack_entry(&stack_a, child_a);
                        add_stack_entry(&stack_b, child_b);
                } else {
                        assert(child_b == NULL);
                }
        }

        pcx_buffer_destroy(&stack_a);
        pcx_buffer_destroy(&stack_b);

        return ret;
}

static int
compare_node_cb(const void *pa,
                const void *pb)
{
        const struct trie_node *a = *(const struct trie_node **) pa;
        const struct trie_node *b = *(const struct trie_node **) pb;

        return compare_node(a, b);
}

static size_t
n_bytes_for_size(size_t size)
{
        int n_bits;

        /* Count the number of bits needed to store this number */
        for (n_bits = sizeof (size) * 8;
             n_bits > 1 && (size & (((size_t) 1) << (n_bits - 1))) == 0;
             n_bits--);

        /* We can store 7 of the bits per byte */
        return (n_bits + 6) / 7;
}

static void
calculate_file_positions(struct trie_builder *builder)
{
        int n_nodes = builder->all_nodes.length / sizeof (struct trie_node **);
        struct trie_node **nodes =
                (struct trie_node **) builder->all_nodes.data;

        for (int i = n_nodes - 1; i >= 0; i--) {
                struct trie_node *node = nodes[i];
                size_t next_offset;

                /* If this node is the same as the next node then just
                 * reuse the same offset.
                 */
                if (i + 1 < n_nodes) {
                        struct trie_node *next_node = nodes[i + 1];

                        if (compare_node(node, next_node) == 0) {
                                node->offset = next_node->offset;
                                continue;
                        }

                        next_offset = next_node->offset;
                } else {
                        next_offset = 0;
                }

                struct trie_node_info info;

                calculate_trie_node_info(node, next_offset, &info);

                node->offset = (next_offset +
                                info.character_length +
                                n_bytes_for_size(info.child_offset) +
                                n_bytes_for_size(info.sibling_offset));
        }
}

static bool
write_offset(size_t offset, FILE *f)
{
        size_t n_bytes = n_bytes_for_size(offset);

        for (unsigned i = 0; i < n_bytes; i++) {
                uint8_t ch = offset & 0x7f;

                if (i + 1 < n_bytes)
                        ch |= 0x80;

                if (fputc(ch, f) == EOF) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        return false;
                }

                offset >>= 7;
        }

        return true;
}

static bool
write_node(const struct trie_node *node,
           size_t next_offset,
           FILE *f)
{
        struct trie_node_info info;

        calculate_trie_node_info(node, next_offset, &info);

        if (!write_offset(info.sibling_offset, f) ||
            !write_offset(info.child_offset, f))
                return false;

        if (fwrite(info.encoded_char, 1, info.character_length, f) !=
            info.character_length) {
                fprintf(stderr,
                        "%s\n",
                        strerror(errno));
                return false;
        }

        return true;
}

static bool
write_nodes(struct trie_builder *builder,
            const char *filename)
{
        FILE *f = fopen(filename, "wb");

        if (f == NULL) {
                fprintf(stderr, "%s: %s\n", filename, strerror(errno));
                return false;
        }

        bool ret = true;

        int n_nodes = builder->all_nodes.length / sizeof (struct trie_node **);
        const struct trie_node **nodes =
                (const struct trie_node **) builder->all_nodes.data;

        for (unsigned i = 0; i < n_nodes; i++) {
                const struct trie_node *node = nodes[i];

                /* If this node is the same as the next one then skip it */
                if (i + 1 < n_nodes && nodes[i + 1]->offset == node->offset)
                        continue;

                size_t next_offset;

                if (i + 1 < n_nodes)
                        next_offset = nodes[i + 1]->offset;
                else
                        next_offset = 0;

                if (!write_node(node, next_offset, f)) {
                        ret = false;
                        break;
                }
        }

        fclose(f);

        return ret;
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (argc != 2) {
                fprintf(stderr,
                        "usage: make-dictionary <output>\n");
                return EXIT_FAILURE;
        }

        struct trie_builder builder;

        init_trie_builder(&builder);

        if (!add_lines(&builder, stdin)) {
                ret = EXIT_FAILURE;
        } else {
                /* Sort all the children of each node by character so
                 * that it’s easier to compare them.
                 */
                sort_all_children(&builder);

                /* Calculate the size of each node in the trie as if
                 * it was a binary tree so that we can be sure to
                 * output the nodes closer to the root first.
                 */
                calculate_size(builder.root);

                /* Sort all the nodes by descending size and then by
                 * contents so that we can put the bigger nodes first
                 * and easily detect duplicates.
                 */
                sort_pointer_buffer(&builder.all_nodes, compare_node_cb);

                /* Calculate the position of each node in the final
                 * file and detect duplicates.
                 */
                calculate_file_positions(&builder);

                if (!write_nodes(&builder, argv[1]))
                        ret = EXIT_FAILURE;
        }

        destroy_trie_builder(&builder);

        return ret;
}
