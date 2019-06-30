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

enum load_config_section {
        SECTION_NONE,
        SECTION_AUTH,
        SECTION_SETUP
} section;

struct load_config_data {
        const char *filename;
        struct pcx_config *config;
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
                char **ptr = (char **) ((uint8_t *) data->config + offset);
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
                int64_t *ptr = (int64_t *) ((uint8_t *) data->config + offset);
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
#define OPTION(section, name, type)                     \
                {                                       \
                        section,                        \
                        #name,                          \
                        offsetof(struct pcx_config, name), \
                        OPTION_TYPE_ ## type,           \
                }
                OPTION(SECTION_AUTH, apikey, STRING),
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
validate_config(struct pcx_config *config,
                const char *filename,
                struct pcx_error **error)
{
        if (config->apikey == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: missing apikey option",
                              filename);
                return false;
        }

        if (config->botname == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "%s: missing botname option",
                              filename);
                return false;
        }

        return true;
}

static char *
get_data_file(const char *name,
              struct pcx_error **error)
{
        const char *home = getenv("HOME");

        if (home == NULL) {
                pcx_set_error(error,
                              &pcx_config_error,
                              PCX_CONFIG_ERROR_IO,
                              "HOME environment variable is not set");
                return NULL;
        }

        return pcx_strconcat(home, "/.pucxobot/", name, NULL);
}

static bool
load_config(struct pcx_config *config,
            struct pcx_error **error)
{
        bool ret = true;

        char *fn = get_data_file("conf.txt", error);

        if (fn == NULL)
                return false;

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
                        .section = SECTION_NONE,
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

        pcx_free(fn);

        return ret;
}

struct pcx_config *
pcx_config_load(struct pcx_error **error)
{
        struct pcx_config *config = pcx_calloc(sizeof *config);

        if (!load_config(config, error))
                goto error;

        return config;

error:
        pcx_config_free(config);
        return NULL;
}

void
pcx_config_free(struct pcx_config *config)
{
        pcx_free(config->apikey);
        pcx_free(config->botname);
        pcx_free(config->announce_channel);

        pcx_free(config);
}
