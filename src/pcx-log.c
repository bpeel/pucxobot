/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2011, 2013, 2020  Neil Roberts
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

#include "pcx-log.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#include "pcx-buffer.h"

static FILE *pcx_log_file = NULL;
static struct pcx_buffer pcx_log_buffer = PCX_BUFFER_STATIC_INIT;
static pthread_t pcx_log_thread;
static bool pcx_log_has_thread = false;
static pthread_mutex_t pcx_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pcx_log_cond = PTHREAD_COND_INITIALIZER;
static bool pcx_log_finished = false;

struct pcx_error_domain
pcx_log_error;

void
pcx_log(const char *format, ...)
{
        va_list ap;
        time_t now;
        struct tm tm;

        pthread_mutex_lock(&pcx_log_mutex);

        time(&now);
        gmtime_r(&now, &tm);

        pcx_buffer_append_printf(&pcx_log_buffer,
                                 "[%4d-%02d-%02dT%02d:%02d:%02dZ] ",
                                 tm.tm_year + 1900,
                                 tm.tm_mon + 1,
                                 tm.tm_mday,
                                 tm.tm_hour,
                                 tm.tm_min,
                                 tm.tm_sec);

        va_start(ap, format);
        pcx_buffer_append_vprintf(&pcx_log_buffer, format, ap);
        va_end(ap);

        pcx_buffer_append_c(&pcx_log_buffer, '\n');

        pthread_cond_signal(&pcx_log_cond);

        pthread_mutex_unlock(&pcx_log_mutex);
}

static void
block_sigint(void)
{
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        sigaddset(&sigset, SIGTERM);

        if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) == -1)
                pcx_warning("pthread_sigmask failed: %s", strerror(errno));
}

static void *
pcx_log_thread_func(void *data)
{
        struct pcx_buffer alternate_buffer;
        struct pcx_buffer tmp;
        bool had_error = false;

        block_sigint();

        pcx_buffer_init(&alternate_buffer);

        pthread_mutex_lock(&pcx_log_mutex);

        while (!pcx_log_finished || pcx_log_buffer.length > 0) {
                size_t wrote;

                /* Wait until there's something to do */
                while (!pcx_log_finished && pcx_log_buffer.length == 0)
                        pthread_cond_wait(&pcx_log_cond, &pcx_log_mutex);

                if (had_error) {
                        /* Just ignore the data */
                        pcx_buffer_set_length(&pcx_log_buffer, 0);
                } else {
                        /* Swap the log buffer for an empty alternate
                           buffer so we can write from the normal
                           one */
                        tmp = pcx_log_buffer;
                        pcx_log_buffer = alternate_buffer;
                        alternate_buffer = tmp;

                        /* Release the mutex while we do a blocking write */
                        pthread_mutex_unlock(&pcx_log_mutex);

                        wrote = fwrite(alternate_buffer.data, 1 /* size */ ,
                                       alternate_buffer.length, pcx_log_file);

                        /* If there was an error then we'll just start
                           ignoring data until we're told to quit */
                        if (wrote != alternate_buffer.length)
                                had_error = true;
                        else
                                fflush(pcx_log_file);

                        pcx_buffer_set_length(&alternate_buffer, 0);

                        pthread_mutex_lock(&pcx_log_mutex);
                }
        }

        pthread_mutex_unlock(&pcx_log_mutex);

        pcx_buffer_destroy(&alternate_buffer);

        return NULL;
}

bool
pcx_log_set_file(const char *filename, struct pcx_error **error)
{
        FILE *file;

        file = fopen(filename, "a");

        if (file == NULL) {
                pcx_set_error(error,
                              &pcx_log_error,
                              PCX_LOG_ERROR_FILE,
                              "%s: %s",
                              filename,
                              strerror(errno));
                return false;
        }

        pcx_log_close();

        pcx_log_file = file;
        pcx_log_finished = false;

        return true;
}

void
pcx_log_start(void)
{
        if (pcx_log_has_thread)
                return;

        if (pcx_log_file == NULL)
                pcx_log_file = stdout;

        int res = pthread_create(&pcx_log_thread,
                                 NULL, /* attr */
                                 pcx_log_thread_func,
                                 NULL /* thread func arg */);

        if (res) {
                pcx_fatal("Error creating thread: %s",
                          strerror(res));
        }

        pcx_log_has_thread = true;
}

void
pcx_log_close(void)
{
        if (pcx_log_has_thread) {
                pthread_mutex_lock(&pcx_log_mutex);
                pcx_log_finished = true;
                pthread_cond_signal(&pcx_log_cond);
                pthread_mutex_unlock(&pcx_log_mutex);

                pthread_join(pcx_log_thread, NULL);

                pcx_log_has_thread = false;
        }

        pcx_buffer_destroy(&pcx_log_buffer);
        pcx_buffer_init(&pcx_log_buffer);

        if (pcx_log_file && pcx_log_file != stdout) {
                fclose(pcx_log_file);
                pcx_log_file = NULL;
        }
}
