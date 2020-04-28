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

#include "config.h"

#include "pcx-text.h"

#include <string.h>

#include "pcx-text-esperanto.h"
#include "pcx-text-french.h"
#include "pcx-text-english.h"
#include "pcx-text-pt-br.h"

static const char **
languages[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] = pcx_text_esperanto,
        [PCX_TEXT_LANGUAGE_FRENCH] = pcx_text_french,
        [PCX_TEXT_LANGUAGE_ENGLISH] = pcx_text_english,
        [PCX_TEXT_LANGUAGE_PT_BR] = pcx_text_pt_br,
};

const char *
pcx_text_get(enum pcx_text_language lang,
             enum pcx_text_string string)
{
        return languages[lang][string];
}

bool
pcx_text_lookup_language(const char *code,
                         enum pcx_text_language *language)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(languages); i++) {
                if (!strcmp(code,
                            pcx_text_get(i, PCX_TEXT_STRING_LANGUAGE_CODE))) {
                        *language = i;
                        return true;
                }
        }

        return false;
}
