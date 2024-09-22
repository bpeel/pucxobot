/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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

#ifndef PCX_CURL_MULTI_H
#define PCX_CURL_MULTI_H

#include <curl/curl.h>

struct pcx_curl_multi;

typedef void
(* pcx_curl_multi_finished_cb)(CURLcode result, void *user_data);

struct pcx_curl_multi *
pcx_curl_multi_new(void);

void
pcx_curl_multi_add_handle(struct pcx_curl_multi *pcurl,
                          CURL *e,
                          pcx_curl_multi_finished_cb finished_cb,
                          void *user_data);

void
pcx_curl_multi_remove_handle(struct pcx_curl_multi *pcurl,
                             CURL *e);

void
pcx_curl_multi_free(struct pcx_curl_multi *pcurl);

#endif /* PCX_CURL_MULTI_H */
