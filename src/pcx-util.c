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

#include "pcx-util.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

void
pcx_fatal(const char *format, ...)
{
        va_list ap;

        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);

        fputc('\n', stderr);

        fflush(stderr);

        abort();
}

void
pcx_warning(const char *format, ...)
{
        va_list ap;

        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);

        fputc('\n', stderr);
}

void *
pcx_alloc(size_t size)
{
        void *result = malloc(size);

        if (result == NULL)
                pcx_fatal("Memory exhausted");

        return result;
}

void *
pcx_calloc(size_t size)
{
        void *result = pcx_alloc(size);

        memset(result, 0, size);

        return result;
}

void *
pcx_realloc(void *ptr, size_t size)
{
        if (ptr == NULL)
                return pcx_alloc(size);

        ptr = realloc(ptr, size);

        if (ptr == NULL)
                pcx_fatal("Memory exhausted");

        return ptr;
}

void *
pcx_memdup(const void *data, size_t size)
{
        void *ret;

        ret = pcx_alloc(size);
        memcpy(ret, data, size);

        return ret;
}

char *
pcx_strdup(const char *str)
{
        return pcx_memdup(str, strlen(str) + 1);
}

char *
pcx_strndup(const char *str, size_t size)
{
        const char *end = str;

        while (end - str < size && *end != '\0')
                end++;

        size = end - str;

        char *ret = pcx_alloc(size + 1);
        memcpy(ret, str, size);
        ret[size] = '\0';

        return ret;
}

char *
pcx_strconcat(const char *string1, ...)
{
        size_t string1_length;
        size_t total_length;
        size_t str_length;
        va_list ap, apcopy;
        const char *str;
        char *result, *p;

        if (string1 == NULL)
                return pcx_strdup("");

        total_length = string1_length = strlen(string1);

        va_start(ap, string1);
        va_copy(apcopy, ap);

        while ((str = va_arg(ap, const char *)))
                total_length += strlen(str);

        va_end(ap);

        result = pcx_alloc(total_length + 1);
        memcpy(result, string1, string1_length);
        p = result + string1_length;

        while ((str = va_arg(apcopy, const char *))) {
                str_length = strlen(str);
                memcpy(p, str, str_length);
                p += str_length;
        }
        *p = '\0';

        va_end(apcopy);

        return result;
}

void
pcx_free(void *ptr)
{
        if (ptr)
                free(ptr);
}

int
pcx_close(int fd)
{
        int ret;

        do {
                ret = close(fd);
        } while (ret == -1 && errno == EINTR);

        return ret;
}

bool
pcx_ascii_string_case_equal(const char *a, const char *b)
{
        while (true) {
                if (pcx_ascii_tolower(*a) != pcx_ascii_tolower(*b))
                        return false;
                if (*a == 0)
                        return true;
                a++;
                b++;
        }
}
