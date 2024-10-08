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

#include "config.h"

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
#include "pcx-curl-multi.h"
#include "pcx-message-queue.h"
#include "pcx-log.h"

#define GAME_TIMEOUT (5 * 60 * 1000)
#define IN_GAME_TIMEOUT (GAME_TIMEOUT * 2)

struct player {
        int64_t id;
        char *name;
};

struct game {
        struct pcx_list link;
        void *game;
        int n_players;
        struct pcx_buffer players;
        int64_t chat;
        struct pcx_main_context_source *game_timeout_source;
        struct pcx_bot *bot;
        const struct pcx_game *type;
        char letter_id;
};

struct pcx_bot {
        struct pcx_list games;

        struct pcx_main_context_source *restart_updates_source;

        const struct pcx_config *config;
        const struct pcx_config_bot *bot_config;

        struct pcx_class_store *class_store;

        char *url_base;

        struct pcx_curl_multi *pcurl;

        CURL *updates_handle;
        struct curl_slist *content_type_headers;

        struct json_tokener *tokener;

        int64_t last_update_id;

        CURL *request_handle;
        bool request_handle_busy;
        struct pcx_main_context_source *start_request_source;
        struct pcx_list queued_requests;
        struct json_tokener *request_tokener;

        /* Messages are stored in a separate queue so that we can
         * rate-limit them per chat.
         */
        struct pcx_message_queue *message_queue;
        struct pcx_main_context_source *message_delay_source;

        struct pcx_buffer known_ids;

        unsigned next_id;
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
        const char *format = pcx_text_get(bot->bot_config->language, string);
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
                        if (ret) {
                                pcx_log("%s: %s",
                                        bot->bot_config->botname,
                                        desc);
                        }
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
                pcx_log("%s: request failed: %s",
                        bot->bot_config->botname,
                        curl_easy_strerror(code));
        }

        pcx_curl_multi_remove_handle(bot->pcurl, bot->request_handle);
        curl_easy_reset(bot->request_handle);

        json_tokener_reset(bot->request_tokener);

        bot->request_handle_busy = false;

        maybe_start_request(bot);
}

