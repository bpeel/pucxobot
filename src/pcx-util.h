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

#ifndef PCX_UTIL_H
#define PCX_UTIL_H

#include <stddef.h>

#ifdef __GNUC__
#define PCX_NO_RETURN __attribute__((noreturn))
#define PCX_PRINTF_FORMAT(string_index, first_to_check) \
  __attribute__((format(printf, string_index, first_to_check)))
#define PCX_NULL_TERMINATED __attribute__((sentinel))
#else
#define PCX_PRINTF_FORMAT(string_index, first_to_check)
#define PCX_NULL_TERMINATED
#ifdef _MSC_VER
#define PCX_NO_RETURN __declspec(noreturn)
#else
#define PCX_NO_RETURN
#endif
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PCX_N_ELEMENTS(array) \
  (sizeof (array) / sizeof ((array)[0]))

void *
pcx_alloc(size_t size);

void *
pcx_calloc(size_t size);

void *
pcx_realloc(void *ptr, size_t size);

void
pcx_free(void *ptr);

PCX_NULL_TERMINATED char *
pcx_strconcat(const char *string1, ...);

char *
pcx_strdup(const char *str);

char *
pcx_strndup(const char *str, size_t size);

void *
pcx_memdup(const void *data, size_t size);

PCX_NO_RETURN
PCX_PRINTF_FORMAT(1, 2)
void
pcx_fatal(const char *format, ...);

PCX_PRINTF_FORMAT(1, 2) void
pcx_warning(const char *format, ...);

int
pcx_close(int fd);

#endif /* PCX_UTIL_H */
