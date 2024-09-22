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

#ifndef PCX_BOT_H
#define PCX_BOT_H

#include "pcx-curl-multi.h"
#include "pcx-config.h"
#include "pcx-class-store.h"

struct pcx_bot;

struct pcx_bot *
pcx_bot_new(const struct pcx_config *config,
            const struct pcx_config_bot *bot_config,
            struct pcx_class_store *class_store,
            struct pcx_curl_multi *pcurl);

int
pcx_bot_get_n_running_games(struct pcx_bot *bot);

void
pcx_bot_free(struct pcx_bot *bot);

#endif /* PCX_BOT_H */