static void
start_request_cb(struct pcx_main_context_source *source,
                 void *user_data)
{
        struct pcx_bot *bot = user_data;

        bot->start_request_source = NULL;

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
send_request_no_start(struct pcx_bot *bot,
                      const char *method,
                      struct json_object *args)
{
        struct request *request = pcx_alloc(sizeof *request);

        request->args = json_object_get(args);
        request->method = pcx_strdup(method);

        pcx_list_insert(bot->queued_requests.prev, &request->link);
}

static void
send_delay_cb(struct pcx_main_context_source *source,
              void *user_data)
{
        struct pcx_bot *bot = user_data;

        bot->message_delay_source = NULL;

        maybe_start_request(bot);
}

static void
start_message_delay(struct pcx_bot *bot,
                    uint64_t send_delay)
{
        assert(bot->message_delay_source == NULL);

        bot->message_delay_source =
                pcx_main_context_add_timeout(NULL,
                                             send_delay,
                                             send_delay_cb,
                                             bot);
}

static void
remove_message_delay_source(struct pcx_bot *bot)
{
        if (bot->message_delay_source == NULL)
                return;

        pcx_main_context_remove_source(bot->message_delay_source);
        bot->message_delay_source = NULL;
}

static void
maybe_start_request(struct pcx_bot *bot)
{
        if (bot->request_handle_busy ||
            bot->start_request_source)
                return;

        remove_message_delay_source(bot);

        if (pcx_list_empty(&bot->queued_requests)) {
                struct json_object *args;
                bool has_delayed_message;
                uint64_t send_delay;

                args = pcx_message_queue_get(bot->message_queue,
                                             &has_delayed_message,
                                             &send_delay);

                if (args) {
                        send_request_no_start(bot, "sendMessage", args);
                        json_object_put(args);
                } else {
                        if (has_delayed_message)
                                start_message_delay(bot, send_delay);

                        return;
                }
        }

        bot->start_request_source =
                pcx_main_context_add_timeout(NULL,
                                             0, /* ms */
                                             start_request_cb,
                                             bot);
}

static void
send_request(struct pcx_bot *bot,
             const char *method,
             struct json_object *args)
{
        send_request_no_start(bot, method, args);

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

        pcx_message_queue_add(bot->message_queue,
                              chat_id,
                              args);

        json_object_put(args);

        maybe_start_request(bot);
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

        const char *format = pcx_text_get(bot->bot_config->language, string);
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
                pcx_log("%s: getUpdates failed: %s",
                        bot->bot_config->botname,
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
get_known_ids_file(struct pcx_bot *bot)
{
        return pcx_strconcat(bot->config->data_dir,
                             "/known-ids-",
                             bot->bot_config->botname,
                             ".txt",
                             NULL);
}

static struct player *
get_player(struct game *game,
           int player_num)
{
        assert(player_num >= 0 && player_num < game->n_players);
        return ((struct player *) game->players.data) + player_num;
}

static void
remove_game(struct pcx_bot *bot,
            struct game *game)
{
        if (game->game)
                game->type->free_game_cb(game->game);

        for (unsigned i = 0; i < game->n_players; i++)
                pcx_free(get_player(game, i)->name);

        pcx_buffer_destroy(&game->players);

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

        int timeout = (game->game ?
                       IN_GAME_TIMEOUT :
                       GAME_TIMEOUT) / (60 * 1000);

        game->game_timeout_source = NULL;

        if (game->game == NULL &&
            game->n_players > 1 &&
            game->n_players >= game->type->min_players) {
                send_message_printf(bot,
                                    game->chat,
                                    -1, /* in_reply_to */
                                    PCX_TEXT_STRING_TIMEOUT_START,
                                    timeout);

                start_game(bot, game);
        } else {
                send_message_printf(bot,
                                    game->chat,
                                    -1, /* in_reply_to */
                                    PCX_TEXT_STRING_TIMEOUT_ABANDON,
                                    timeout);

                if (game->game) {
                        pcx_log("game %c timed out after starting",
                                game->letter_id);
                } else {
                        pcx_log("game %c timed out without starting",
                                game->letter_id);
                }
                remove_game(bot, game);
        }
}

static void
set_game_timeout(struct game *game)
{
        remove_game_timeout_source(game);

        int timeout = game->game ? IN_GAME_TIMEOUT : GAME_TIMEOUT;

        game->game_timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             timeout,
                                             game_timeout_cb,
                                             game);
}

static int
find_player_in_game(struct game *game,
                    int64_t player_id)
{
        for (int i = 0; i < game->n_players; i++) {
                if (get_player(game, i)->id == player_id)
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
send_message_cb(const struct pcx_game_message *message,
                void *user_data)
{
        struct game *game = user_data;
        struct pcx_bot *bot = game->bot;

        int64_t chat_id = (message->target == -1 ?
                           game->chat :
                           get_player(game, message->target)->id);

        send_message_full(bot,
                          chat_id,
                          -1, /* in_reply_to */
                          message->format,
                          message->text,
                          message->n_buttons,
                          message->buttons);
}

static void
game_over_cb(void *user_data)
{
        struct game *game = user_data;
        pcx_log("game %c finished successfully", game->letter_id);
        remove_game(game->bot, game);
}

static struct pcx_class_store *
get_class_store_cb(void *user_data)
{
        struct game *game = user_data;

        return game->bot->class_store;
}

static void
set_sideband_data_cb(int data_num,
                     const struct pcx_game_sideband_data *value,
                     bool force,
                     void *user_data)
{
}

static const struct pcx_game_callbacks
game_callbacks = {
        .send_message = send_message_cb,
        .game_over = game_over_cb,
        .get_class_store = get_class_store_cb,
        .set_sideband_data = set_sideband_data_cb,
};

static void
delete_message(struct pcx_bot *bot,
               int64_t chat_id,
               int64_t message_id)
{
        struct json_object *args = json_object_new_object();

        json_object_object_add(args,
                               "chat_id",
                               json_object_new_int64(chat_id));
        json_object_object_add(args,
                               "message_id",
                               json_object_new_int64(message_id));

        send_request(bot, "deleteMessage", args);

        json_object_put(args);
}

static void
remove_buttons_from_message(struct pcx_bot *bot,
                            int64_t chat_id,
                            int64_t message_id)
{
        struct json_object *args = json_object_new_object();

        json_object_object_add(args,
                               "chat_id",
                               json_object_new_int64(chat_id));
        json_object_object_add(args,
                               "message_id",
                               json_object_new_int64(message_id));

        send_request(bot, "editMessageReplyMarkup", args);

        json_object_put(args);
}

static void
show_help(struct pcx_bot *bot,
          const struct pcx_game *game,
          int64_t chat_id,
          int64_t message_id)
{
        char *help = game->get_help_cb(bot->bot_config->language);

        send_message_full(bot,
                          chat_id,
                          -1, /* message_id */
                          PCX_GAME_MESSAGE_FORMAT_HTML,
                          help,
                          0, /* n_buttons */
                          NULL /* buttons */);

        pcx_free(help);
}

static bool
can_run_game(struct pcx_bot *bot,
             const struct pcx_game *game)
{
        /* Skip games that don’t have a translation */
        return pcx_text_get(bot->bot_config->language,
                            game->start_command) != NULL;
}

struct message_info {
        const char *text;
        int64_t from_id;
        int64_t chat_id;
        int64_t message_id;
        bool is_private;
        /* Can be null */
        const char *first_name;
        const char *chat_username;
};

static bool
get_message_from_info(struct json_object *from,
                      struct message_info *info)
{
        bool ret = get_fields(from,
                              "id", json_type_int, &info->from_id,
                              NULL);
        if (!ret)
                return false;

        ret = get_fields(from,
                         "first_name", json_type_string, &info->first_name,
                         NULL);
        if (!ret)
                info->first_name = NULL;

        return true;
}

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

        if (!get_message_from_info(from, info))
                return false;

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

        ret = get_fields(chat,
                         "username", json_type_string, &info->chat_username,
                         NULL);
        if (!ret)
                info->chat_username = NULL;

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
        char *fn = get_known_ids_file(bot);
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
        char *fn = get_known_ids_file(bot);
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

static void
append_current_players_message(struct pcx_bot *bot,
                               struct game *game,
                               struct pcx_buffer *buf)
{
        pcx_buffer_append_string(buf,
                                 pcx_text_get(bot->bot_config->language,
                                              PCX_TEXT_STRING_CURRENT_PLAYERS));
        pcx_buffer_append_string(buf, "\n");

        const char *final_separator =
                pcx_text_get(bot->bot_config->language,
                             PCX_TEXT_STRING_FINAL_CONJUNCTION);

        for (unsigned i = 0; i < game->n_players; i++) {
                if (i > 0) {
                        if (i == game->n_players - 1)
                                pcx_buffer_append_string(buf, final_separator);
                        else
                                pcx_buffer_append_string(buf, ", ");
                }
                pcx_buffer_append_string(buf, get_player(game, i)->name);
        }
}

static void
join_game(struct pcx_bot *bot,
          struct game *game,
          const struct message_info *info)
{
        set_game_timeout(game);

        pcx_buffer_set_length(&game->players,
                              (game->n_players + 1) *
                              sizeof (struct player));
        struct player *player = get_player(game, game->n_players++);

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

        enum pcx_text_string welcome_note =
                game->n_players < game->type->max_players ?
                PCX_TEXT_STRING_WELCOME :
                PCX_TEXT_STRING_WELCOME_FULL;

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(bot->bot_config->language,
                                              welcome_note));

        pcx_buffer_append_string(&buf, "\n\n");

        text_append_printf(bot, &buf,
                           PCX_TEXT_STRING_CHOSEN_GAME,
                           pcx_text_get(bot->bot_config->language,
                                        game->type->name_string));

        if (game->n_players < game->type->max_players) {
                pcx_buffer_append_string(&buf, "\n\n");
                append_current_players_message(bot, game, &buf);
        }

        send_message(bot,
                     info->chat_id,
                     info->message_id,
                     (char *) buf.data);

        pcx_buffer_destroy(&buf);

        if (game->n_players >= game->type->max_players)
                start_game(bot, game);
}

static bool
check_id_valid_for_game(struct pcx_bot *bot,
                        const struct pcx_game *game,
                        const struct message_info *info)
{
        if (!game->needs_private_messages)
                return true;

        if (!is_known_id(bot, info->from_id)) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE,
                                    bot->bot_config->botname);
                return false;
        }

        return true;
}

static bool
check_already_in_game(struct pcx_bot *bot,
                      const struct message_info *info)
{
        struct game *game;
        int player_num;

        if (find_player(bot, info->from_id, &game, &player_num)) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_ALREADY_IN_GAME);
                return false;
        }

        return true;
}

