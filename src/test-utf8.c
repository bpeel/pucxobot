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

#include <stdarg.h>
#include <assert.h>
#include <string.h>

static void
check_sequence(const char *p,
               ...)
{
        const char *end = p + strlen(p);
        va_list ap;

        assert(pcx_utf8_is_valid_string(p));

        va_start(ap, p);

        while (true) {
                int ch = va_arg(ap, int);

                if (ch == -1) {
                        assert(p == end);
                        assert(*p == 0);
                        break;
                }

                assert(*p);
                assert(ch == pcx_utf8_get_char(p));
                p = pcx_utf8_next(p);
        }

        va_end(ap);
}

int
main(int argc, char **argv)
{
        /* Check each of the four possible lengths */
        check_sequence("a\x24g",
                       'a',
                       0x24,
                       'g',
                       -1);
        check_sequence("a\xc2\xa2g",
                       'a',
                       0xa2,
                       'g',
                       -1);
        check_sequence("a\xe0\xa4\xb9g",
                       'a',
                       0x939,
                       'g',
                       -1);
        check_sequence("a\xf0\x90\x8d\x88g",
                       'a',
                       0x10348,
                       'g',
                       -1);

        /* Check unterminated code sequences */
        assert(!pcx_utf8_is_valid_string("a\xc2g"));
        assert(!pcx_utf8_is_valid_string("a\xc2"));
        assert(!pcx_utf8_is_valid_string("a\xe0\xa4g"));
        assert(!pcx_utf8_is_valid_string("a\xe0\xa4"));
        assert(!pcx_utf8_is_valid_string("a\xf0\x90\x8dg"));
        assert(!pcx_utf8_is_valid_string("a\xf0\x90\x8d"));
        assert(!pcx_utf8_is_valid_string("a\xf0\x90gg"));
        assert(!pcx_utf8_is_valid_string("a\xf0ggg"));

        /* UTF-16 surrogate pairs */
        check_sequence("a\xed\x9f\xbfg",
                       'a',
                       0xd7ff,
                       'g',
                       -1);
        assert(!pcx_utf8_is_valid_string("a\xed\xa0\x80g"));
        check_sequence("a\xee\x80\x80g",
                       'a',
                       0xe000,
                       'g',
                       -1);
        assert(!pcx_utf8_is_valid_string("a\xed\xbf\xbf"));

        /* Overlong encodings */

        check_sequence("a\x7fg", 'a', 0x7f, 'g', -1);
        assert(!pcx_utf8_is_valid_string("a\xc1\xbfg"));

        check_sequence("a\xc2\x80g",
                       'a',
                       0x80,
                       'g',
                       -1);
        check_sequence("a\xdf\xbfg",
                       'a',
                       0x7ff,
                       'g',
                       -1);
        assert(!pcx_utf8_is_valid_string("a\xe0\x9f\xbfg"));

        check_sequence("a\xe0\xa0\x80g",
                       'a',
                       0x800,
                       'g',
                       -1);
        check_sequence("a\xef\xbf\xbfg",
                       'a',
                       0xffff,
                       'g',
                       -1);
        assert(!pcx_utf8_is_valid_string("a\xf0\x8f\xbf\xbfg"));

        check_sequence("a\xf0\x90\x80\x80g",
                       'a',
                       0x10000,
                       'g',
                       -1);

        /* Top of the allowed range for UTF-16 */
        check_sequence("a\xf4\x8f\xbf\xbfg",
                       'a',
                       0x10ffff,
                       'g',
                       -1);
        assert(!pcx_utf8_is_valid_string("a\xf4\x90\x80\x80g"));

        /* Sequence that would require 5 bytes */
        assert(!pcx_utf8_is_valid_string("a\xf8\x88\x80\x80\x80g"));
}
