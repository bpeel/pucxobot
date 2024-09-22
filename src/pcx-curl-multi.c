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

#include "config.h"

#include "pcx-curl-multi.h"

#include <curl/curl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-list.h"

struct pcx_curl_multi {
        struct pcx_main_context_source *timeout_source;

        struct pcx_list sockets;
        struct pcx_list handles;

        CURLM *curlm;
};

struct socket_data {
        struct pcx_list link;
        struct pcx_main_context_source *source;
        curl_socket_t fd;
};

struct handle_data {
        struct pcx_list link;
        struct CURL *e;
        pcx_curl_multi_finished_cb finished_cb;
        void *user_data;
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
remove_handle(struct pcx_curl_multi *pcurl,
              struct handle_data *data)
{
        curl_multi_remove_handle(pcurl->curlm,
                                 data->e);
        pcx_list_remove(&data->link);
        pcx_free(data);
}

static void
remove_timeout_source(struct pcx_curl_multi *pcurl)
{
        if (pcurl->timeout_source == NULL)
                return;

        pcx_main_context_remove_source(pcurl->timeout_source);
        pcurl->timeout_source = NULL;
}

static struct handle_data *
find_handle(struct pcx_curl_multi *pcurl,
            CURL *e)
{
        struct handle_data *handle;

        pcx_list_for_each(handle, &pcurl->handles, link) {
                if (handle->e == e)
                        return handle;
        }

        return NULL;
}

static void
flush_messages(struct pcx_curl_multi *pcurl)
{
        CURLMsg *msg;
        int msgs_in_queue;

        while ((msg = curl_multi_info_read(pcurl->curlm, &msgs_in_queue))) {
                if (msg->msg != CURLMSG_DONE)
                        continue;

                struct handle_data *handle =
                        find_handle(pcurl, msg->easy_handle);

                if (handle && handle->finished_cb) {
                        handle->finished_cb(msg->data.result,
                                            handle->user_data);
                }
        }
}

static void
socket_action_cb(struct pcx_main_context_source *source,
                 int fd,
                 enum pcx_main_context_poll_flags flags,
                 void *user_data)
{
        struct pcx_curl_multi *pcurl = user_data;

        int ev_bitmask = 0;

        if ((flags & PCX_MAIN_CONTEXT_POLL_IN))
                ev_bitmask |= CURL_CSELECT_IN;
        if ((flags & PCX_MAIN_CONTEXT_POLL_OUT))
                ev_bitmask |= CURL_CSELECT_OUT;
        if ((flags & PCX_MAIN_CONTEXT_POLL_ERROR))
                ev_bitmask |= CURL_CSELECT_ERR;

        int running_handles;

        curl_multi_socket_action(pcurl->curlm,
                                 fd,
                                 ev_bitmask,
                                 &running_handles);

        flush_messages(pcurl);
}

static int
socket_cb(CURL *easy,
          curl_socket_t s,
          int what,
          void *userp,
          void *socketp)
{
        struct pcx_curl_multi *pcurl = userp;
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
                pcx_list_insert(&pcurl->sockets, &sock->link);
                curl_multi_assign(pcurl->curlm, s, sock);
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
                                                         pcurl);
        }

        return CURLM_OK;
}

static void
timeout_cb(struct pcx_main_context_source *source,
           void *user_data)
{
        struct pcx_curl_multi *pcurl = user_data;

        pcurl->timeout_source = NULL;

        int running_handles;

        curl_multi_socket_action(pcurl->curlm,
                                 CURL_SOCKET_TIMEOUT,
                                 0, /* ev_bitmask */
                                 &running_handles);

        flush_messages(pcurl);
}

static int
timer_cb(CURLM *multi,
         long timeout_ms,
         void *userp)
{
        struct pcx_curl_multi *pcurl = userp;

        remove_timeout_source(pcurl);

        if (timeout_ms >= 0) {
                pcurl->timeout_source =
                        pcx_main_context_add_timeout(NULL,
                                                     timeout_ms,
                                                     timeout_cb,
                                                     pcurl);
        }

        return CURLM_OK;
}

struct pcx_curl_multi *
pcx_curl_multi_new(void)
{
        struct pcx_curl_multi *pcurl = pcx_calloc(sizeof *pcurl);

        pcx_list_init(&pcurl->sockets);
        pcx_list_init(&pcurl->handles);

        pcurl->curlm = curl_multi_init();

        curl_multi_setopt(pcurl->curlm, CURLMOPT_SOCKETFUNCTION, socket_cb);
        curl_multi_setopt(pcurl->curlm, CURLMOPT_SOCKETDATA, pcurl);
        curl_multi_setopt(pcurl->curlm, CURLMOPT_TIMERFUNCTION, timer_cb);
        curl_multi_setopt(pcurl->curlm, CURLMOPT_TIMERDATA, pcurl);


        return pcurl;
}

void
pcx_curl_multi_add_handle(struct pcx_curl_multi *pcurl,
                          CURL *e,
                          pcx_curl_multi_finished_cb finished_cb,
                          void *user_data)
{
        struct handle_data *handle = pcx_alloc(sizeof *handle);

        handle->e = e;
        handle->finished_cb = finished_cb;
        handle->user_data = user_data;

        pcx_list_insert(&pcurl->handles, &handle->link);

        curl_multi_add_handle(pcurl->curlm, e);
}

void
pcx_curl_multi_remove_handle(struct pcx_curl_multi *pcurl,
                             CURL *e)
{
        struct handle_data *handle = find_handle(pcurl, e);

        assert(handle);
        remove_handle(pcurl, handle);
}

static void
remove_handles(struct pcx_curl_multi *pcurl)
{
        struct handle_data *handle, *tmp;

        pcx_list_for_each_safe(handle, tmp, &pcurl->handles, link) {
                remove_handle(pcurl, handle);
        }
}

static void
remove_sockets(struct pcx_curl_multi *pcurl)
{
        struct socket_data *sock, *tmp;

        pcx_list_for_each_safe(sock, tmp, &pcurl->sockets, link) {
                remove_socket(sock);
        }
}

void
pcx_curl_multi_free(struct pcx_curl_multi *pcurl)
{
        if (pcurl->curlm)
                curl_multi_cleanup(pcurl->curlm);

        remove_timeout_source(pcurl);
        remove_sockets(pcurl);
        remove_handles(pcurl);

        pcx_free(pcurl);
}
