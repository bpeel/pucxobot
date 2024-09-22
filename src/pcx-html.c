/*
 * Pucxobot - A bot and website to play some card games
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

#include "config.h"

#include "pcx-html.h"

void
pcx_html_escape_limit(struct pcx_buffer *buf,
                      const char *text,
                      int limit)
{
        static const struct {
                char ch;
                const char *rep;
        } replacements[] = {
                { '<', "&lt;" },
                { '>', "&gt;" },
                { '&', "&amp;" },
                { '"', "&quot;" },
        };

        const char *last = text;

        for (int i = 0; *text && (limit == -1 || i < limit); text++, i++) {
                for (unsigned i = 0; i < PCX_N_ELEMENTS(replacements); i++) {
                        if (replacements[i].ch == *text) {
                                pcx_buffer_append(buf, last, text - last);
                                last = text + 1;
                                pcx_buffer_append_string(buf,
                                                         replacements[i].rep);
                                break;
                        }
                }
        }

        pcx_buffer_append(buf, last, text - last);
        pcx_buffer_append_c(buf, '\0');
        buf->length--;
}

void
pcx_html_escape(struct pcx_buffer *buf,
                const char *text)
{
        pcx_html_escape_limit(buf, text, -1);
}
