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
#include <json_tokener.h>
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

struct player {
        int64_t id;
        char *name;
};

struct pcx_http_game {
        struct pcx_game *game;

        struct pcx_main_context_source *timeout_source;
        struct pcx_main_context_source *restart_updates_source;

        struct pcx_list sockets;

        char *apikey;
        int64_t game_chat;
        char *botname;
        char *announce_channel;

        char *url_base;

        CURLM *curlm;
        CURL *updates_handle;
        struct curl_slist *content_type_headers;

        struct json_tokener *tokener;

        int64_t last_update_id;

        struct pcx_list inflight_requests;
        struct pcx_list unused_requests;

        int n_players;
        struct player players[PCX_GAME_MAX_PLAYERS];
};

struct socket_data {
        struct pcx_list link;
        struct pcx_main_context_source *source;
        curl_socket_t fd;
};

struct request {
        struct pcx_list link;
        struct json_tokener *tokener;
        CURL *handle;
};

static void
set_updates_handle_options(struct pcx_http_game *game);

PCX_NULL_TERMINATED
static bool
get_fields(struct json_object *obj,
           ...)
{
        if (!json_object_is_type(obj, json_type_object))
                return false;

        bool ret = true;
        va_list ap;

        va_start(ap, obj);

        const char *key;

        while ((key = va_arg(ap, const char *))) {
                struct json_object *value;

                if (!json_object_object_get_ex(obj, key, &value)) {
                        ret = false;
                        break;
                }

                enum json_type type = va_arg(ap, enum json_type);

                if (!json_object_is_type(value, type)) {
                        ret = false;
                        break;
                }

                switch (type) {
                case json_type_boolean:
                        *va_arg(ap, bool *) = json_object_get_boolean(value);
                        break;
                case json_type_string:
                        *va_arg(ap, const char **) =
                                json_object_get_string(value);
                        break;
                case json_type_double:
                        *va_arg(ap, double *) = json_object_get_double(value);
                        break;
                case json_type_int:
                        *va_arg(ap, int64_t *) = json_object_get_int64(value);
                        break;
                case json_type_object:
                case json_type_array:
                        *va_arg(ap, struct json_object **) = value;
                        break;
                default:
                        pcx_fatal("Unexpected json type");
                }
        }

        va_end(ap);

        return ret;
}

static struct request *
get_unused_request(struct pcx_http_game *game)
{
        if (pcx_list_empty(&game->unused_requests)) {
                struct request *request = pcx_alloc(sizeof *request);

                request->handle = curl_easy_init();
                request->tokener = json_tokener_new();
                return request;
        } else {
                struct request *request =
                        pcx_container_of(game->unused_requests.next,
                                         struct request,
                                         link);
                pcx_list_remove(&request->link);
                return request;
        }
}

static void
set_post_json_data(struct pcx_http_game *game,
                   CURL *handle,
                   struct json_object *obj)
{
        curl_easy_setopt(handle,
                         CURLOPT_HTTPHEADER,
                         game->content_type_headers);
        curl_easy_setopt(handle,
                         CURLOPT_COPYPOSTFIELDS,
                         json_object_to_json_string(obj));
}

static void
set_easy_handle_method(struct pcx_http_game *game,
                       CURL *e,
                       const char *method)
{
        char *url = pcx_strconcat(game->url_base, method, NULL);
        curl_easy_setopt(e, CURLOPT_URL, url);
        pcx_free(url);
}

static size_t
request_write_cb(char *ptr,
                 size_t size,
                 size_t nmemb,
                 void *userdata)
{
        struct request *request = userdata;

        struct json_object *obj =
                json_tokener_parse_ex(request->tokener,
                                      ptr,
                                      size * nmemb);

        if (obj) {
                bool ok;
                bool ret = get_fields(obj,
                                      "ok", json_type_boolean, &ok,
                                      NULL);

                if (ret && !ok) {
                        const char *desc;
                        ret = get_fields(obj,
                                         "description", json_type_string, &desc,
                                         NULL);
                        if (ret)
                                fprintf(stderr, "%s\n", desc);
                }

                json_object_put(obj);
                return (ret && ok) ? size * nmemb : 0;
        }

        enum json_tokener_error error =
                json_tokener_get_error(request->tokener);

        if (error == json_tokener_continue)
                return size * nmemb;
        else
                return 0;
}