static void
send_game_question_reply(struct pcx_bot *bot,
                         enum pcx_text_string question,
                         const char *keyword,
                         const struct message_info *info)
{
        int n_games;

        for (n_games = 0; pcx_game_list[n_games]; n_games++);

        struct pcx_game_button *buttons = pcx_alloc(n_games * sizeof *buttons);

        int n_buttons = 0;

        for (int i = 0; i < n_games; i++) {
                if (!can_run_game(bot, pcx_game_list[i]))
                        continue;

                const struct pcx_game *game = pcx_game_list[i];

                buttons[n_buttons].text =
                        pcx_text_get(bot->bot_config->language,
                                     game->name_string);

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "%s:%s", keyword, game->name);
                buttons[n_buttons].data = (char *) buf.data;

                n_buttons++;
        }

        send_message_full(bot,
                          info->chat_id,
                          info->message_id,
                          PCX_GAME_MESSAGE_FORMAT_PLAIN,
                          pcx_text_get(bot->bot_config->language, question),
                          n_buttons,
                          buttons);

        for (int i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);

        pcx_free(buttons);
}

static void
send_create_game_question(struct pcx_bot *bot,
                          const struct message_info *info)
{
        if (info->is_private) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_NEED_PUBLIC_GROUP);
                return;
        }

        send_game_question_reply(bot,
                                 PCX_TEXT_STRING_WHICH_GAME,
                                 "creategame",
                                 info);
}

