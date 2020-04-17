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

#include "pcx-config.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "pcx-util.h"
#include "pcx-key-value.h"
#include "pcx-buffer.h"

struct pcx_error_domain
pcx_config_error;

struct load_config_data {
        const char *filename;
        struct pcx_config *config;
        bool had_error;
        struct pcx_buffer error_buffer;
        struct pcx_config_bot *bot;
        struct pcx_config_server *server;
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
        OPTION_TYPE_INT,
        OPTION_TYPE_LANGUAGE_CODE,
};

struct option {
        const char *key;
        size_t offset;
        enum option_type type;
};

static const struct option
bot_options[] = {
#define OPTION(name, type)                              \
        {                                               \
                #name,                                  \
                offsetof(struct pcx_config_bot, name),  \
                OPTION_TYPE_ ## type,                   \
        }
        OPTION(apikey, STRING),
        OPTION(botname, STRING),
        OPTION(announce_channel, STRING),
        OPTION(language, LANGUAGE_CODE),
#undef OPTION
};

static const struct option
server_options[] = {
#define OPTION(name, type)                                      \
        {                                                       \
                #name,                                          \
                offsetof(struct pcx_config_server, name),       \
                OPTION_TYPE_ ## type,                           \
        }
        OPTION(address, STRING),
#undef OPTION
};

static const struct option
general_options[] = {
#define OPTION(name, type)                              \
        {                                               \
                #name,                                  \
                offsetof(struct pcx_config, name),      \
                OPTION_TYPE_ ## type,                   \
        }
        OPTION(data_dir, STRING),
        OPTION(log_file, STRING),
#undef OPTION
};

static void
set_option(struct load_config_data *data,
           void *config_item,
           const struct option *option,
           const char *value)
{
        switch (option->type) {
        case OPTION_TYPE_STRING: {
                char **ptr = (char **) ((uint8_t *) config_item +
                                        option->offset);
                if (*ptr) {
                        load_config_error(data,
                                          "%s specified twice",
                                          option->key);
                } else {
                        *ptr = pcx_strdup(value);
                }
                break;
        }
        case OPTION_TYPE_LANGUAGE_CODE: {
                enum pcx_text_language *ptr =
                        (enum pcx_text_language *)
                        ((uint8_t *) config_item + option->offset);
                if (!pcx_text_lookup_language(value, ptr)) {
                        load_config_error(data,
                                          "invalid language: %s",
                                          value);
                }
                break;
        }
        case OPTION_TYPE_INT: {
                int64_t *ptr = (int64_t *) ((uint8_t *) config_item +
                                            option->offset);
                errno = 0;
                char *tail;
                *ptr = strtoll(value, &tail, 10);
                if (errno || *tail) {
                        load_config_error(data,
                                          "invalid value for %s",
                                          option->key);
                }
                break;
        }
        }
}

static void
set_from_options(struct load_config_data *data,
                 void *config_item,
                 size_t n_options,
                 const struct option *options,
                 const char *key,
                 const char *value)
{
        for (unsigned i = 0; i < n_options; i++) {
                if (strcmp(key, options[i].key))
                        continue;

                set_option(data,
                           config_item,
                           options + i,
                           value);
                return;
        }

        load_config_error(data, "unknown config option: %s", key);
}

static void
load_config_func(enum pcx_key_value_event event,
                 int line_number,
                 const char *key,
                 const char *value,
                 void *user_data)
{
        struct load_config_data *data = user_data;

        switch (event) {
        case PCX_KEY_VALUE_EVENT_HEADER:
                if (!strcmp(value, "bot")) {
                        data->bot = pcx_calloc(sizeof *data->bot);
                        data->bot->language = PCX_TEXT_LANGUAGE_ESPERANTO;
                        pcx_list_insert(data->config->bots.prev,
                                        &data->bot->link);
                        data->server = NULL;
                } else if (!strcmp(value, "server")) {
                        data->server = pcx_calloc(sizeof *data->server);
                        pcx_list_insert(data->config->servers.prev,
                                        &data->server->link);
                        data->bot = NULL;
                } else if (!strcmp(value, "general")) {
                        data->bot = NULL;
                        data->server = NULL;
                } else {
                        load_config_error(data, "unknown section: %s", value);
                        data->bot = NULL;
                }
                break;
        case PCX_KEY_VALUE_EVENT_PROPERTY:
                if (data->bot) {
                        set_from_options(data,
                                         data->bot,
                                         PCX_N_ELEMENTS(bot_options),
                                         bot_options,
                                         key,
                                         value);
                } else if (data->server) {
                        set_from_options(data,
                                         data->server,
                                         PCX_N_ELEMENTS(server_options),
                                         server_options,
                                         key,
                                         value);
                } else {
                        set_from_options(data,
                                         data->config,
                                         PCX_N_ELEMENTS(general_options),
                                         general_options,
                                         key,
                                         value);
                }
                break;
        }
}