static void
send_request(struct pcx_http_game *game,
             const char *method,
             struct json_object *args)
{
        struct request *request = get_unused_request(game);

        set_easy_handle_method(game, request->handle, method);

        curl_easy_setopt(request->handle,
                         CURLOPT_WRITEFUNCTION,
                         request_write_cb);
        curl_easy_setopt(request->handle, CURLOPT_WRITEDATA, request);

        set_post_json_data(game, request->handle, args);

        pcx_list_insert(&game->inflight_requests, &request->link);

        curl_multi_add_handle(game->curlm, request->handle);
}

static void
send_message_with_buttons(struct pcx_http_game *game,
                          int64_t chat_id,
                          int64_t in_reply_to,
                          const char *message,
                          size_t n_buttons,
                          const struct pcx_game_button *buttons)
{
        struct json_object *args = json_object_new_object();

        json_object_object_add(args,
                               "chat_id",
                               json_object_new_int64(chat_id));

        json_object_object_add(args,
                               "text",
                               json_object_new_string(message));

        if (in_reply_to != -1) {
                json_object_object_add(args,
                                       "reply_to_message_id",
                                       json_object_new_int64(in_reply_to));
        }

        if (n_buttons > 0) {
                struct json_object *button_array = json_object_new_array();

                for (unsigned i = 0; i < n_buttons; i++) {
                        struct json_object *row = json_object_new_array();
                        struct json_object *button = json_object_new_object();

                        struct json_object *text =
                                json_object_new_string(buttons[i].text);
                        json_object_object_add(button, "text", text);

                        struct json_object *data =
                                json_object_new_string(buttons[i].data);
                        json_object_object_add(button, "callback_data", data);

                        json_object_array_add(row, button);
                        json_object_array_add(button_array, row);
                }

                struct json_object *reply_markup = json_object_new_object();
                json_object_object_add(reply_markup,
                                       "inline_keyboard",
                                       button_array);
                json_object_object_add(args, "reply_markup", reply_markup);
        }

        send_request(game, "sendMessage", args);

        json_object_put(args);
}

static void
send_message(struct pcx_http_game *game,
             int64_t chat_id,
             int64_t in_reply_to,
             const char *message)
{
        send_message_with_buttons(game,
                                  chat_id,
                                  in_reply_to,
                                  message,
                                  0, /* n_buttons */
                                  NULL /* buttons */);
}

static void
send_message_vprintf(struct pcx_http_game *game,
                     int64_t chat_id,
                     int64_t in_reply_to,
                     const char *format,
                     va_list ap)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_vprintf(&buf, format, ap);

        send_message(game,
                     chat_id,
                     in_reply_to,
                     (char *) buf.data);

        pcx_buffer_destroy(&buf);
}

PCX_PRINTF_FORMAT(4, 5)
static void
send_message_printf(struct pcx_http_game *game,
                    int64_t chat_id,
                    int64_t in_reply_to,
                    const char *format,
                    ...)
{
        va_list ap;
        va_start(ap, format);
        send_message_vprintf(game,
                             chat_id,
                             in_reply_to,
                             format,
                             ap);
        va_end(ap);
}

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
remove_restart_updates_source(struct pcx_http_game *game)
{
        if (game->restart_updates_source == NULL)
                return;

        pcx_main_context_remove_source(game->restart_updates_source);
        game->restart_updates_source = NULL;
}

static void
restart_updates_cb(struct pcx_main_context_source *source,
                   void *user_data)
{
        struct pcx_http_game *game = user_data;

        game->restart_updates_source = NULL;

        curl_multi_remove_handle(game->curlm,
                                 game->updates_handle);

        curl_easy_reset(game->updates_handle);
        set_updates_handle_options(game);

        json_tokener_reset(game->tokener);

        curl_multi_add_handle(game->curlm, game->updates_handle);
}

