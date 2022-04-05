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

#include "pcx-syllabary.h"

#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "pcx-file-error.h"
#include "pcx-utf8.h"

/* The number of bytes reserved for the syllable text */
#define SYLLABLE_TEXT_SIZE 16
/* The total number of bytes for each entry */
#define ENTRY_SIZE (SYLLABLE_TEXT_SIZE + sizeof (uint32_t))

struct pcx_syllabary {
        int fd;
        size_t size;
        uint32_t total_hit_count;
        uint8_t *data;
};

static uint32_t
get_entry_hit_count(struct pcx_syllabary *syllabary,
                    int entry_num)
{
        uint32_t cumulative_hit_count_le;

        memcpy(&cumulative_hit_count_le,
               syllabary->data + entry_num * ENTRY_SIZE,
               sizeof cumulative_hit_count_le);

        return PCX_UINT32_FROM_LE(cumulative_hit_count_le);
}

struct pcx_syllabary *
pcx_syllabary_new(const char *filename,
                  struct pcx_error **error)
{
        struct pcx_syllabary *syllabary = pcx_calloc(sizeof *syllabary);

        syllabary->data = MAP_FAILED;

        syllabary->fd = open(filename, O_RDONLY);

        if (syllabary->fd == -1)
                goto error;

        struct stat statbuf;

        if (fstat(syllabary->fd, &statbuf) == -1)
                goto error;

        syllabary->size = statbuf.st_size;

        syllabary->data = mmap(NULL, /* addr */
                               statbuf.st_size,
                               PROT_READ,
                               MAP_PRIVATE,
                               syllabary->fd,
                               0 /* offset */);

        if (syllabary->data == MAP_FAILED)
                goto error;

        /* We don’t need to keep the file open after mapping */
        pcx_close(syllabary->fd);
        syllabary->fd = -1;

        if (syllabary->size >= ENTRY_SIZE) {
                syllabary->total_hit_count =
                        get_entry_hit_count(syllabary,
                                            syllabary->size / ENTRY_SIZE - 1);
        }

        return syllabary;

error:
        pcx_file_error_set(error,
                           errno,
                           "%s: %s",
                           filename,
                           strerror(errno));
        pcx_syllabary_free(syllabary);
        return NULL;
}

bool
pcx_syllabary_get_random(struct pcx_syllabary *syllabary,
                         char syllable[PCX_SYLLABARY_MAX_SYLLABLE_LENGTH + 1],
                         int *difficulty)
{
        if (syllabary->total_hit_count <= 0)
                return false;

        int min = 0, max = syllabary->size / ENTRY_SIZE;
        uint32_t target = (rand() /
                           (double) RAND_MAX *
                           syllabary->total_hit_count);

        while (max > min) {
                int mid = (min + max) / 2;
                uint32_t range_end = get_entry_hit_count(syllabary, mid);
                uint32_t range_start =
                        (mid > 0 ?
                         get_entry_hit_count(syllabary, mid - 1) :
                         0);

                if (target < range_start) {
                        max = mid;
                } else if (target >= range_end) {
                        min = mid + 1;
                } else {
                        char *text = (char *) (syllabary->data +
                                               mid * ENTRY_SIZE +
                                               sizeof (uint32_t));

                        if (!memchr(text, 0, SYLLABLE_TEXT_SIZE) ||
                            !pcx_utf8_is_valid_string(text))
                                return false;

                        strcpy(syllable, text);
                        *difficulty = ((syllabary->total_hit_count -
                                        range_start) *
                                       PCX_SYLLABARY_MAX_DIFFICULTY /
                                       syllabary->total_hit_count);

                        return true;
                }
        }

        return false;
}

void
pcx_syllabary_free(struct pcx_syllabary *syllabary)
{
        if (syllabary->data != MAP_FAILED)
                munmap(syllabary->data, syllabary->size);

        if (syllabary->fd != -1)
                pcx_close(syllabary->fd);

        pcx_free(syllabary);
}
