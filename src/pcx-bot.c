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

#include "pcx-bot.h"

#include <curl/curl.h>
#include <json_object.h>
#include <json_tokener.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-config.h"
#include "pcx-buffer.h"
#include "pcx-game-help.h"
#include "pcx-curl-multi.h"

#define GAME_TIMEOUT (5 * 60 * 1000)

struct player {
        int64_t id;
        char *name;
};

struct game {
        struct pcx_list link;
        struct pcx_game *game;
        int n_players;
        struct player players[PCX_GAME_MAX_PLAYERS];
        int64_t chat;
        struct pcx_main_context_source *game_timeout_source;
        struct pcx_bot *bot;
};

struct pcx_bot {
        struct pcx_list games;

        struct pcx_main_context_source *restart_updates_source;

        const struct pcx_config_bot *config;

        char *url_base;

        struct pcx_curl_multi *pcurl;

        CURL *updates_handle;
        struct curl_slist *content_type_headers;

        struct json_tokener *tokener;

        int64_t last_update_id;

        CURL *request_handle;
        bool request_handle_busy;
        struct pcx_list queued_requests;
        struct json_tokener *request_tokener;

        struct pcx_buffer known_ids;
};

struct request {
        struct pcx_list link;
        char *method;
        struct json_object *args;
};

static void
set_updates_handle_options(struct pcx_bot *bot);

static void
start_game(struct pcx_bot *bot,
           struct game *game);

static void
maybe_start_request(struct pcx_bot *bot);

static void
get_updates_finished_cb(CURLcode code,
                        void *user_data);

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

static void
set_post_json_data(struct pcx_bot *bot,
                   CURL *handle,
                   struct json_object *obj)
{
        curl_easy_setopt(handle,
                         CURLOPT_HTTPHEADER,
                         bot->content_type_headers);
        curl_easy_setopt(handle,
                         CURLOPT_COPYPOSTFIELDS,
                         json_object_to_json_string(obj));
}

static void
set_easy_handle_method(struct pcx_bot *bot,
                       CURL *e,
                       const char *method)
{
        char *url = pcx_strconcat(bot->url_base, method, NULL);
        curl_easy_setopt(e, CURLOPT_URL, url);
        pcx_free(url);
}

static void
text_append_vprintf(struct pcx_bot *bot,
                    struct pcx_buffer *buf,
                    enum pcx_text_string string,
                    va_list ap)
{
        const char *format = pcx_text_get(bot->config->language, string);
        pcx_buffer_append_vprintf(buf, format, ap);
}

static void
text_append_printf(struct pcx_bot *bot,
                   struct pcx_buffer *buf,
                   enum pcx_text_string string,
                   ...)
{
        va_list ap;
        va_start(ap, string);
        text_append_vprintf(bot, buf, string, ap);
        va_end(ap);
}