static void
restart_updates(struct pcx_http_game *game,
                CURLcode code)
{
        long timeout = 0;

        if (code != CURLE_OK) {
                fprintf(stderr,
                        "getUpdates failed: %s\n",
                        curl_easy_strerror(code));
                timeout = 60 * 1000;
        }

        remove_restart_updates_source(game);

        game->restart_updates_source =
                pcx_main_context_add_timeout(NULL,
                                             timeout,
                                             restart_updates_cb,
                                             game);
}

static void
remove_inflight_request(struct pcx_http_game *game,
                        CURL *handle,
                        CURLcode code)
{
        struct request *request;

        pcx_list_for_each(request, &game->inflight_requests, link) {
                if (handle != request->handle)
                        continue;

                if (code != CURLE_OK) {
                        fprintf(stderr,
                                "request failed: %s\n",
                                curl_easy_strerror(code));
                }

                curl_multi_remove_handle(game->curlm, handle);
                curl_easy_reset(handle);

                json_tokener_reset(request->tokener);

                pcx_list_remove(&request->link);
                pcx_list_insert(&game->unused_requests, &request->link);

                return;
        }

        pcx_fatal("unknown curl handle finished");
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

        CURLMsg *msg;
        int msgs_in_queue;

        while ((msg = curl_multi_info_read(game->curlm, &msgs_in_queue))) {
                if (msg->msg != CURLMSG_DONE)
                        continue;

                if (msg->easy_handle == game->updates_handle) {
                        restart_updates(game, msg->data.result);
                } else {
                        remove_inflight_request(game,
                                                msg->easy_handle,
                                                msg->data.result);
                }
        }
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

enum option_type {
        OPTION_TYPE_STRING,
        OPTION_TYPE_INT
};

static void
set_option(struct load_config_data *data,
           enum option_type type,
           size_t offset,
           const char *key,
           const char *value)
{
        switch (type) {
        case OPTION_TYPE_STRING: {
                char **ptr = (char **) ((uint8_t *) data->game + offset);
                if (*ptr) {
                        load_config_error(data,
                                          "%s specified twice",
                                          key);
                } else {
                        *ptr = pcx_strdup(value);
                }
                break;
        }
        case OPTION_TYPE_INT: {
                int64_t *ptr = (int64_t *) ((uint8_t *) data->game + offset);
                errno = 0;
                char *tail;
                *ptr = strtoll(value, &tail, 10);
                if (errno || *tail) {
                        load_config_error(data,
                                          "invalid value for %s",
                                          key);
                }
                break;
        }
        }
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
                enum option_type type;
        } options[] = {
#define OPTION(section, name, type)                             \
                {                                               \
                        section,                                \
                        #name,                                  \
                        offsetof(struct pcx_http_game, name),   \
                        OPTION_TYPE_ ## type,                   \
                }
                OPTION(SECTION_AUTH, apikey, STRING),
                OPTION(SECTION_SETUP, game_chat, INT),
                OPTION(SECTION_SETUP, botname, STRING),
                OPTION(SECTION_SETUP, announce_channel, STRING),
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

                        set_option(data,
                                   options[i].type,
                                   options[i].offset,
                                   key,
                                   value);
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

        if (game->botname == NULL) {
                pcx_set_error(error,
                              &pcx_http_game_error,
                              PCX_HTTP_GAME_ERROR_CONFIG,
                              "%s: missing botname option",
                              filename);
                return false;
        }

        if (game->game_chat == 0) {
                pcx_set_error(error,
                              &pcx_http_game_error,
                              PCX_HTTP_GAME_ERROR_CONFIG,
                              "%s: missing game_chat option",
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

static void
reset_game(struct pcx_http_game *game)
{
        if (game->game) {
                pcx_game_free(game->game);
                game->game = NULL;
        }

        for (unsigned i = 0; i < game->n_players; i++)
                pcx_free(game->players[i].name);

        game->n_players = 0;
}

static void
send_private_message_cb(int user_num,
                        enum pcx_game_message_format format,
                        const char *message,
                        size_t n_buttons,
                        const struct pcx_game_button *buttons,
                        void *user_data)
{
        struct pcx_http_game *game = user_data;

        assert(user_num >= 0 && user_num < game->n_players);

        send_message_with_buttons(game,
                                  game->players[user_num].id,
                                  -1, /* in_reply_to */
                                  message,
                                  n_buttons,
                                  buttons);
}

static void
send_message_cb(enum pcx_game_message_format format,
                const char *message,
                size_t n_buttons,
                const struct pcx_game_button *buttons,
                void *user_data)
{
        struct pcx_http_game *game = user_data;

        send_message_with_buttons(game,
                                  game->game_chat,
                                  -1, /* in_reply_to */
                                  message,
                                  n_buttons,
                                  buttons);
}

static void
game_over_cb(void *user_data)
{
        struct pcx_http_game *game = user_data;
        reset_game(game);
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_private_message = send_private_message_cb,
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
answer_callback(struct pcx_http_game *game,
                const char *id)
{
        struct json_object *args = json_object_new_object();

        json_object_object_add(args,
                               "callback_query_id",
                               json_object_new_string(id));

        send_request(game, "answerCallbackQuery", args);

        json_object_put(args);
}

static bool
process_callback(struct pcx_http_game *game,
                 struct json_object *callback)
{
        const char *id;
        const char *callback_data;
        struct json_object *from;

        bool ret = get_fields(callback,
                              "id", json_type_string, &id,
                              "data", json_type_string, &callback_data,
                              "from", json_type_object, &from,
                              NULL);

        if (!ret)
                return false;

        int64_t from_id;

        ret = get_fields(from,
                         "id", json_type_int, &from_id,
                         NULL);

        if (!ret)
                return false;

        answer_callback(game, id);

        if (!game->game)
                return true;

        for (unsigned i = 0; i < game->n_players; i++) {
                if (game->players[i].id == from_id) {
                        pcx_game_handle_callback_data(game->game,
                                                      i,
                                                      callback_data);
                        break;
                }
        }

        return true;
}

struct message_info {
        const char *text;
        int64_t from_id;
        int64_t chat_id;
        int64_t message_id;
        bool is_private;
        /* Can be null */
        const char *first_name;
};

static bool
get_message_info(struct json_object *message,
                 struct message_info *info)
{
        struct json_object *chat, *from;

        bool ret = get_fields(message,
                              "chat", json_type_object, &chat,
                              "from", json_type_object, &from,
                              "text", json_type_string, &info->text,
                              "message_id", json_type_int, &info->message_id,
                              NULL);
        if (!ret)
                return false;

        ret = get_fields(from,
                         "id", json_type_int, &info->from_id,
                         NULL);
        if (!ret)
                return false;

        ret = get_fields(from,
                         "first_name", json_type_string, &info->first_name,
                         NULL);
        if (!ret)
                info->first_name = NULL;

        ret = get_fields(chat,
                         "id", json_type_int, &info->chat_id,
                         NULL);
        if (!ret)
                return false;

        const char *chat_type;

        ret = get_fields(chat,
                         "type", json_type_string, &chat_type,
                         NULL);
        info->is_private = ret && !strcmp(chat_type, "private");

        return true;
}

static int
find_player(struct pcx_http_game *game,
            int64_t player_id)
{
        for (int i = 0; i < game->n_players; i++) {
                if (game->players[i].id == player_id)
                        return i;
        }

        return -1;
}

static void
process_join(struct pcx_http_game *game,
             const struct message_info *info)
{
        if (info->chat_id != game->game_chat)
                return;

        int player_num = find_player(game, info->from_id);

        if (player_num != -1) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "Vi jam estas en la ludo");
                return;
        }

        if (game->n_players >= PCX_GAME_MAX_PLAYERS) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "La ludo jam estas plena");
                return;
        }

        if (game->game) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "La ludo jam komenciĝis");
                return;
        }

        struct player *player = game->players + game->n_players++;

        if (info->first_name) {
                player->name = pcx_strdup(info->first_name);
        } else {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "Sr.%" PRIi64, info->from_id);
                player->name = (char *) buf.data;
        }

        player->id = info->from_id;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (unsigned i = 0; i < game->n_players; i++) {
                if (i > 0) {
                        if (i == game->n_players - 1)
                                pcx_buffer_append_string(&buf, " kaj ");
                        else
                                pcx_buffer_append_string(&buf, ", ");
                }
                pcx_buffer_append_string(&buf, game->players[i].name);
        }

        send_message_printf(game,
                            info->chat_id,
                            info->message_id,
                            "Bonvenon. Aliaj ludantoj tajpu "
                            "/aligxi por aliĝi al la ludo aŭ tajpu /komenci "
                            "por komenci la ludon. La aktualaj ludantoj "
                            "estas:\n"
                            "%s",
                            (char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static void
process_start(struct pcx_http_game *game,
              const struct message_info *info)
{
        if (info->chat_id != game->game_chat)
                return;

        if (game->game) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "La ludo jam komenciĝis");
                return;
        }

        if (game->n_players <= 0) {
                process_join(game, info);
                return;
        }

        int player_num = find_player(game, info->from_id);

        if (player_num == -1) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "Aliĝi al la ludo per /aligxi antaŭ ol "
                                    "komenci ĝin");
                return;
        }

        if (game->n_players < 2) {
                send_message_printf(game,
                                    info->chat_id,
                                    info->message_id,
                                    "Necesas almenaŭ 2 ludantoj por ludi.");
                return;
        }

        const char *names[PCX_GAME_MAX_PLAYERS];

        for (unsigned i = 0; i < game->n_players; i++)
                names[i] = game->players[i].name;

        game->game = pcx_game_new(&game_callbacks,
                                  game,
                                  game->n_players,
                                  names);
}

