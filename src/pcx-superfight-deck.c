/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2019  Neil Roberts
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

#include "pcx-superfight-deck.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-text.h"

/* If we can’t manage to find this many cards, instead of making the
 * game crash it will just add bogus cards.
 */
#define MIN_CARDS 3
#define SLICE_SIZE (1024 - sizeof (struct pcx_superfight_deck_slice *))

struct pcx_superfight_deck_slice {
        struct pcx_superfight_deck_slice *next;
        char buf[SLICE_SIZE];
};

struct pcx_superfight_deck {
        struct pcx_superfight_deck_slice *slices;
        int n_cards;
        int card_pos;
        char **cards;
};

struct load_data {
        struct pcx_superfight_deck_slice *slices;
        struct pcx_buffer cards;
        size_t slice_used;
};

static void
add_card(struct load_data *data,
         const char *name,
         size_t name_len)
{
        if (name_len >= SLICE_SIZE)
                name_len = SLICE_SIZE - 1;

        if (data->slice_used + name_len >= SLICE_SIZE) {
                struct pcx_superfight_deck_slice *slice =
                        pcx_alloc(sizeof *slice);
                slice->next = data->slices;
                data->slices = slice;
                data->slice_used = 0;
        }

        struct pcx_superfight_deck_slice *slice = data->slices;
        char *name_start = slice->buf + data->slice_used;

        memcpy(name_start, name, name_len);
        name_start[name_len] = '\0';
        pcx_buffer_append(&data->cards, &name_start, sizeof name_start);

        data->slice_used += name_len + 1;
}

static void
shuffle_deck(struct pcx_superfight_deck *deck)
{
        for (unsigned i = deck->n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                char *t = deck->cards[j];
                deck->cards[j] = deck->cards[i];
                deck->cards[i] = t;
        }
}

static bool
is_space_char(char ch)
{
        return strchr("\r\n \t", ch) != NULL;
}

static void
add_line(struct load_data *data, const char *start, const char *end)
{
        while (start < end && is_space_char(*start))
                start++;
        while (end > start && is_space_char(end[-1]))
                end--;

        if (start >= end || *start == '#')
                return;

        add_card(data, start, end - start);
}

static void
load_from_stream(struct load_data *data, FILE *in)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        while (true) {
                pcx_buffer_ensure_size(&buf, buf.length + 512);

                size_t got = fread(buf.data + buf.length,
                                   1,
                                   buf.size - buf.length,
                                   in);

                if (got <= 0)
                        break;

                buf.length += got;

                size_t processed = 0;

                while (true) {
                        char *end = memchr(buf.data + processed,
                                           '\n',
                                           buf.length - processed);

                        if (end == NULL)
                                break;

                        add_line(data, (char *) buf.data + processed, end);

                        processed = end + 1 - (char *) buf.data;
                }

                memmove(buf.data, buf.data + processed, buf.length - processed);
                buf.length -= processed;
        }

        add_line(data, (char *) buf.data, (char *) buf.data + buf.length);

        pcx_buffer_destroy(&buf);
}

static char *
get_full_filename(const struct pcx_config *config,
                  enum pcx_text_language language,
                  const char *filename)
{
        return pcx_strconcat(config->data_dir,
                             "/superfight-",
                             filename,
                             "-",
                             pcx_text_get(language,
                                          PCX_TEXT_STRING_LANGUAGE_CODE),
                             ".txt",
                             NULL);
}

struct pcx_superfight_deck *
pcx_superfight_deck_load(const struct pcx_config *config,
                         enum pcx_text_language language,
                         const char *filename)
{
        struct load_data data = {
                .slices = NULL,
                .cards = PCX_BUFFER_STATIC_INIT,
                .slice_used = SLICE_SIZE,
        };

        char *full_filename = get_full_filename(config, language, filename);

        FILE *in = fopen(full_filename, "r");

        if (in) {
                load_from_stream(&data, in);
                fclose(in);
        }

        pcx_free(full_filename);

        while (data.cards.length < MIN_CARDS * sizeof (char *)) {
                char name = 'A' + data.cards.length / sizeof (char *);
                add_card(&data, &name, 1);
        }

        struct pcx_superfight_deck *deck = pcx_alloc(sizeof *deck);

        deck->slices = data.slices;
        deck->cards = (char **) data.cards.data;
        deck->n_cards = data.cards.length / sizeof (char *);
        deck->card_pos = 0;

        shuffle_deck(deck);

        return deck;
}

const char *
pcx_superfight_deck_draw_card(struct pcx_superfight_deck *deck)
{
        if (deck->card_pos >= deck->n_cards) {
                shuffle_deck(deck);
                deck->card_pos = 0;
        }

        return deck->cards[deck->card_pos++];
}

void
pcx_superfight_deck_free(struct pcx_superfight_deck *deck)
{
        struct pcx_superfight_deck_slice *slice, *next;

        for (slice = deck->slices; slice; slice = next) {
                next = slice->next;
                pcx_free(slice);
        }

        pcx_free(deck->cards);
        pcx_free(deck);
}
