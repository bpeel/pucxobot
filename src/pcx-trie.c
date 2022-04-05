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

#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "pcx-file-error.h"
#include "pcx-utf8.h"
#include "pcx-buffer.h"

struct pcx_trie {
        int fd;
        size_t size;
        uint8_t *data;
};

static const uint8_t *
read_length(const uint8_t *data,
            size_t size,
            size_t *length_out)
{
        size_t length = 0;
        int byte_num = 0;

        while (true) {
                if (size <= 0)
                        return NULL;

                length |= (*data & 0x7f) << (byte_num * 7);

                size--;
                data++;
                byte_num++;

                if ((data[-1] & 0x80) == 0)
                        break;
        }

        if (length > size || length <= 0)
                return NULL;

        *length_out = length;

        return data;
}

bool
pcx_trie_contains_word(struct pcx_trie *trie,
                       const char *word,
                       uint32_t *token)
{
        const uint8_t *data = trie->data;
        size_t size = trie->size;

        while (true) {
                data = read_length(data, size, &size);

                if (data == NULL)
                        return false;

                /* Skip the letter */
                if (size < 1)
                        return false;

                size_t letter_length =
                        pcx_utf8_next((const char *) data) -
                        (const char *) data;
                if (letter_length > size)
                        return false;

                data += letter_length;
                size -= letter_length;

                const char *next_letter = pcx_utf8_next(word);

                const uint8_t *child = data;

                while (true) {
                        size_t child_length;

                        const uint8_t *child_letter =
                                read_length(child,
                                            data + size - child,
                                            &child_length);

                        if (child_letter == NULL)
                                return false;

                        if (child_length >= next_letter - word &&
                            !memcmp(child_letter, word, next_letter - word)) {
                                if (*word == '\0') {
                                        /* Use the offset of the
                                         * terminator as a unique ID
                                         * for this word.
                                         */
                                        *token = child_letter - trie->data;
                                        return true;
                                }

                                /* We’ve found the child, so descend into it */
                                size -= child - data;
                                data = child;
                                word = next_letter;
                                break;
                        }

                        /* Skip over this child */
                        child = child_letter + child_length;

                        if (child > data + size)
                                return false;
                }
        }
}

struct stack_entry {
        const uint8_t *child;
        const uint8_t *end;
        size_t word_length;
};

static struct stack_entry *
get_stack_top(struct pcx_buffer *stack)
{
        return (struct stack_entry *) (stack->data + stack->length) - 1;
}

static void
add_stack_entry(struct pcx_buffer *stack,
                const uint8_t *child,
                const uint8_t *end,
                size_t word_length)
{
        pcx_buffer_set_length(stack,
                              stack->length + sizeof (struct stack_entry));
        struct stack_entry *entry = get_stack_top(stack);

        entry->child = child;
        entry->end = end;
        entry->word_length = word_length;
}

static void
stack_pop(struct pcx_buffer *stack)
{
        stack->length -= sizeof (struct stack_entry);
}

void
pcx_trie_iterate(struct pcx_trie *trie,
                 pcx_trie_iterate_cb cb,
                 void *user_data)
{
        struct pcx_buffer stack = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer word = PCX_BUFFER_STATIC_INIT;

        add_stack_entry(&stack,
                        trie->data,
                        trie->data + trie->size,
                        0 /* word_length */);

        while (stack.length > 0) {
                struct stack_entry *entry = get_stack_top(&stack);

                size_t child_length;

                const uint8_t *letter_start =
                        read_length(entry->child,
                                    entry->end - entry->child,
                                    &child_length);

                if (letter_start == NULL || letter_start >= entry->end) {
                        stack_pop(&stack);
                        continue;
                }

                const uint8_t *children =
                        (const uint8_t *) pcx_utf8_next((const char *)
                                                        letter_start);

                if (children > entry->end) {
                        stack_pop(&stack);
                        continue;
                }

                pcx_buffer_set_length(&word, entry->word_length);
                pcx_buffer_append(&word, letter_start, children - letter_start);

                if (*letter_start == '\0' &&
                    pcx_utf8_is_valid_string((const char *) word.data)) {
                        cb(pcx_utf8_next((const char *) word.data),
                           user_data);
                }

                entry->child = letter_start + child_length;

                add_stack_entry(&stack, children, entry->child, word.length);
        }

        pcx_buffer_destroy(&word);
        pcx_buffer_destroy(&stack);
}

struct pcx_trie *
pcx_trie_new(const char *filename,
             struct pcx_error **error)
{
        struct pcx_trie *trie = pcx_calloc(sizeof *trie);

        trie->data = MAP_FAILED;

        trie->fd = open(filename, O_RDONLY);

        if (trie->fd == -1)
                goto error;

        struct stat statbuf;

        if (fstat(trie->fd, &statbuf) == -1)
                goto error;

        trie->size = statbuf.st_size;

        trie->data = mmap(NULL, /* addr */
                          trie->size,
                          PROT_READ,
                          MAP_PRIVATE,
                          trie->fd,
                          0 /* offset */);

        if (trie->data == MAP_FAILED)
                goto error;

        /* We don’t need to keep the file open after mapping */
        pcx_close(trie->fd);
        trie->fd = -1;

        return trie;

error:
        pcx_file_error_set(error,
                           errno,
                           "%s: %s",
                           filename,
                           strerror(errno));
        pcx_trie_free(trie);
        return NULL;
}

void
pcx_trie_free(struct pcx_trie *trie)
{
        if (trie->data != MAP_FAILED)
                munmap(trie->data, trie->size);

        if (trie->fd != -1)
                pcx_close(trie->fd);

        pcx_free(trie);
}
