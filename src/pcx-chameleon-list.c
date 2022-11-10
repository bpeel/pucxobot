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

#include "pcx-chameleon-list.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include "pcx-file-error.h"
#include "pcx-utf8.h"
#include "pcx-buffer.h"
#include "pcx-list.h"
#include "pcx-slab.h"
#include "pcx-slice.h"

struct pcx_error_domain
pcx_chameleon_list_error;

struct pcx_chameleon_list {
        struct pcx_slab_allocator slab;
        struct pcx_chameleon_list_group *groups;
        size_t n_groups;
};

struct word_group {
        struct pcx_list link;
        char *topic;
        struct pcx_list words;
};

struct read_list_data {
        struct pcx_chameleon_list *word_list;
        struct pcx_slice_allocator group_allocator;
        struct pcx_list groups;
        struct word_group *current_group;
};

#define MAX_WORD_LENGTH (PCX_SLAB_SIZE / 2)

static void
start_group(struct read_list_data *data,
            const char *topic,
            size_t topic_length)
{
        struct word_group *group = pcx_slice_alloc(&data->group_allocator);

        pcx_list_init(&group->words);
        pcx_list_insert(data->groups.prev, &group->link);

        group->topic = pcx_slab_allocate(&data->word_list->slab,
                                         topic_length + 1,
                                         1 /* alignment */);
        memcpy(group->topic, topic, topic_length);
        group->topic[topic_length] = '\0';

        data->current_group = group;
}

static void
add_word(struct read_list_data *data,
         const char *word_text,
         size_t word_length)
{
        struct pcx_chameleon_list_word *word =
                pcx_slab_allocate(&data->word_list->slab,
                                  offsetof(struct pcx_chameleon_list_word,
                                           word) +
                                  word_length +
                                  1,
                                  alignof(struct pcx_chameleon_list_word));

        memcpy(word->word, word_text, word_length);
        word->word[word_length] = '\0';

        pcx_list_insert(data->current_group->words.prev, &word->link);
}

static bool
is_space_char(char ch)
{
        return ch != 0 && strchr("\r\n \t", ch) != NULL;
}

static void
process_line(struct read_list_data *data,
             const char *line)
{
        if (!pcx_utf8_is_valid_string(line))
                return;

        while (is_space_char(*line))
                line++;

        size_t length = strlen(line);

        while (length > 0 && is_space_char(line[length - 1]))
                length--;

        if (length == 0) {
                /* Empty line. End the current group if its not empty */
                if (data->current_group &&
                    !pcx_list_empty(&data->current_group->words))
                        data->current_group = NULL;
        } else if (*line != '#' && length <= MAX_WORD_LENGTH) {
                if (data->current_group)
                        add_word(data, line, length);
                else
                        start_group(data, line, length);
        }
}

static size_t
process_lines(struct read_list_data *data, char *buf, size_t buf_size)
{
        char *start = buf;

        while (true) {
                char *end = memchr(start, '\n', buf + buf_size - start);

                if (end == NULL)
                        break;

                *end = '\0';

                process_line(data, start);

                start = end + 1;
        }

        return start - buf;
}

static void
convert_groups_to_array(struct read_list_data *data)
{
        struct pcx_chameleon_list *word_list = data->word_list;

        word_list->n_groups = pcx_list_length(&data->groups);
        word_list->groups = pcx_alloc(sizeof (struct pcx_chameleon_list_group) *
                                      MAX(1, word_list->n_groups));

        int i = 0;
        struct word_group *g;

        pcx_list_for_each(g, &data->groups, link) {
                struct pcx_chameleon_list_group *group =
                        word_list->groups + i++;

                group->topic = g->topic;

                pcx_list_init(&group->words);
                pcx_list_insert_list(&group->words, &g->words);
        }
}

static void
read_words(struct pcx_chameleon_list *word_list,
           FILE *f)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        struct read_list_data data = {
                .word_list = word_list,
        };

        pcx_slice_allocator_init(&data.group_allocator,
                                 sizeof (struct word_group),
                                 alignof(struct word_group));
        pcx_list_init(&data.groups);

        while (true) {
                pcx_buffer_ensure_size(&buf, buf.length + 1024);

                size_t got = fread(buf.data + buf.length,
                                   1,
                                   buf.size - buf.length,
                                   f);
                bool end = got < buf.size - buf.length;

                buf.length += got;

                size_t consumed = process_lines(&data,
                                                (char *) buf.data,
                                                buf.length);

                memmove(buf.data, buf.data + consumed, buf.length - consumed);
                buf.length -= consumed;

                if (end)
                        break;
        }

        if (buf.length > 0) {
                pcx_buffer_append_c(&buf, '\0');
                process_line(&data, (char *) buf.data);
        }

        /* If the last group was empty then remove it */
        if (data.current_group && pcx_list_empty(&data.current_group->words))
                pcx_list_remove(&data.current_group->link);

        pcx_buffer_destroy(&buf);

        convert_groups_to_array(&data);

        pcx_slice_allocator_destroy(&data.group_allocator);
}

struct pcx_chameleon_list *
pcx_chameleon_list_new(const char *filename,
                       struct pcx_error **error)
{
        struct pcx_chameleon_list *word_list = pcx_calloc(sizeof *word_list);

        pcx_slab_init(&word_list->slab);

        FILE *f = fopen(filename, "r");

        if (f == NULL) {
                pcx_file_error_set(error,
                                   errno,
                                   "%s: %s",
                                   filename,
                                   strerror(errno));
                pcx_chameleon_list_free(word_list);
                return NULL;
        }

        read_words(word_list, f);

        fclose(f);

        if (word_list->n_groups <= 0) {
                pcx_set_error(error,
                              &pcx_chameleon_list_error,
                              PCX_CHAMELEON_LIST_ERROR_EMPTY,
                              "%s: file contains no words",
                              filename);
                pcx_chameleon_list_free(word_list);
                return NULL;
        }

        return word_list;
}

size_t
pcx_chameleon_list_get_n_groups(struct pcx_chameleon_list *word_list)
{
        return word_list->n_groups;
}

const struct pcx_chameleon_list_group *
pcx_chameleon_list_get_group(struct pcx_chameleon_list *word_list,
                             int group)
{
        assert(group >= 0 && group < word_list->n_groups);
        return word_list->groups + group;
}

void
pcx_chameleon_list_free(struct pcx_chameleon_list *word_list)
{
        pcx_free(word_list->groups);

        pcx_slab_destroy(&word_list->slab);

        pcx_free(word_list);
}
