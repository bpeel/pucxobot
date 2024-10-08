/*
 * Pucxobot - A bot and website to play some card games
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

#ifndef PCX_UTF8_H
#define PCX_UTF8_H

#include <stdint.h>
#include <stdbool.h>

#include "pcx-utf8.h"

#define PCX_UTF8_MAX_CHAR_LENGTH 4

uint32_t
pcx_utf8_get_char(const char *p);

const char *
pcx_utf8_next(const char *p);

bool
pcx_utf8_is_valid_string(const char *p);

int
pcx_utf8_encode(uint32_t ch, char *str);

#endif /* PCX_UTF8_H */