static bool
process_entity(struct pcx_http_game *game,
               struct json_object *entity,
               const struct message_info *info)
{
        int64_t offset, length;
        const char *type;

        bool ret = get_fields(entity,
                              "offset", json_type_int, &offset,
                              "length", json_type_int, &length,
                              "type", json_type_string, &type,
                              NULL);
        if (!ret)
                return false;

        if (offset < 0 || length < 1 || offset + length > strlen(info->text))
                return false;

        if (strcmp(type, "bot_command"))
                return true;

        const char *at = memchr(info->text + offset, '@', length);

        if (at) {
                size_t botname_len = strlen(game->botname);
                if (info->text + offset + length - at - 1 != botname_len ||
                    memcmp(at + 1, game->botname, botname_len)) {
                        return true;
                }

                length = at - (info->text + offset);
        }

        static const struct {
                const char *name;
                void (* func)(struct pcx_http_game *game,
                              const struct message_info *info);
        } commands[] = {
                { "/aligxi", process_join },
                { "/komenci", process_start }
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(commands); i++) {
                if (length != strlen(commands[i].name) ||
                    memcmp(info->text + offset, commands[i].name, length))
                        continue;

                commands[i].func(game, info);

                break;
        }

        return true;
}

static bool
process_message(struct pcx_http_game *game,
                struct json_object *message)
{
        struct message_info info;

