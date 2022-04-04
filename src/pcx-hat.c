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

#include "pcx-hat.h"

#include "pcx-utf8.h"

uint32_t
pcx_hat_to_lower(uint32_t ch)
{
        if (ch >= 'A' && ch <= 'Z')
                return ch - 'A' + 'a';

        switch (ch) {
        case 0x0124: /* Ĥ */
        case 0x015c: /* Ŝ */
        case 0x011c: /* Ĝ */
        case 0x0108: /* Ĉ */
        case 0x0134: /* Ĵ */
        case 0x016c: /* Ŭ */
                return ch + 1;
        }

        return ch;
}

bool
pcx_hat_is_alphabetic(uint32_t ch)
{
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z'))
                return true;

        switch (ch) {
        case 0x0124: /* Ĥ */
        case 0x015c: /* Ŝ */
        case 0x011c: /* Ĝ */
        case 0x0108: /* Ĉ */
        case 0x0134: /* Ĵ */
        case 0x016c: /* Ŭ */
        case 0x0125: /* ĥ */
        case 0x015d: /* ŝ */
        case 0x011d: /* ĝ */
        case 0x0109: /* ĉ */
        case 0x0135: /* ĵ */
        case 0x016d: /* ŭ */
                return true;
        }

        return false;
}

bool
pcx_hat_is_alphabetic_string(const char *str)
{
        while (*str) {
                if (!pcx_hat_is_alphabetic(pcx_utf8_get_char(str)))
                        return false;
                str = pcx_utf8_next(str);
        }

        return true;
}

uint32_t
pcx_hat_iter_next(struct pcx_hat_iter *iter)
{
        uint32_t ch = pcx_utf8_get_char(iter->pos);
        const char *next = pcx_utf8_next(iter->pos);

        iter->length -= next - iter->pos;
        iter->pos = next;

        if (iter->length > 0 && (*iter->pos == 'x' || *iter->pos == 'X')) {
                switch (ch) {
                case 'h':
                        ch = 0x0125; /* ĥ */
                        break;
                case 's':
                        ch = 0x015d; /* ŝ */
                        break;
                case 'g':
                        ch = 0x011d; /* ĝ */
                        break;
                case 'c':
                        ch = 0x0109; /* ĉ */
                        break;
                case 'j':
                        ch = 0x0135; /* ĵ */
                        break;
                case 'u':
                        ch = 0x016d; /* ŭ */
                        break;
                case 'H':
                        ch = 0x0124; /* Ĥ */
                        break;
                case 'S':
                        ch = 0x015c; /* Ŝ */
                        break;
                case 'G':
                        ch = 0x011c; /* Ĝ */
                        break;
                case 'C':
                        ch = 0x0108; /* Ĉ */
                        break;
                case 'J':
                        ch = 0x0134; /* Ĵ */
                        break;
                case 'U':
                        ch = 0x016c; /* Ŭ */
                        break;
                default:
                        return ch;
                }

                iter->pos++;
                iter->length--;

                return ch;
        }

        return ch;
}