static size_t
request_write_cb(char *ptr,
                 size_t size,
                 size_t nmemb,
                 void *userdata)
{
        struct pcx_bot *bot = userdata;

        struct json_object *obj =
                json_tokener_parse_ex(bot->request_tokener,
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
                json_tokener_get_error(bot->request_tokener);

        if (error == json_tokener_continue)
                return size * nmemb;
        else
                return 0;
}

static void
free_request(struct request *request)
{
        pcx_list_remove(&request->link);
        json_object_put(request->args);
        pcx_free(request->method);
        pcx_free(request);
}

static void
request_finished_cb(CURLcode code,
                    void *user_data)
{
        struct pcx_bot *bot = user_data;

        if (code != CURLE_OK) {
                fprintf(stderr,
                        "request failed: %s\n",
                        curl_easy_strerror(code));
        }

        pcx_curl_multi_remove_handle(bot->pcurl, bot->request_handle);
        curl_easy_reset(bot->request_handle);

        json_tokener_reset(bot->request_tokener);

        bot->request_handle_busy = false;

        maybe_start_request(bot);
}

static void
maybe_start_request(struct pcx_bot *bot)
{
        if (bot->request_handle_busy ||
            pcx_list_empty(&bot->queued_requests))
                return;

        struct request *request =
                pcx_container_of(bot->queued_requests.next,
                                 struct request,
                                 link);

        bot->request_handle_busy = true;

        set_easy_handle_method(bot, bot->request_handle, request->method);

        curl_easy_setopt(bot->request_handle,
                         CURLOPT_WRITEFUNCTION,
                         request_write_cb);
        curl_easy_setopt(bot->request_handle, CURLOPT_WRITEDATA, bot);

        set_post_json_data(bot, bot->request_handle, request->args);

        pcx_curl_multi_add_handle(bot->pcurl,
                                  bot->request_handle,
                                  request_finished_cb,
                                  bot);

        free_request(request);
}

static void
send_request(struct pcx_bot *bot,
             const char *method,
             struct json_object *args)
{
        struct request *request = pcx_alloc(sizeof *request);

        request->args = json_object_get(args);
        request->method = pcx_strdup(method);

        pcx_list_insert(bot->queued_requests.prev, &request->link);

        maybe_start_request(bot);
}

static void
send_message_full(struct pcx_bot *bot,
                  int64_t chat_id,
                  int64_t in_reply_to,
                  enum pcx_game_message_format format,
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

        switch (format) {
        case PCX_GAME_MESSAGE_FORMAT_HTML:
                json_object_object_add(args,
                                       "parse_mode",
                                       json_object_new_string("HTML"));
                break;
        case PCX_GAME_MESSAGE_FORMAT_PLAIN:
                break;
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

        send_request(bot, "sendMessage", args);

        json_object_put(args);
}

static void
send_message(struct pcx_bot *bot,
             int64_t chat_id,
             int64_t in_reply_to,
             const char *message)
{
        send_message_full(bot,
                          chat_id,
                          in_reply_to,
                          PCX_GAME_MESSAGE_FORMAT_PLAIN,
                          message,
                          0, /* n_buttons */
                          NULL /* buttons */);
}

static void
send_message_vprintf(struct pcx_bot *bot,
                     int64_t chat_id,
                     int64_t in_reply_to,
                     enum pcx_text_string string,
                     va_list ap)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *format = pcx_text_get(bot->config->language, string);
        pcx_buffer_append_vprintf(&buf, format, ap);

        send_message(bot,
                     chat_id,
                     in_reply_to,
                     (char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static void
send_message_printf(struct pcx_bot *bot,
                    int64_t chat_id,
                    int64_t in_reply_to,
                    enum pcx_text_string string,
                    ...)
{
        va_list ap;
        va_start(ap, string);
        send_message_vprintf(bot,
                             chat_id,
                             in_reply_to,
                             string,
                             ap);
        va_end(ap);
}

static void
remove_game_timeout_source(struct game *game)
{
        if (game->game_timeout_source == NULL)
                return;

        pcx_main_context_remove_source(game->game_timeout_source);
        game->game_timeout_source = NULL;
}

static void
remove_restart_updates_source(struct pcx_bot *bot)
{
        if (bot->restart_updates_source == NULL)
                return;

        pcx_main_context_remove_source(bot->restart_updates_source);
        bot->restart_updates_source = NULL;
}

static void
restart_updates_cb(struct pcx_main_context_source *source,
                   void *user_data)
{
        struct pcx_bot *bot = user_data;

        bot->restart_updates_source = NULL;

        pcx_curl_multi_remove_handle(bot->pcurl,
                                     bot->updates_handle);

        curl_easy_reset(bot->updates_handle);
        set_updates_handle_options(bot);

        json_tokener_reset(bot->tokener);

        pcx_curl_multi_add_handle(bot->pcurl,
                                  bot->updates_handle,
                                  get_updates_finished_cb,
                                  bot);
}

static void
get_updates_finished_cb(CURLcode code,
                        void *user_data)
{
        struct pcx_bot *bot = user_data;
        long timeout = 0;

        if (code != CURLE_OK) {
                fprintf(stderr,
                        "getUpdates failed: %s\n",
                        curl_easy_strerror(code));
                timeout = 60 * 1000;
        }

        remove_restart_updates_source(bot);

        bot->restart_updates_source =
                pcx_main_context_add_timeout(NULL,
                                             timeout,
                                             restart_updates_cb,
                                             bot);
}

static char *
get_data_file(const char *name)
{
        const char *home = getenv("HOME");

        if (home == NULL)
                return NULL;

        return pcx_strconcat(home, "/.pucxobot/", name, NULL);
}

static void
remove_game(struct game *game)
{
        if (game->game)
                pcx_game_free(game->game);

        for (unsigned i = 0; i < game->n_players; i++)
                pcx_free(game->players[i].name);

        remove_game_timeout_source(game);

        pcx_list_remove(&game->link);
        pcx_free(game);
}

static void
game_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct game *game = user_data;
        struct pcx_bot *bot = game->bot;

        game->game_timeout_source = NULL;

        if (game->game == NULL && game->n_players >= 2) {
                send_message_printf(bot,
                                    game->chat,
                                    -1, /* in_reply_to */
                                    PCX_TEXT_STRING_TIMEOUT_START,
                                    GAME_TIMEOUT / (60 * 1000));

                start_game(bot, game);
        } else {
                send_message_printf(bot,
                                    game->chat,
                                    -1, /* in_reply_to */
                                    PCX_TEXT_STRING_TIMEOUT_ABANDON,
                                    GAME_TIMEOUT / (60 * 1000));

                remove_game(game);
        }
}

static void
set_game_timeout(struct game *game)
{
        remove_game_timeout_source(game);

        game->game_timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             GAME_TIMEOUT,
                                             game_timeout_cb,
                                             game);
}

static int
find_player_in_game(struct game *game,
                    int64_t player_id)
{
        for (int i = 0; i < game->n_players; i++) {
                if (game->players[i].id == player_id)
                        return i;
        }

        return -1;
}

static bool
find_player(struct pcx_bot *bot,
            int64_t player_id,
            struct game **game_out,
            int *player_num_out)
{
        struct game *game;

        pcx_list_for_each(game, &bot->games, link) {
                int player_num = find_player_in_game(game, player_id);
                if (player_num != -1) {
                        *game_out = game;
                        *player_num_out = player_num;
                        return true;
                }
        }

        return false;
}

static void
send_private_message_cb(int user_num,
                        enum pcx_game_message_format format,
                        const char *message,
                        size_t n_buttons,
                        const struct pcx_game_button *buttons,
                        void *user_data)
{
        struct game *game = user_data;
        struct pcx_bot *bot = game->bot;

        assert(user_num >= 0 && user_num < game->n_players);

        send_message_full(bot,
                          game->players[user_num].id,
                          -1, /* in_reply_to */
                          format,
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
        struct game *game = user_data;
        struct pcx_bot *bot = game->bot;

        send_message_full(bot,
                          game->chat,
                          -1, /* in_reply_to */
                          format,
                          message,
                          n_buttons,
                          buttons);
}

static void
game_over_cb(void *user_data)
{
        struct game *game = user_data;
        remove_game(game);
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_private_message = send_private_message_cb,
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
answer_callback(struct pcx_bot *bot,
                const char *id)
{
        struct json_object *args = json_object_new_object();

        json_object_object_add(args,
                               "callback_query_id",
                               json_object_new_string(id));

        send_request(bot, "answerCallbackQuery", args);

        json_object_put(args);
}

static void
process_callback(struct pcx_bot *bot,
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
                return;

        int64_t from_id;

        ret = get_fields(from,
                         "id", json_type_int, &from_id,
                         NULL);

        if (!ret)
                return;

        answer_callback(bot, id);

        struct game *game;
        int player_num;

        if (find_player(bot, from_id, &game, &player_num) && game->game) {
                set_game_timeout(game);
                pcx_game_handle_callback_data(game->game,
                                              player_num,
                                              callback_data);
        }
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

static bool
is_known_id(struct pcx_bot *bot,
            int64_t id)
{
        size_t n_ids = bot->known_ids.length / sizeof (int64_t);
        const int64_t *ids = (const int64_t *) bot->known_ids.data;

        for (unsigned i = 0; i < n_ids; i++) {
                if (ids[i] == id)
                        return true;
        }

        return false;
}

static void
save_known_ids(struct pcx_bot *bot)
{
        char *fn = get_data_file("known_ids.txt");

        if (fn == NULL)
                return;

        char *tmp_fn = pcx_strconcat(fn, ".tmp", NULL);
        FILE *f = fopen(tmp_fn, "w");

        if (f) {
                size_t n_ids = bot->known_ids.length / sizeof (int64_t);
                const int64_t *ids = (const int64_t *) bot->known_ids.data;

                for (unsigned i = 0; i < n_ids; i++)
                        fprintf(f, "%" PRIi64 "\n", ids[i]);

                fclose(f);

                rename(tmp_fn, fn);
        }

        pcx_free(tmp_fn);
        pcx_free(fn);
}

static void
load_known_ids(struct pcx_bot *bot)
{
        char *fn = get_data_file("known_ids.txt");

        if (fn == NULL)
                return;

        FILE *f = fopen(fn, "r");

        if (f) {
                bot->known_ids.length = 0;

                int64_t id;

                while (fscanf(f, "%" PRIi64 "\n", &id) == 1)
                        pcx_buffer_append(&bot->known_ids, &id, sizeof id);

                fclose(f);
        }

        pcx_free(fn);
}

static void
add_known_id(struct pcx_bot *bot,
             int64_t id)
{
        pcx_buffer_append(&bot->known_ids, &id, sizeof id);
        save_known_ids(bot);
}

static struct game *
find_game(struct pcx_bot *bot,
          int64_t chat_id)
{
        struct game *game;

        pcx_list_for_each(game, &bot->games, link) {
                if (game->chat == chat_id)
                        return game;
        }


        return NULL;
}

static struct game *
find_or_create_game(struct pcx_bot *bot,
                    int64_t chat_id)
{
        struct game *game = find_game(bot, chat_id);

        if (game == NULL) {
                game = pcx_calloc(sizeof *game);
                game->chat = chat_id;
                game->bot = bot;

                pcx_list_insert(&bot->games, &game->link);
        }

        return game;
}

static void
process_join(struct pcx_bot *bot,
             const struct message_info *info)
{
        struct game *game;
        int player_num;

        if (info->is_private) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_NEED_PUBLIC_GROUP);
                return;
        }

        if (!is_known_id(bot, info->from_id)) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE,
                                    bot->config->botname);
                return;
        }

        if (find_player(bot, info->from_id, &game, &player_num)) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_ALREADY_IN_GAME);
                return;
        }

        game = find_or_create_game(bot, info->chat_id);

        if (game->n_players >= PCX_GAME_MAX_PLAYERS) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_GAME_FULL);
                return;
        }

        if (game->game) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_GAME_ALREADY_STARTED);
                return;
        }

        set_game_timeout(game);

        struct player *player = game->players + game->n_players++;

        if (info->first_name) {
                player->name = pcx_strdup(info->first_name);
        } else {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                text_append_printf(bot,
                                   &buf,
                                   PCX_TEXT_STRING_NAME_FROM_ID,
                                   (int) info->from_id);
                player->name = (char *) buf.data;
        }

        player->id = info->from_id;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *final_separator =
                pcx_text_get(bot->config->language,
                             PCX_TEXT_STRING_FINAL_CONJUNCTION);

        for (unsigned i = 0; i < game->n_players; i++) {
                if (i > 0) {
                        if (i == game->n_players - 1)
                                pcx_buffer_append_string(&buf, final_separator);
                        else
                                pcx_buffer_append_string(&buf, ", ");
                }
                pcx_buffer_append_string(&buf, game->players[i].name);
        }

        send_message_printf(bot,
                            info->chat_id,
                            info->message_id,
                            PCX_TEXT_STRING_WELCOME,
                            (char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static void
start_game(struct pcx_bot *bot,
           struct game *game)
{
        assert(game->game == NULL);

        set_game_timeout(game);

        const char *names[PCX_GAME_MAX_PLAYERS];

        for (unsigned i = 0; i < game->n_players; i++)
                names[i] = game->players[i].name;

        game->game = pcx_game_new(&game_callbacks,
                                  game,
                                  bot->config->language,
                                  game->n_players,
                                  names);
}

static void
process_start(struct pcx_bot *bot,
              const struct message_info *info)
{
        struct game *game = find_game(bot, info->chat_id);

        if (game == NULL) {
                process_join(bot, info);
                return;
        }

        if (game->game) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_GAME_ALREADY_STARTED);
                return;
        }

        int player_num = find_player_in_game(game, info->from_id);

        if (player_num == -1) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_JOIN_BEFORE_START);
                return;
        }

        if (game->n_players < 2) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_NEED_TWO_PLAYERS);
                return;
        }

        start_game(bot, game);
}