        if (!get_message_info(message, &info))
                return false;

        struct json_object *entities;

        if (!get_fields(message,
                        "entities", json_type_array, &entities,
                        NULL))
                return true;

        for (unsigned i = 0; i < json_object_array_length(entities); i++) {
                struct json_object *entity =
                        json_object_array_get_idx(entities, i);

                if (!process_entity(game, entity, &info))
                        return false;
        }

        return true;
}

static bool
process_updates(struct pcx_http_game *game,
                struct json_object *obj)
{
        struct json_object *result;
        bool ok;
        bool ret = get_fields(obj,
                              "ok", json_type_boolean, &ok,
                              "result", json_type_array, &result,
                              NULL);
        if (!ret || !ok)
                return false;

        game->last_update_id = 0;

        for (unsigned i = 0; i < json_object_array_length(result); i++) {
                struct json_object *update =
                        json_object_array_get_idx(result, i);
                int64_t update_id;

                if (get_fields(update,
                               "update_id", json_type_int, &update_id,
                               NULL) &&
                    update_id > game->last_update_id) {
                        game->last_update_id = update_id;
                }

                struct json_object *message;

                if (get_fields(update,
                               "message", json_type_object, &message,
                               NULL)) {
                        if (!process_message(game, message))
                                return false;
                }

                struct json_object *callback;

                if (get_fields(update,
                               "callback_query", json_type_object, &callback,
                               NULL)) {
                        if (!process_callback(game, callback))
                                return false;
                }
        }

