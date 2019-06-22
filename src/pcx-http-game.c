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

#include "pcx-http-game.h"

#include <curl/curl.h>
#include <assert.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"

struct pcx_http_game {
        struct pcx_game *game;

        struct pcx_main_context_source *timeout_source;

        struct pcx_list sockets;

        CURLM *curlm;
};

struct socket_data {
        struct pcx_list link;
        struct pcx_main_context_source *source;
        curl_socket_t fd;
};

static void
remove_socket(struct socket_data *sock)
{
        if (sock->source)
                pcx_main_context_remove_source(sock->source);
        pcx_list_remove(&sock->link);
        pcx_free(sock);
}

static void
remove_timeout_source(struct pcx_http_game *game)
{
        if (game->timeout_source == NULL)
                return;

        pcx_main_context_remove_source(game->timeout_source);
        game->timeout_source = NULL;
}

static void
socket_action_cb(struct pcx_main_context_source *source,
                 int fd,
                 enum pcx_main_context_poll_flags flags,
                 void *user_data)
{
        struct pcx_http_game *game = user_data;

        int ev_bitmask = 0;

        if ((flags & PCX_MAIN_CONTEXT_POLL_IN))
                ev_bitmask |= CURL_CSELECT_IN;
        if ((flags & PCX_MAIN_CONTEXT_POLL_OUT))
                ev_bitmask |= CURL_CSELECT_OUT;
        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR))
                ev_bitmask |= CURL_CSELECT_ERR;

        int running_handles;

        curl_multi_socket_action(game->curlm,
                                 fd,
                                 ev_bitmask,
                                 &running_handles);
}

static int
socket_cb(CURL *easy,
          curl_socket_t s,
          int what,
          void *userp,
          void *socketp)
{
        struct pcx_http_game *game = userp;
        struct socket_data *sock = socketp;

        if (sock) {
                if (what == CURL_POLL_REMOVE) {
                        remove_socket(sock);
                        return CURLM_OK;
                }

                assert(s == sock->fd);
        } else {
                sock = pcx_calloc(sizeof *sock);
                sock->fd = s;
                pcx_list_insert(&game->sockets, &sock->link);
                curl_multi_assign(game->curlm, s, sock);
        }

        enum pcx_main_context_poll_flags flags;

        switch (what) {
        case CURL_POLL_IN:
                flags = (PCX_MAIN_CONTEXT_POLL_IN |
                         PCX_MAIN_CONTEXT_POLL_ERROR);
                break;
        case CURL_POLL_OUT:
                flags = PCX_MAIN_CONTEXT_POLL_OUT;
                break;
        case CURL_POLL_INOUT:
                flags = (PCX_MAIN_CONTEXT_POLL_IN |
                         PCX_MAIN_CONTEXT_POLL_OUT |
                         PCX_MAIN_CONTEXT_POLL_ERROR);
                break;
        default:
                pcx_fatal("Unknown curl poll status");
        }

        if (sock->source) {
                pcx_main_context_modify_poll(sock->source, flags);
        } else {
                sock->source = pcx_main_context_add_poll(NULL,
                                                         s,
                                                         flags,
                                                         socket_action_cb,
                                                         game);
        }

        return CURLM_OK;
}

static void
timeout_cb(struct pcx_main_context_source *source,
           void *user_data)
{
        struct pcx_http_game *game = user_data;

        game->timeout_source = NULL;

        int running_handles;

        curl_multi_socket_action(game->curlm,
                                 CURL_SOCKET_TIMEOUT,
                                 0, /* ev_bitmask */
                                 &running_handles);
}

static int
timer_cb(CURLM *multi,
         long timeout_ms,
         void *userp)
{
        struct pcx_http_game *game = userp;

        remove_timeout_source(game);

        if (timeout_ms >= 0) {
                game->timeout_source =
                        pcx_main_context_add_timeout(NULL,
                                                     timeout_ms,
                                                     timeout_cb,
                                                     game);
        }

        return CURLM_OK;
}

struct pcx_http_game *
pcx_http_game_new(void)
{
        struct pcx_http_game *game = pcx_calloc(sizeof *game);

        pcx_list_init(&game->sockets);

        curl_global_init(CURL_GLOBAL_ALL);

        game->curlm = curl_multi_init();

        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETFUNCTION, socket_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETDATA, game);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERFUNCTION, timer_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERDATA, game);

        return game;
}

static void
remove_sockets(struct pcx_http_game *game)
{
        struct socket_data *sock, *tmp;

        pcx_list_for_each_safe(sock, tmp, &game->sockets, link) {
                remove_socket(sock);
        }
}

void
pcx_http_game_free(struct pcx_http_game *game)
{
        if (game->game)
                pcx_game_free(game->game);

        if (game->curlm)
                curl_multi_cleanup(game->curlm);

        curl_global_cleanup();

        remove_sockets(game);

        remove_timeout_source(game);

        pcx_free(game);
}