static void
process_help(struct pcx_bot *bot,
             const struct message_info *info)
{
        send_message_full(bot,
                          info->chat_id,
                          info->message_id,
                          PCX_GAME_MESSAGE_FORMAT_HTML,
                          pcx_game_help[bot->config->language],
                          0, /* n_buttons */
                          NULL /* buttons */);
}

static void
process_entity(struct pcx_bot *bot,
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
                return;

        if (offset < 0 || length < 1 || offset + length > strlen(info->text))
                return;

        if (strcmp(type, "bot_command"))
                return;

        const char *at = memchr(info->text + offset, '@', length);

        if (at) {
                size_t botname_len = strlen(bot->config->botname);
                if (info->text + offset + length - at - 1 != botname_len ||
                    memcmp(at + 1, bot->config->botname, botname_len)) {
                        return;
                }

                length = at - (info->text + offset);
        }

        static const struct {
                enum pcx_text_string name;
                void (* func)(struct pcx_bot *bot,
                              const struct message_info *info);
        } commands[] = {
                { PCX_TEXT_STRING_JOIN_COMMAND, process_join },
                { PCX_TEXT_STRING_START_COMMAND, process_start },
                { PCX_TEXT_STRING_HELP_COMMAND, process_help },
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(commands); i++) {
                const char *name =
                        pcx_text_get(bot->config->language,
                                     commands[i].name);
                if (length != strlen(name) ||
                    memcmp(info->text + offset, name, length))
                        continue;

                commands[i].func(bot, info);

                break;
        }
}