        return true;
}

static size_t
updates_write_cb(char *ptr,
                 size_t size,
                 size_t nmemb,
                 void *userdata)
{
        struct pcx_http_game *game = userdata;

        struct json_object *obj =
                json_tokener_parse_ex(game->tokener,
                                      ptr,
                                      size * nmemb);

        if (obj) {
                bool ret = process_updates(game, obj);
                json_object_put(obj);
                return ret ? size * nmemb : 0;
        }

        enum json_tokener_error error =
                json_tokener_get_error(game->tokener);

        if (error == json_tokener_continue)
                return size * nmemb;
        else
                return 0;
}

static void
set_updates_handle_options(struct pcx_http_game *game)
{
        set_easy_handle_method(game, game->updates_handle, "getUpdates");

        curl_easy_setopt(game->updates_handle,
                         CURLOPT_WRITEFUNCTION,
                         updates_write_cb);
        curl_easy_setopt(game->updates_handle, CURLOPT_WRITEDATA, game);

        struct json_object *au = json_object_new_array();
        json_object_array_add(au, json_object_new_string("message"));
        json_object_array_add(au, json_object_new_string("callback_query"));
        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "allowed_updates", au);
        json_object_object_add(obj, "timeout", json_object_new_int(300));

        if (game->last_update_id > 0) {
                struct json_object *id =
                        json_object_new_int64(game->last_update_id + 1);
                json_object_object_add(obj, "offset", id);
        }

        set_post_json_data(game, game->updates_handle, obj);

        json_object_put(obj);
}

struct pcx_http_game *
pcx_http_game_new(struct pcx_error **error)
{
        struct pcx_http_game *game = pcx_calloc(sizeof *game);

        pcx_list_init(&game->sockets);
        pcx_list_init(&game->inflight_requests);
        pcx_list_init(&game->unused_requests);

        curl_global_init(CURL_GLOBAL_ALL);

        if (!load_config(game, error))
                goto error;

        game->tokener = json_tokener_new();

        game->url_base = pcx_strconcat("https://api.telegram.org/bot",
                                       game->apikey,
                                       "/",
                                       NULL);

        game->curlm = curl_multi_init();

        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETFUNCTION, socket_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_SOCKETDATA, game);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERFUNCTION, timer_cb);
        curl_multi_setopt(game->curlm, CURLMOPT_TIMERDATA, game);

        game->content_type_headers =
                curl_slist_append(NULL,
                                  "Content-Type: "
                                  "application/json; charset=utf-8");

        game->updates_handle = curl_easy_init();

        set_updates_handle_options(game);

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

static void
free_requests(struct pcx_http_game *game,
              struct pcx_list *list,
              bool remove_from_multi)
{
        struct request *request, *tmp;

        pcx_list_for_each_safe(request, tmp, list, link) {
                if (remove_from_multi)
                        curl_multi_remove_handle(game->curlm, request->handle);
                curl_easy_cleanup(request->handle);
                json_tokener_free(request->tokener);
                pcx_free(request);
        }
}

void
pcx_http_game_free(struct pcx_http_game *game)
{
        if (game->updates_handle) {
                curl_multi_remove_handle(game->curlm,
                                         game->updates_handle);
                curl_easy_cleanup(game->updates_handle);
        }

        free_requests(game, &game->inflight_requests, true);
        free_requests(game, &game->unused_requests, false);

        curl_slist_free_all(game->content_type_headers);

        if (game->curlm)
                curl_multi_cleanup(game->curlm);

        if (game->tokener)
                json_tokener_free(game->tokener);

        curl_global_cleanup();

        reset_game(game);

        remove_sockets(game);

        remove_timeout_source(game);
        remove_restart_updates_source(game);

        pcx_free(game->apikey);
        pcx_free(game->botname);
        pcx_free(game->announce_channel);

        pcx_free(game->url_base);

        pcx_free(game);
}
