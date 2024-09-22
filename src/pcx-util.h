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

#ifndef PCX_UTIL_H
#define PCX_UTIL_H

#include <stddef.h>
#include <stdbool.h>

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

#define PCX_STRINGIFY(macro_or_string) PCX_STRINGIFY_ARG(macro_or_string)
#define PCX_STRINGIFY_ARG(contents) #contents

#define PCX_N_ELEMENTS(array) \
  (sizeof (array) / sizeof ((array)[0]))

#define PCX_SWAP_UINT16(x)                      \
  ((uint16_t)                                   \
   (((uint16_t) (x) >> 8) |                     \
    ((uint16_t) (x) << 8)))
#define PCX_SWAP_UINT32(x)                               \
  ((uint32_t)                                           \
   ((((uint32_t) (x) & UINT32_C (0x000000ff)) << 24) |  \
    (((uint32_t) (x) & UINT32_C (0x0000ff00)) << 8) |   \
    (((uint32_t) (x) & UINT32_C (0x00ff0000)) >> 8) |   \
    (((uint32_t) (x) & UINT32_C (0xff000000)) >> 24)))
#define PCX_SWAP_UINT64(x)                                               \
  ((uint64_t)                                                           \
   ((((uint64_t) (x) & (uint64_t) UINT64_C (0x00000000000000ff)) << 56) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x000000000000ff00)) << 40) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x0000000000ff0000)) << 24) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x00000000ff000000)) << 8) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x000000ff00000000)) >> 8) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x0000ff0000000000)) >> 24) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0x00ff000000000000)) >> 40) | \
    (((uint64_t) (x) & (uint64_t) UINT64_C (0xff00000000000000)) >> 56)))

#if defined(HAVE_BIG_ENDIAN)
#define PCX_UINT16_FROM_BE(x) ((uint16_t) (x))
#define PCX_UINT32_FROM_BE(x) ((uint32_t) (x))
#define PCX_UINT64_FROM_BE(x) ((uint64_t) (x))
#define PCX_UINT16_FROM_LE(x) PCX_SWAP_UINT16((uint16_t) (x))
#define PCX_UINT32_FROM_LE(x) PCX_SWAP_UINT32((uint32_t) (x))
#define PCX_UINT64_FROM_LE(x) PCX_SWAP_UINT64((uint64_t) (x))
#elif defined(HAVE_LITTLE_ENDIAN)
#define PCX_UINT16_FROM_LE(x) ((uint16_t) (x))
#define PCX_UINT32_FROM_LE(x) ((uint32_t) (x))
#define PCX_UINT64_FROM_LE(x) ((uint64_t) (x))
#define PCX_UINT16_FROM_BE(x) PCX_SWAP_UINT16((uint16_t) (x))
#define PCX_UINT32_FROM_BE(x) PCX_SWAP_UINT32((uint32_t) (x))
#define PCX_UINT64_FROM_BE(x) PCX_SWAP_UINT64((uint64_t) (x))
#else
#error Platform is neither little-endian nor big-endian
#endif

#define PCX_UINT16_TO_LE(x) PCX_UINT16_FROM_LE(x)
#define PCX_UINT16_TO_BE(x) PCX_UINT16_FROM_BE(x)
#define PCX_UINT32_TO_LE(x) PCX_UINT32_FROM_LE(x)
#define PCX_UINT32_TO_BE(x) PCX_UINT32_FROM_BE(x)
#define PCX_UINT64_TO_LE(x) PCX_UINT64_FROM_LE(x)
#define PCX_UINT64_TO_BE(x) PCX_UINT64_FROM_BE(x)

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

static inline char
pcx_ascii_tolower(char ch)
{
        if (ch >= 'A' && ch <= 'Z')
                return ch - 'A' + 'a';
        else
                return ch;
}

static inline bool
pcx_ascii_isdigit(char ch)
{
        return ch >= '0' && ch <= '9';
}

/* Returns true if the given strings are the same, ignoring case. The
 * case is compared ignoring the locale and operates on ASCII only.
 */
bool
pcx_ascii_string_case_equal(const char *a, const char *b);

#endif /* PCX_UTIL_H */