static bool
validate_bot(struct pcx_config_bot *bot,
             const char *filename,
             struct pcx_error **error)
{
        if (bot->apikey == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: missing apikey option",
                              filename);
                return false;
        }

        if (bot->botname == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: missing botname option",
                              filename);
                return false;
        }

        return true;
}

static bool
validate_server(struct pcx_config_server *server,
                const char *filename,
                struct pcx_error **error)
{
        return true;
}

static bool
validate_config(struct pcx_config *config,
                const char *filename,
                struct pcx_error **error)
{
        bool found_something = false;

        struct pcx_config_bot *bot;

        pcx_list_for_each(bot, &config->bots, link) {
                if (!validate_bot(bot, filename, error))
                        return false;
                found_something = true;
        }

        struct pcx_config_server *server;

        pcx_list_for_each(server, &config->servers, link) {
                if (!validate_server(server, filename, error))
                        return false;
                found_something = true;
        }

        if (!found_something) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: no bots or servers configured",
                              filename);
                return false;
        }

        if (config->data_dir == NULL) {
                const char *home = getenv("HOME");

                if (home == NULL) {
                        pcx_set_error(error,
                                      &pcx_config_error,
                                      PCX_CONFIG_ERROR_IO,
                                      "No data dir specified and HOME "
                                      "env var not set");
                        return false;
                }

                config->data_dir = pcx_strconcat(home, "/.pucxobot", NULL);
        }

        return true;
}

static bool
load_config(const char *fn,
            struct pcx_config *config,
            struct pcx_error **error)
{
        bool ret = true;

        FILE *f = fopen(fn, "r");

        if (f == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: %s",
                              fn,
                              strerror(errno));
                ret = false;
        } else {
                struct load_config_data data = {
                        .filename = fn,
                        .config = config,
                        .had_error = false,
                        .bot = NULL,
                        .server = NULL,
                };

                pcx_buffer_init(&data.error_buffer);

                pcx_key_value_load(f,
                                   load_config_func,
                                   load_config_error_func,
                                   &data);

                if (data.had_error) {
                        pcx_set_error(error,
                                      &pcx_config_error,
                                      PCX_CONFIG_ERROR_IO,
                                      "%s",
                                      (char *) data.error_buffer.data);
                        ret = false;
                } else if (!validate_config(config, fn, error)) {
                        ret = false;
                }

                pcx_buffer_destroy(&data.error_buffer);

                fclose(f);
        }

        return ret;
}

struct pcx_config *
pcx_config_load(const char *filename,
                struct pcx_error **error)
{
        struct pcx_config *config = pcx_calloc(sizeof *config);

        pcx_list_init(&config->bots);
        pcx_list_init(&config->servers);

        if (!load_config(filename, config, error))
                goto error;

        return config;

error:
        pcx_config_free(config);
        return NULL;
}

static void
free_bots(struct pcx_config *config)
{
        struct pcx_config_bot *bot, *tmp;

        pcx_list_for_each_safe(bot, tmp, &config->bots, link) {
                pcx_free(bot->apikey);
                pcx_free(bot->botname);
                pcx_free(bot->announce_channel);
                pcx_free(bot);
        }
}

static void
free_servers(struct pcx_config *config)
{
        struct pcx_config_server *server, *tmp;

        pcx_list_for_each_safe(server, tmp, &config->servers, link) {
                pcx_free(server->address);
                pcx_free(server);
        }

}

void
pcx_config_free(struct pcx_config *config)
{
        free_bots(config);
        free_servers(config);

        pcx_free(config->data_dir);
        pcx_free(config->log_file);

        pcx_free(config);
}
