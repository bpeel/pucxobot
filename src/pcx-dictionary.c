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

#include "pcx-dictionary.h"

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

struct pcx_dictionary {
        int fd;
        size_t size;
        uint8_t *data;
};

struct node_info {
        size_t sibling_offset;
        size_t child_offset;
        size_t letter_pos;
        size_t letter_length;
};

static int
read_offset(const struct pcx_dictionary *dict,
            size_t file_offset,
            size_t *offset_out)
{
        size_t length = 0;
        int byte_num = 0;

        while (true) {
                if (file_offset >= dict->size)
                        return -1;

                uint8_t byte = dict->data[file_offset];

                length |= (byte & 0x7f) << (byte_num * 7);

                file_offset++;
                byte_num++;

                if ((byte & 0x80) == 0)
                        break;
        }

        *offset_out = length;

        return byte_num;
}

static bool
extract_node(struct pcx_dictionary *dict,
             size_t pos,
             struct node_info *info)
{
        int sibling_offset_length = read_offset(dict,
                                                pos,
                                                &info->sibling_offset);

        if (sibling_offset_length == -1)
                return false;

        pos += sibling_offset_length;

        int child_offset_length = read_offset(dict,
                                              pos,
                                              &info->child_offset);

        if (child_offset_length == -1)
                return false;

        pos += child_offset_length;

        info->letter_pos = pos;

        if (pos >= dict->size)
                return false;

        info->letter_length =
                pcx_utf8_next((const char *) dict->data + pos) -
                (const char *) dict->data -
                pos;

        if (pos + info->letter_length > dict->size)
                return false;

        return true;
}

bool
pcx_dictionary_contains_word(struct pcx_dictionary *dict,
                             const char *word)
{
        struct node_info root_info;

        /* Skip the root node */
        if (!extract_node(dict, 0, &root_info) ||
            root_info.child_offset == 0)
                return false;

        size_t pos = root_info.letter_pos + root_info.child_offset;

        while (true) {
                struct node_info info;

                if (!extract_node(dict, pos, &info))
                        return false;

                const char *next_letter = pcx_utf8_next(word);

                if (next_letter - word == info.letter_length &&
                    !memcmp(dict->data + info.letter_pos,
                            word,
                            info.letter_length)) {
                        if (*word == '\0')
                                return true;

                        if (info.child_offset == 0)
                                return false;

                        pos = info.letter_pos + info.child_offset;
                        word = pcx_utf8_next(word);
                } else {
                        if (info.sibling_offset == 0)
                                return false;

                        pos = info.letter_pos + info.sibling_offset;
                }
        }
}

struct stack_entry {
        size_t pos;
        size_t word_length;
};

static struct stack_entry *
get_stack_top(struct pcx_buffer *stack)
{
        return (struct stack_entry *) (stack->data + stack->length) - 1;
}

static void
add_stack_entry(struct pcx_buffer *stack,
                size_t pos,
                size_t word_length)
{
        pcx_buffer_set_length(stack,
                              stack->length + sizeof (struct stack_entry));
        struct stack_entry *entry = get_stack_top(stack);

        entry->pos = pos;
        entry->word_length = word_length;
}

static void
stack_pop(struct pcx_buffer *stack)
{
        stack->length -= sizeof (struct stack_entry);
}

void
pcx_dictionary_iterate(struct pcx_dictionary *dict,
                       pcx_dictionary_iterate_cb cb,
                       void *user_data)
{
        struct pcx_buffer stack = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer word = PCX_BUFFER_STATIC_INIT;

        add_stack_entry(&stack,
                        0, /* pos */
                        0 /* word_length */);

        while (stack.length > 0) {
                struct stack_entry entry = *get_stack_top(&stack);

                stack_pop(&stack);

                struct node_info info;

                if (!extract_node(dict, entry.pos, &info))
                        continue;

                if (info.sibling_offset) {
                        add_stack_entry(&stack,
                                        info.letter_pos + info.sibling_offset,
                                        entry.word_length);
                }

                pcx_buffer_set_length(&word, entry.word_length);
                pcx_buffer_append(&word,
                                  dict->data + info.letter_pos,
                                  info.letter_length);

                const char *word_str = (const char *) word.data;

                if (dict->data[info.letter_pos] == '\0' &&
                    pcx_utf8_is_valid_string(word_str)) {
                        cb(pcx_utf8_next(word_str), user_data);
                }

                if (info.child_offset) {
                        add_stack_entry(&stack,
                                        info.letter_pos + info.child_offset,
                                        word.length);
                }
        }

        pcx_buffer_destroy(&word);
        pcx_buffer_destroy(&stack);
}

struct pcx_dictionary *
pcx_dictionary_new(const char *filename,
                   struct pcx_error **error)
{
        struct pcx_dictionary *dict = pcx_calloc(sizeof *dict);

        dict->data = MAP_FAILED;

        dict->fd = open(filename, O_RDONLY);

        if (dict->fd == -1)
                goto error;

        struct stat statbuf;

        if (fstat(dict->fd, &statbuf) == -1)
                goto error;

        dict->size = statbuf.st_size;

        dict->data = mmap(NULL, /* addr */
                          dict->size,
                          PROT_READ,
                          MAP_PRIVATE,
                          dict->fd,
                          0 /* offset */);

        if (dict->data == MAP_FAILED)
                goto error;

        /* We don’t need to keep the file open after mapping */
        pcx_close(dict->fd);
        dict->fd = -1;

        return dict;

error:
        pcx_file_error_set(error,
                           errno,
                           "%s: %s",
                           filename,
                           strerror(errno));
        pcx_dictionary_free(dict);
        return NULL;
}

void
pcx_dictionary_free(struct pcx_dictionary *dict)
{
        if (dict->data != MAP_FAILED)
                munmap(dict->data, dict->size);

        if (dict->fd != -1)
                pcx_close(dict->fd);

        pcx_free(dict);
}