static void
log_start_game(struct pcx_bot *bot,
               struct game *game,
               const struct message_info *info)

{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "%c = game of %s",
                                 game->letter_id,
                                 game->type->name);

        if (info->chat_username) {
                pcx_buffer_append_printf(&buf,
                                         " in %s",
                                         info->chat_username);
        } else {
                pcx_buffer_append_printf(&buf,
                                         " in %" PRIi64,
                                         info->chat_id);
        }

        if (info->first_name) {
                pcx_buffer_append_printf(&buf,
                                         " by %s",
                                         info->first_name);
        }

        pcx_buffer_append_printf(&buf, " via @%s", bot->bot_config->botname);

        pcx_log("%s", (const char *) buf.data);

        pcx_buffer_destroy(&buf);
}

static void
process_create_game(struct pcx_bot *bot,
                    const struct pcx_game *game_type,
                    const struct message_info *info)
{
        if (info->is_private) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_NEED_PUBLIC_GROUP);
                return;
        }

        struct game *game = find_game(bot, info->chat_id);

        if (game != NULL) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_ALREADY_GAME);
                return;
        }

        if (!check_id_valid_for_game(bot, game_type, info))
                return;

        if (!check_already_in_game(bot, info))
                return;

        game = pcx_calloc(sizeof *game);
        game->chat = info->chat_id;
        game->bot = bot;
        game->type = game_type;

        pcx_buffer_init(&game->players);

        game->letter_id = bot->next_id++ % 26 + 'A';

        pcx_list_insert(&bot->games, &game->link);

        log_start_game(bot, game, info);

        join_game(bot, game, info);
}

