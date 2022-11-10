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

#include "pcx-chameleon-list.h"

#include <string.h>
#include <errno.h>

#include "pcx-file-error.h"
#include "pcx-utf8.h"
#include "pcx-buffer.h"

struct pcx_chameleon_list *
pcx_chameleon_list_new(const char *filename,
                       struct pcx_error **error)
{
        struct pcx_chameleon_list *word_list = pcx_calloc(sizeof *word_list);

        return word_list;
}

void
pcx_chameleon_list_free(struct pcx_chameleon_list *word_list)
{
        pcx_free(word_list);
}