static void
process_message(struct pcx_bot *bot,
                struct json_object *message)
{
        struct message_info info;

        if (!get_message_info(message, &info))
                return;

        if (info.is_private && !is_known_id(bot, info.from_id)) {
                add_known_id(bot, info.from_id);
                send_message_printf(bot,
                                    info.chat_id,
                                    info.message_id,
                                    PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE);
        }

        struct json_object *entities;

        if (!get_fields(message,
                        "entities", json_type_array, &entities,
                        NULL))
                return;

        for (unsigned i = 0; i < json_object_array_length(entities); i++) {
                struct json_object *entity =
                        json_object_array_get_idx(entities, i);

                process_entity(bot, entity, &info);
        }
}

static bool
process_updates(struct pcx_bot *bot,
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

        bot->last_update_id = 0;

        for (unsigned i = 0; i < json_object_array_length(result); i++) {
                struct json_object *update =
                        json_object_array_get_idx(result, i);
                int64_t update_id;

                if (get_fields(update,
                               "update_id", json_type_int, &update_id,
                               NULL) &&
                    update_id > bot->last_update_id) {
                        bot->last_update_id = update_id;
                }

                struct json_object *message;

                if (get_fields(update,
                               "message", json_type_object, &message,
                               NULL)) {
                        process_message(bot, message);
                }

                struct json_object *callback;

                if (get_fields(update,
                               "callback_query", json_type_object, &callback,
                               NULL)) {
                        process_callback(bot, callback);
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
        struct pcx_bot *bot = userdata;

        struct json_object *obj =
                json_tokener_parse_ex(bot->tokener,
                                      ptr,
                                      size * nmemb);

        if (obj) {
                bool ret = process_updates(bot, obj);
                json_object_put(obj);
                return ret ? size * nmemb : 0;
        }

        enum json_tokener_error error =
                json_tokener_get_error(bot->tokener);

        if (error == json_tokener_continue)
                return size * nmemb;
        else
                return 0;
}

static void
set_updates_handle_options(struct pcx_bot *bot)
{
        set_easy_handle_method(bot, bot->updates_handle, "getUpdates");

        curl_easy_setopt(bot->updates_handle,
                         CURLOPT_WRITEFUNCTION,
                         updates_write_cb);
        curl_easy_setopt(bot->updates_handle, CURLOPT_WRITEDATA, bot);

        struct json_object *au = json_object_new_array();
        json_object_array_add(au, json_object_new_string("message"));
        json_object_array_add(au, json_object_new_string("callback_query"));
        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "allowed_updates", au);
        json_object_object_add(obj, "timeout", json_object_new_int(300));

        if (bot->last_update_id > 0) {
                struct json_object *id =
                        json_object_new_int64(bot->last_update_id + 1);
                json_object_object_add(obj, "offset", id);
        }

        set_post_json_data(bot, bot->updates_handle, obj);

        json_object_put(obj);
}

struct pcx_bot *
pcx_bot_new(struct pcx_curl_multi *pcurl,
            const struct pcx_config_bot *config)
{
        struct pcx_bot *bot = pcx_calloc(sizeof *bot);

        pcx_list_init(&bot->queued_requests);
        pcx_list_init(&bot->games);

        pcx_buffer_init(&bot->known_ids);

        bot->config = config;

        load_known_ids(bot);

        bot->tokener = json_tokener_new();

        bot->url_base = pcx_strconcat("https://api.telegram.org/bot",
                                      bot->config->apikey,
                                      "/",
                                      NULL);

        bot->pcurl = pcurl;

        bot->content_type_headers =
                curl_slist_append(NULL,
                                  "Content-Type: "
                                  "application/json; charset=utf-8");

        bot->updates_handle = curl_easy_init();

        set_updates_handle_options(bot);

        pcx_curl_multi_add_handle(bot->pcurl,
                                  bot->updates_handle,
                                  get_updates_finished_cb,
                                  bot);

        bot->request_handle = curl_easy_init();
        bot->request_tokener = json_tokener_new();

        return bot;
}

static void
free_requests(struct pcx_bot *bot)
{
        struct request *request, *tmp;

        pcx_list_for_each_safe(request, tmp, &bot->queued_requests, link) {
                free_request(request);
        }
}

static void
free_games(struct pcx_bot *bot)
{
        struct game *game, *tmp;

        pcx_list_for_each_safe(game, tmp, &bot->games, link) {
                remove_game(game);
        }
}

void
pcx_bot_free(struct pcx_bot *bot)
{
        if (bot->updates_handle) {
                pcx_curl_multi_remove_handle(bot->pcurl,
                                             bot->updates_handle);
                curl_easy_cleanup(bot->updates_handle);
        }

        if (bot->request_handle) {
                if (bot->request_handle_busy) {
                        pcx_curl_multi_remove_handle(bot->pcurl,
                                                     bot->request_handle);
                }
                curl_easy_cleanup(bot->request_handle);
        }

        free_requests(bot);

        curl_slist_free_all(bot->content_type_headers);

        if (bot->tokener)
                json_tokener_free(bot->tokener);

        if (bot->request_tokener)
                json_tokener_free(bot->request_tokener);

        free_games(bot);

        remove_restart_updates_source(bot);

        pcx_buffer_destroy(&bot->known_ids);

        pcx_free(bot->url_base);

        pcx_free(bot);
}