static void
process_join(struct pcx_bot *bot,
             const struct message_info *info)
{
        struct game *game;

        if (!check_already_in_game(bot, info))
                return;

        game = find_game(bot, info->chat_id);

        if (game == NULL) {
                send_create_game_question(bot, info);
                return;
        }

        if (!check_id_valid_for_game(bot, game->type, info))
                return;

        if (game->n_players >= game->type->max_players) {
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

        join_game(bot, game, info);
}

static void
start_game(struct pcx_bot *bot,
           struct game *game)
{
        assert(game->game == NULL);

        const char **names = pcx_alloc(game->n_players * sizeof (const char *));

        for (unsigned i = 0; i < game->n_players; i++)
                names[i] = get_player(game, i)->name;

        pcx_log("game %C started with %i players",
                game->letter_id,
                game->n_players);

        game->game = game->type->create_game_cb(bot->config,
                                                &game_callbacks,
                                                game,
                                                bot->bot_config->language,
                                                game->n_players,
                                                names);

        pcx_free(names);

        set_game_timeout(game);
}

static void
process_start(struct pcx_bot *bot,
              const struct message_info *info)
{
        struct game *game;

        game = find_game(bot, info->chat_id);

        if (game == NULL) {
                send_create_game_question(bot, info);
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

        if (game->n_players < game->type->min_players) {
                send_message_printf(bot,
                                    info->chat_id,
                                    info->message_id,
                                    PCX_TEXT_STRING_NEED_MIN_PLAYERS,
                                    game->type->min_players);
                return;
        }

        start_game(bot, game);
}

static void
process_cancel(struct pcx_bot *bot,
               const struct message_info *info)
{
        struct game *game;
        game = find_game(bot, info->chat_id);

        enum pcx_text_string response;
        if (game != NULL) {
                if (find_player_in_game(game, info->from_id) != -1) {
                        response = PCX_TEXT_STRING_CANCELED;
                        pcx_log("game %c was cancelled", game->letter_id);
                        remove_game(bot, game);
                } else {
                        response = PCX_TEXT_STRING_CANT_CANCEL;
                }
        } else {
                response = PCX_TEXT_STRING_NO_GAME;
        }

        send_message_printf(bot,
                            info->chat_id,
                            info->message_id,
                            response);
}

static void
process_help(struct pcx_bot *bot,
             const struct message_info *info)
{
        struct game *running_game = find_game(bot, info->chat_id);

        if (running_game != NULL) {
                show_help(bot,
                          running_game->type,
                          info->chat_id,
                          info->message_id);
                return;
        }

        send_game_question_reply(bot,
                                 PCX_TEXT_STRING_WHICH_HELP,
                                 "help",
                                 info);
}

static bool
is_command_str(const char *text,
               int64_t length,
               const char *name)
{
        return length == strlen(name) && !memcmp(text, name, length);
}

static bool
is_command(struct pcx_bot *bot,
           const char *text,
           int64_t length,
           enum pcx_text_string string)
{
        return is_command_str(text,
                              length,
                              pcx_text_get(bot->bot_config->language, string));

}

static bool
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
                return false;

        if (offset < 0 || length < 1 || offset + length > strlen(info->text))
                return false;

        if (strcmp(type, "bot_command"))
                return false;

        const char *at = memchr(info->text + offset, '@', length);

        if (at) {
                size_t botname_len = strlen(bot->bot_config->botname);
                if (info->text + offset + length - at - 1 != botname_len ||
                    memcmp(at + 1, bot->bot_config->botname, botname_len)) {
                        return false;
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
                { PCX_TEXT_STRING_CANCEL_COMMAND, process_cancel },
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(commands); i++) {
                if (!is_command(bot,
                                info->text + offset, length,
                                commands[i].name))
                        continue;

                commands[i].func(bot, info);

                return true;
        }

        /* Make /start run the start command regardless of the actual
         * translation of the command name. Telegram encourages you to
         * type /start in all of the bots so it’s useful if that does
         * something in all bots.
         */
        if (is_command_str(info->text + offset, length, "/start")) {
                process_start(bot, info);
                return true;
        }

        for (unsigned i = 0; pcx_game_list[i]; i++) {
                if (!can_run_game(bot, pcx_game_list[i]))
                        continue;

                if (!is_command(bot,
                                info->text + offset, length,
                                pcx_game_list[i]->start_command))
                        continue;

                process_create_game(bot, pcx_game_list[i], info);

                return true;
        }

        return false;
}

static bool
process_entities(struct pcx_bot *bot,
                 struct json_object *message,
                 const struct message_info *info)
{
        struct json_object *entities;

        if (!get_fields(message,
                        "entities", json_type_array, &entities,
                        NULL))
                return false;

        bool ret = false;

        for (unsigned i = 0; i < json_object_array_length(entities); i++) {
                struct json_object *entity =
                        json_object_array_get_idx(entities, i);

                if (process_entity(bot, entity, info))
                        ret = true;
        }

        return ret;
}

static void
process_game_message(struct pcx_bot *bot,
                     const struct message_info *info)
{
        struct game *game = find_game(bot, info->chat_id);

        if (game == NULL)
                return;

        if (game->type->handle_message_cb == NULL)
                return;

        if (game->game == NULL)
                return;

        int player_num = find_player_in_game(game, info->from_id);

        if (player_num == -1)
                return;

        const char *text = info->text;

        /* If the message is directed at someone, then only pay
         * attention to it if it’s directed at the bot. In that case
         * we will also strip off the bot username from the message.
         * This provides a way to interact with the bot even when it
         * isn’t an admin.
         */
        if (*text == '@') {
                text++;

                size_t botname_len = strlen(bot->bot_config->botname);

                if (strncmp(text, bot->bot_config->botname, botname_len))
                        return;

                text += botname_len;

                if (*text != ' ' && *text != '\n')
                        return;

                text++;
        }

        set_game_timeout(game);

        game->type->handle_message_cb(game->game,
                                      player_num,
                                      text);
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

        if (!process_entities(bot, message, &info))
                process_game_message(bot, &info);
}

static bool
get_game_callback_data(struct pcx_bot *bot,
                       struct json_object *callback,
                       const char *callback_data,
                       const struct pcx_game **game,
                       struct message_info *info)
{
        for (unsigned i = 0; pcx_game_list[i]; i++) {
                if (!strcmp(pcx_game_list[i]->name, callback_data)) {
                        *game = pcx_game_list[i];
                        goto found_game;
                }
        }

        return false;

found_game: (void) 0;

        if (!can_run_game(bot, *game))
                return false;

        struct json_object *message;

        bool ret = get_fields(callback,
                              "message", json_type_object, &message,
                              NULL);
        if (!ret)
                return false;

        if (!get_message_info(message, info))
                return false;

        return true;
}

static void
process_help_callback_data(struct pcx_bot *bot,
                           struct json_object *callback,
                           const char *callback_data)
{
        const struct pcx_game *game;
        struct message_info info;

        if (!get_game_callback_data(bot,
                                    callback,
                                    callback_data,
                                    &game,
                                    &info))
                return;

        delete_message(bot, info.chat_id, info.message_id);

        show_help(bot, game, info.chat_id, -1 /* message_id */);
}

static void
process_create_game_callback_data(struct pcx_bot *bot,
                                  struct json_object *callback,
                                  const char *callback_data)
{
        const struct pcx_game *game;
        struct message_info info;

        if (!get_game_callback_data(bot,
                                    callback,
                                    callback_data,
                                    &game,
                                    &info))
                return;

        struct json_object *from;

        bool ret = get_fields(callback,
                              "from", json_type_object, &from,
                              NULL);

        if (!ret)
                return;

        if (!get_message_from_info(from, &info))
                return;

        remove_buttons_from_message(bot, info.chat_id, info.message_id);

        process_create_game(bot, game, &info);
}

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

static bool
has_callback_prefix(const char *callback_data,
                    const char *prefix,
                    const char **extra_data)
{
        int prefix_len = strlen(prefix);

        if (strlen(callback_data) > prefix_len &&
            !memcmp(callback_data, prefix, prefix_len) &&
            callback_data[prefix_len] == ':') {
                *extra_data = callback_data + prefix_len + 1;
                return true;
        } else {
                return false;
        }
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

        const char *extra_data;

        if (has_callback_prefix(callback_data, "help", &extra_data)) {
                process_help_callback_data(bot, callback, extra_data);
                return;
        }

        if (has_callback_prefix(callback_data, "creategame", &extra_data)) {
                process_create_game_callback_data(bot, callback, extra_data);
                return;
        }

        struct game *game;
        int player_num;

        if (find_player(bot, from_id, &game, &player_num) && game->game) {
                set_game_timeout(game);
                game->type->handle_callback_data_cb(game->game,
                                                    player_num,
                                                    callback_data);
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
        if (!ret || !ok) {
                pcx_log("getUpdates request failed: %s",
                        json_object_to_json_string(obj));
                return false;
        }

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
                if (ret) {
                        return size * nmemb;
                } else {
                        pcx_log("Error processing updates");
                        return 0;
                }
        }

        enum json_tokener_error error =
                json_tokener_get_error(bot->tokener);

        if (error == json_tokener_continue) {
                return size * nmemb;
        } else {
                pcx_log("Invalid JSON received in updates");
                return 0;
        }
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

static void
add_command_description(struct pcx_bot *bot,
                        struct json_object *commands,
                        enum pcx_text_string command,
                        enum pcx_text_string desc)
{
        struct json_object *obj = json_object_new_object();

        const char *command_text = pcx_text_get(bot->bot_config->language,
                                                command);
        if (command_text[0] == '/')
                command_text++;

        struct json_object *command_str =
                json_object_new_string(command_text);

        json_object_object_add(obj, "command", command_str);

        struct json_object *desc_str =
                json_object_new_string(pcx_text_get(bot->bot_config->language,
                                                    desc));
        json_object_object_add(obj, "description", desc_str);

        json_object_array_add(commands, obj);
}

static void
queue_update_commands_request(struct pcx_bot *bot)
{
        struct json_object *commands = json_object_new_array();

        for (unsigned i = 0; pcx_game_list[i]; i++) {
                const struct pcx_game *game = pcx_game_list[i];

                if (!can_run_game(bot, game))
                        continue;

                add_command_description(bot,
                                        commands,
                                        game->start_command,
                                        game->start_command_description);
        }

        static const struct {
                enum pcx_text_string command;
                enum pcx_text_string desc;
        } command_descs[] = {
                {
                        PCX_TEXT_STRING_JOIN_COMMAND,
                        PCX_TEXT_STRING_JOIN_COMMAND_DESCRIPTION
                },
                {
                        PCX_TEXT_STRING_START_COMMAND,
                        PCX_TEXT_STRING_START_COMMAND_DESCRIPTION
                },
                {
                        PCX_TEXT_STRING_CANCEL_COMMAND,
                        PCX_TEXT_STRING_CANCEL_COMMAND_DESCRIPTION
                },
                {
                        PCX_TEXT_STRING_HELP_COMMAND,
                        PCX_TEXT_STRING_HELP_COMMAND_DESCRIPTION
                },
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(command_descs); i++) {
                add_command_description(bot,
                                        commands,
                                        command_descs[i].command,
                                        command_descs[i].desc);
        }

        struct json_object *args = json_object_new_object();

        json_object_object_add(args, "commands", commands);

        send_request(bot, "setMyCommands", args);

        json_object_put(args);
}

static char *
get_url_base(const struct pcx_config *config,
             const struct pcx_config_bot *bot_config)
{
        struct pcx_buffer url = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&url,
                                 config->telegram_url ?
                                 config->telegram_url :
                                 "https://api.telegram.org/");

        if (url.length == 0 || url.data[url.length - 1] != '/')
                pcx_buffer_append_c(&url, '/');

        pcx_buffer_append_string(&url, "bot");
        pcx_buffer_append_string(&url, bot_config->apikey);
        pcx_buffer_append_string(&url, "/");

        return (char *) url.data;
}

struct pcx_bot *
pcx_bot_new(const struct pcx_config *config,
            const struct pcx_config_bot *bot_config,
            struct pcx_class_store *class_store,
            struct pcx_curl_multi *pcurl)
{
        struct pcx_bot *bot = pcx_calloc(sizeof *bot);

        pcx_list_init(&bot->queued_requests);
        pcx_list_init(&bot->games);

        bot->message_queue = pcx_message_queue_new();

        pcx_buffer_init(&bot->known_ids);

        bot->config = config;
        bot->bot_config = bot_config;

        bot->class_store = class_store;

        load_known_ids(bot);

        bot->tokener = json_tokener_new();

        bot->url_base = get_url_base(config, bot_config);

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

        queue_update_commands_request(bot);

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
                remove_game(bot, game);
        }
}

int
pcx_bot_get_n_running_games(struct pcx_bot *bot)
{
        return pcx_list_length(&bot->games);
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

        pcx_message_queue_free(bot->message_queue);
        remove_message_delay_source(bot);

        curl_slist_free_all(bot->content_type_headers);

        if (bot->tokener)
                json_tokener_free(bot->tokener);

        if (bot->request_tokener)
                json_tokener_free(bot->request_tokener);

        free_games(bot);

        remove_restart_updates_source(bot);

        if (bot->start_request_source)
                pcx_main_context_remove_source(bot->start_request_source);

        pcx_buffer_destroy(&bot->known_ids);

        pcx_free(bot->url_base);

        pcx_free(bot);
}
