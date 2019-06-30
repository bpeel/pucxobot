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
#include <json_object.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-key-value.h"
#include "pcx-buffer.h"

struct pcx_error_domain
pcx_http_game_error;

struct pcx_http_game {
        struct pcx_game *game;

        struct pcx_main_context_source *timeout_source;

        struct pcx_list sockets;

        char *apikey;
        char *game_chat;
        char *botname;
        char *announce_channel;

        char *url_base;

        CURLM *curlm;
        CURL *updates_handle;
        struct curl_slist *updates_headers;
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

enum load_config_section {
        SECTION_NONE,
        SECTION_AUTH,
        SECTION_SETUP
} section;

struct load_config_data {
        const char *filename;
        struct pcx_http_game *game;
        bool had_error;
        struct pcx_buffer error_buffer;
        enum load_config_section section;
};

PCX_PRINTF_FORMAT(2, 3)
static void
load_config_error(struct load_config_data *data,
                  const char *format,
                  ...)
{
        data->had_error = true;

        if (data->error_buffer.length > 0)
                pcx_buffer_append_c(&data->error_buffer, '\n');

        pcx_buffer_append_printf(&data->error_buffer, "%s: ", data->filename);

        va_list ap;

        va_start(ap, format);
        pcx_buffer_append_vprintf(&data->error_buffer, format, ap);
        va_end(ap);
}

static void
load_config_error_func(const char *message,
                       void *user_data)
{
        struct load_config_data *data = user_data;
        load_config_error(data, "%s", message);
}

static void
load_config_func(enum pcx_key_value_event event,
                 int line_number,
                 const char *key,
                 const char *value,
                 void *user_data)
{
        struct load_config_data *data = user_data;
        static const struct {
                enum load_config_section section;
                const char *key;
                size_t offset;
        } options[] = {
#define OPTION(section, name)                                   \
                {                                               \
                        section,                                \
                        #name,                                  \
                        offsetof(struct pcx_http_game, name)    \
                }
                OPTION(SECTION_AUTH, apikey),
                OPTION(SECTION_SETUP, game_chat),
                OPTION(SECTION_SETUP, botname),
                OPTION(SECTION_SETUP, announce_channel),
#undef OPTION
        };

        switch (event) {
        case PCX_KEY_VALUE_EVENT_HEADER:
                if (!strcmp(value, "auth"))
                        data->section = SECTION_AUTH;
                else if (!strcmp(value, "setup"))
                        data->section = SECTION_SETUP;
                else
                        load_config_error(data, "unknown section: %s", value);
                break;
        case PCX_KEY_VALUE_EVENT_PROPERTY:
                for (unsigned i = 0; i < PCX_N_ELEMENTS(options); i++) {
                        if (data->section != options[i].section ||
                            strcmp(key, options[i].key))
                                continue;

                        char **ptr = (char **) ((uint8_t *) data->game +
                                                options[i].offset);
                        if (*ptr) {
                                load_config_error(data,
                                                  "%s specified twice",
                                                  key);
                        } else {
                                *ptr = pcx_strdup(value);
                        }
                        goto found_key;
                }

                load_config_error(data, "unknown config option: %s", key);
        found_key:
                break;
        }
}

static bool
validate_config(struct pcx_http_game *game,
                const char *filename,
                struct pcx_error **error)
{
        if (game->apikey == NULL) {
                pcx_set_error(error,
                              &pcx_http_game_error,
                              PCX_HTTP_GAME_ERROR_CONFIG,
                              "%s: missing apikey option",
                              filename);
                return false;
        }

        return true;
}

static bool
load_config(struct pcx_http_game *game,
            struct pcx_error **error)
{
        bool ret = true;
        const char *home = getenv("HOME");

        if (home == NULL) {
                pcx_set_error(error,
                              &pcx_http_game_error,
                              PCX_HTTP_GAME_ERROR_CONFIG,
                              "HOME environment variable is not set");
                return false;
        }

        char *fn = pcx_strconcat(home, "/.pucxobot/conf.txt", NULL);

        FILE *f = fopen(fn, "r");

        if (f == NULL) {
                pcx_set_error(error,
                              &pcx_http_game_error,
                              PCX_HTTP_GAME_ERROR_CONFIG,
                              "%s: %s",
                              fn,
                              strerror(errno));
                ret = false;
        } else {
                struct load_config_data data = {
                        .filename = fn,
                        .game = game,
                        .had_error = false,
                        .section = SECTION_NONE,
                };

                pcx_buffer_init(&data.error_buffer);

                pcx_key_value_load(f,
                                   load_config_func,
                                   load_config_error_func,
                                   &data);

                if (data.had_error) {
                        pcx_set_error(error,
                                      &pcx_http_game_error,
                                      PCX_HTTP_GAME_ERROR_CONFIG,
                                      "%s",
                                      (char *) data.error_buffer.data);
                        ret = false;
                } else if (!validate_config(game, fn, error)) {
                        ret = false;
                }

                pcx_buffer_destroy(&data.error_buffer);

                fclose(f);
        }

        pcx_free(fn);

        return ret;
}

static CURL *
create_easy_handle(struct pcx_http_game *game,
                   const char *method)
{
        CURL *e = curl_easy_init();

        char *url = pcx_strconcat(game->url_base, method, NULL);
        curl_easy_setopt(e, CURLOPT_URL, url);
        pcx_free(url);

        return e;
}

static void
set_updates_handle_post_data(struct pcx_http_game *game)
{
        struct json_object *au = json_object_new_array();
        json_object_array_add(au, json_object_new_string("message"));
        json_object_array_add(au, json_object_new_string("callback_query"));
        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "allowed_updates", au);
        json_object_object_add(obj, "timeout", json_object_new_int(300));

        curl_easy_setopt(game->updates_handle,
                         CURLOPT_COPYPOSTFIELDS,
                         json_object_to_json_string(obj));

        json_object_put(obj);
}

struct pcx_http_game *
pcx_http_game_new(struct pcx_error **error)
{
        struct pcx_http_game *game = pcx_calloc(sizeof *game);

        pcx_list_init(&game->sockets);

        curl_global_init(CURL_GLOBAL_ALL);

        if (!load_config(game, error))
                goto error;

        game->url_base = pcx_strconcat("https://api.telegram.org/bot",
                                       game->apikey,
                                       "/",
                                       NULL);

        game->curlm = curl_multi_init();

        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETFUNCTION, socket_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETDATA, game);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERFUNCTION, timer_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERDATA, game);

        game->updates_handle = create_easy_handle(game, "getUpdates");

        game->updates_headers =
                curl_slist_append(NULL,
                                  "Content-Type: "
                                  "application/json; charset=utf-8");

        curl_easy_setopt(game->updates_handle,
                         CURLOPT_HTTPHEADER,
                         game->updates_headers);

        set_updates_handle_post_data(game);

        curl_multi_add_handle(game->curlm, game->updates_handle);

        return game;

error:
        pcx_http_game_free(game);
        return NULL;
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

        if (game->updates_handle)
                curl_easy_cleanup(game->updates_handle);

        curl_slist_free_all(game->updates_headers);

        if (game->curlm)
                curl_multi_cleanup(game->curlm);

        curl_global_cleanup();

        remove_sockets(game);

        remove_timeout_source(game);

        pcx_free(game->apikey);
        pcx_free(game->game_chat);
        pcx_free(game->botname);
        pcx_free(game->announce_channel);

        pcx_free(game->url_base);

        pcx_free(game);
}
