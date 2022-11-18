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
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"

/* The number of bytes reserved for the syllable text */
#define SYLLABLE_TEXT_SIZE 16
/* The total number of bytes for each entry */
#define ENTRY_SIZE (SYLLABLE_TEXT_SIZE + sizeof (uint32_t))

static void
dump_syllables(FILE *f)
{
        uint8_t entry[ENTRY_SIZE];

        uint32_t last_cumulative_hit_count = 0;

        while (fread(entry, 1, sizeof entry, f) == sizeof entry) {
                uint32_t cumulative_hit_count_le;

                memcpy(&cumulative_hit_count_le,
                       entry,
                       sizeof cumulative_hit_count_le);

                uint32_t cumulative_hit_count =
                        PCX_UINT32_FROM_LE(cumulative_hit_count_le);
                uint32_t hit_count = (cumulative_hit_count -
                                      last_cumulative_hit_count);

                printf("%i (%i): %.*s\n",
                       cumulative_hit_count,
                       hit_count,
                       SYLLABLE_TEXT_SIZE,
                       (const char *) entry + sizeof cumulative_hit_count_le);

                last_cumulative_hit_count = cumulative_hit_count;
        }
}

int
main(int argc, char **argv)
{
        if (argc < 2) {
                fprintf(stderr,
                        "usage: dump-syllabary <syllables>\n");
                return EXIT_FAILURE;
        }

        const char *syllabary_filename = argv[1];

        FILE *f = fopen(syllabary_filename, "rb");

        if (f == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        syllabary_filename,
                        strerror(errno));
                return EXIT_FAILURE;
        }

        dump_syllables(f);

        return EXIT_SUCCESS;
}
