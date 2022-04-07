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

#ifndef PCX_HAT_H
#define PCX_HAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct pcx_hat_iter {
        const char *pos;
        size_t length;
};

static inline void
pcx_hat_iter_init(struct pcx_hat_iter *iter,
                  const char *str,
                  size_t length)
{
        iter->pos = str;
        iter->length = length;
}

static inline bool
pcx_hat_iter_finished(const struct pcx_hat_iter *iter)
{
        return iter->length <= 0;
}

uint32_t
pcx_hat_iter_next(struct pcx_hat_iter *iter);

uint32_t
pcx_hat_to_lower(uint32_t ch);

uint32_t
pcx_hat_to_upper(uint32_t ch);

bool
pcx_hat_is_alphabetic(uint32_t ch);

bool
pcx_hat_is_alphabetic_string(const char *str);

#endif /* PCX_HAT_H */
