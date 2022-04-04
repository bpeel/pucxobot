/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2021  Neil Roberts
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

#include "pcx-utf8.h"

uint32_t
pcx_utf8_get_char(const char *p)
{
        char start = *p;

        if ((start & 0x80) == 0)
                return *p;

        int next_bytes = 1;
        int test_bit = 0x20;

        do {
                if ((start & test_bit) == 0)
                        break;

                next_bytes++;
                test_bit >>= 1;
        } while (test_bit > 0);

        uint32_t value = start & (test_bit - 1);

        while (next_bytes--)
                value = (value << 6) | (*(++p) & 0x3f);

        return value;
}

const char *
pcx_utf8_next(const char *p)
{
        char start = *p;

        if ((start & 0x80) == 0)
                return p + 1;
        else if ((start & 0xe0) == 0xc0)
                return p + 2;
        else if ((start & 0xf0) == 0xe0)
                return p + 3;
        else
                return p + 4;
}

bool
pcx_utf8_is_valid_string(const char *p)
{
        while (*p) {
                char start = *p;
                int following_bytes;
                int minimum;

                if ((start & 0x80) == 0) {
                        p++;
                        continue;
                } else if ((start & 0xe0) == 0xc0) {
                        minimum = 0x80;
                        following_bytes = 1;
                } else if ((start & 0xf0) == 0xe0) {
                        minimum = 0x800;
                        following_bytes = 2;
                } else if ((start & 0xf8) == 0xf0) {
                        minimum = 0x10000;
                        following_bytes = 3;
                } else {
                        return false;
                }

                for (int i = 0; i < following_bytes; i++) {
                        if ((p[i + 1] & 0xc0) != 0x80)
                                return false;
                }

                uint32_t ch = pcx_utf8_get_char(p);

                if (ch < minimum)
                        return false;

                /* No UTF-16 pairs */
                if (ch >= 0xd800 && ch <= 0xdfff)
                        return false;

                /* No code points that aren’t encodable in UTF-16 */
                if (ch > 0x10ffff)
                        return false;

                p += 1 + following_bytes;
        }

        return true;
}

int
pcx_utf8_encode(uint32_t ch, char *str)
{
        if (ch < 0x80) {
                *(str++) = ch;
                return 1;
        }

        if (ch < 0x800) {
                *(str++) = (ch >> 6) | 0xc0;
                *(str++) = (ch & 0x3f) | 0x80;
                return 2;
        }

        if (ch < 0x10000) {
                *(str++) = (ch >> 12) | 0xe0;
                *(str++) = ((ch >> 6) & 0x3f) | 0x80;
                *(str++) = (ch & 0x3f) | 0x80;
                return 3;
        }

        *(str++) = (ch >> 18) | 0xf0;
        *(str++) = ((ch >> 12) & 0x3f) | 0x80;
        *(str++) = ((ch >> 6) & 0x3f) | 0x80;
        *(str++) = (ch & 0x3f) | 0x80;

        return 4;
}
