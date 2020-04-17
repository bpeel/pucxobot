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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <curl/curl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#include "pcx-tty-game.h"
#include "pcx-bot.h"
#include "pcx-server.h"
#include "pcx-game.h"
#include "pcx-main-context.h"
#include "pcx-config.h"
#include "pcx-curl-multi.h"
#include "pcx-log.h"

struct tty_file {
        struct pcx_list link;
        const char *name;
};

struct pcx_main {
        struct pcx_tty_game *tty_game;
        struct pcx_curl_multi *pcurl;

        size_t n_bots;
        struct pcx_bot **bots;

        size_t n_servers;
        struct pcx_server **servers;

        struct pcx_config *config;

        /* List of const char* pointers. The strings are assumed to
         * come from the command line arguments and are not freed.
         */
        struct pcx_buffer tty_files;
        const char *config_filename;
        const char *log_filename;
        bool daemonize;
        bool curl_inited;
        bool quit;
        int lock_fd;
};

static const char options[] = "-ht:l:c:d";

static void
quit_cb(struct pcx_main_context_source *source,
        int signal_num,
        void *user_data)
{
        struct pcx_main *data = user_data;

        data->quit = true;
}

static void
info_cb(struct pcx_main_context_source *source,
        int signal_num,
        void *user_data)
{
        struct pcx_main *data = user_data;
        int total_games = 0;
        struct pcx_config_bot *bot;
        int bot_num = 0;

        pcx_list_for_each(bot, &data->config->bots, link) {
                int n_games = pcx_bot_get_n_running_games(data->bots[bot_num]);
                pcx_log("@%s: %i", bot->botname, n_games);
                total_games += n_games;
                bot_num++;
        }

        pcx_log("Total games: %i", total_games);

        if (signal_num == SIGUSR2) {
                if (total_games == 0) {
                        pcx_log("No games running, quitting");
                        data->quit = true;
                } else {
                        pcx_log("Not quitting due to running games");
                }
        }
}

static bool
init_main_tty(struct pcx_main *data)
{
        struct pcx_error *error = NULL;
        data->tty_game = pcx_tty_game_new(data->config,
                                          data->tty_files.length /
                                          sizeof (const char *),
                                          (const char *const *)
                                          data->tty_files.data,
                                          &error);

        if (data->tty_game == NULL) {
                pcx_log("%s", error->message);
                pcx_error_free(error);
                return false;
        }

        return true;
}

static void
init_main_bots(struct pcx_main *data)
{
        if (pcx_list_empty(&data->config->bots))
                return;

        curl_global_init(CURL_GLOBAL_ALL);
        data->curl_inited = true;

        data->pcurl = pcx_curl_multi_new();

        struct pcx_config_bot *bot;

        pcx_list_for_each(bot, &data->config->bots, link) {
                data->n_bots++;
        }

        data->bots = pcx_alloc((sizeof *data->bots) * MAX(data->n_bots, 1));

        unsigned i = 0;

        pcx_list_for_each(bot, &data->config->bots, link) {
                data->bots[i++] = pcx_bot_new(data->config,
                                              bot,
                                              data->pcurl);
        }
}

static bool
init_main_servers(struct pcx_main *data)
{
        struct pcx_config_server *server_conf;
        size_t n_servers = 0;

        pcx_list_for_each(server_conf, &data->config->servers, link) {
                n_servers++;
        }

        data->servers = pcx_alloc((sizeof *data->servers) *
                                  MAX(n_servers, 1));

        pcx_list_for_each(server_conf, &data->config->servers, link) {
                struct pcx_error *error = NULL;
                struct pcx_server *server =
                        pcx_server_new(data->config, server_conf, &error);

                if (server == NULL) {
                        fprintf(stderr, "%s\n", error->message);
                        pcx_error_free(error);
                        return false;
                }

                data->servers[data->n_servers++] = server;
        }

        return true;
}

static void
destroy_main(struct pcx_main *data)
{
        if (data->tty_game)
                pcx_tty_game_free(data->tty_game);

        for (unsigned i = 0; i < data->n_bots; i++)
                pcx_bot_free(data->bots[i]);
        pcx_free(data->bots);

        for (unsigned i = 0; i < data->n_servers; i++)
                pcx_server_free(data->servers[i]);
        pcx_free(data->servers);

        pcx_buffer_destroy(&data->tty_files);

        if (data->pcurl)
                pcx_curl_multi_free(data->pcurl);
        if (data->curl_inited)
                curl_global_cleanup();
        if (data->config)
                pcx_config_free(data->config);

        if (data->lock_fd != -1)
                close(data->lock_fd);
}

static bool
set_log_file(const char *filename)
{
        struct pcx_error *error = NULL;

        if (!pcx_log_set_file(filename, &error)) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                return false;
        }

        return true;
}

static void
usage(void)
{
        printf("Pucxobot - a Telegram robot to play games\n"
               "usage: pucxobot [options]...\n"
               " -h                   Show this help message\n"
               " -t <file>            Specify a TTY file to listen on instead\n"
               "                      of running the Telegram bot.\n"
               " -l <file>            Specify a log file. Defaults to stdout.\n"
               " -c <file>            Specify a config file. Defaults to\n"
               "                      ~/.pucxobot/conf.txt\n"
               " -d                   Fork and detach from terminal\n"
               "                      (Daemonize)\n");
}

static bool
process_arguments(struct pcx_main *data,
                  int argc, char **argv)
{
        int opt;

        opterr = false;

        while ((opt = getopt(argc, argv, options)) != -1) {
                switch (opt) {
                case ':':
                case '?':
                        fprintf(stderr,
                                "invalid option '%c'\n",
                                optopt);
                        return false;

                case '\1':
                        fprintf(stderr,
                                "unexpected argument \"%s\"\n",
                                optarg);
                        return false;

                case 't':
                        pcx_buffer_append(&data->tty_files,
                                          &optarg,
                                          sizeof optarg);
                        break;

                case 'h':
                        usage();
                        return false;

                case 'l':
                        data->log_filename = optarg;
                        break;

                case 'c':
                        data->config_filename = optarg;
                        break;

                case 'd':
                        data->daemonize = true;
                        break;
                }
        }

        return true;
}

static bool
load_config(struct pcx_main *data)
{
        struct pcx_error *error = NULL;

        if (data->config_filename) {
                data->config = pcx_config_load(data->config_filename, &error);
        } else {
                const char *home = getenv("HOME");

                if (home == NULL) {
                        fprintf(stderr,
                                "HOME environment variable is not set\n");
                        return false;
                }

                char *filename = pcx_strconcat(home,
                                               "/.pucxobot/conf.txt",
                                               NULL);
                data->config = pcx_config_load(filename, &error);
                pcx_free(filename);
        }

        if (data->config == NULL) {
                fprintf(stderr, "%s\n", error->message);
                pcx_error_free(error);
                return false;
        }

        return true;
}

static bool
start_log(struct pcx_main *data)
{
        if (data->log_filename) {
                if (!set_log_file(data->log_filename))
                        return false;
        } else if (data->config->log_file) {
                if (!set_log_file(data->config->log_file))
                        return false;
        } else if (data->daemonize) {
                char *filename = pcx_strconcat(data->config->data_dir,
                                               "/log.txt",
                                               NULL);
                bool ret = set_log_file(filename);
                pcx_free(filename);
                if (!ret)
                        return false;
        }

        pcx_log_start();

        return true;
}

static bool
check_not_already_running(struct pcx_main *data)
{
        char *filename = pcx_strconcat(data->config->data_dir,
                                       "/pucxobot-lock",
                                       NULL);

        data->lock_fd = open(filename, O_APPEND | O_CREAT, 0666);

        pcx_free(filename);

        if (data->lock_fd == -1) {
                fprintf(stderr,
                        "error opening lock file: %s\n",
                        strerror(errno));
                return false;
        }

        int ret = flock(data->lock_fd, LOCK_EX | LOCK_NB);

        if (ret == -1) {
                if (errno == EWOULDBLOCK) {
                        fprintf(stderr,
                                "pucxobot is already running\n");
                } else {
                        fprintf(stderr,
                                "error getting file lock: %s\n",
                                strerror(errno));
                }

                return false;
        }

        return true;
}

static void
daemonize(void)
{
        pid_t pid, sid;

        pid = fork();

        if (pid < 0) {
                pcx_warning("fork failed: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if (pid > 0) {
                /* Parent process, we can just quit */
                exit(EXIT_SUCCESS);
        }

        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                pcx_warning("setsid failed: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        /* Change the working directory so we're resilient against it being
           removed */
        if (chdir("/") < 0) {
                pcx_warning("chdir failed: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        /* Redirect standard files to /dev/null */
        stdin = freopen("/dev/null", "r", stdin);
        stdout = freopen("/dev/null", "w", stdout);
        stderr = freopen("/dev/null", "w", stderr);
}

int
main(int argc, char **argv)
{
        struct pcx_main data = {
                .tty_game = NULL,
                .pcurl = NULL,
                .n_bots = 0,
                .bots = NULL,
                .n_servers = 0,
                .servers = NULL,
                .config = NULL,
                .curl_inited = false,
                .quit = false,
                .tty_files = PCX_BUFFER_STATIC_INIT,
                .config_filename = NULL,
                .lock_fd = -1,
        };

        int ret = EXIT_SUCCESS;

        pcx_main_context_get_default();

        if (!process_arguments(&data, argc, argv)) {
                ret = EXIT_FAILURE;
                goto done;
        }

        if (!load_config(&data)) {
                ret = EXIT_FAILURE;
                goto done;
        }

        if (!check_not_already_running(&data)) {
                ret = EXIT_FAILURE;
                goto done;
        }

        if (!init_main_servers(&data)) {
                ret = EXIT_FAILURE;
                goto done;
        }

        if (data.daemonize)
                daemonize();

        if (!start_log(&data)) {
                ret = EXIT_FAILURE;
                goto done;
        }

        if (data.tty_files.length > 0) {
                if (!init_main_tty(&data)) {
                        ret = EXIT_FAILURE;
                        goto done;
                }
        } else {
                time_t t;
                time(&t);
                srand(t);

                init_main_bots(&data);
        }

        struct pcx_main_context_source *int_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGINT,
                                                   quit_cb,
                                                   &data);
        struct pcx_main_context_source *term_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGTERM,
                                                   quit_cb,
                                                   &data);
        struct pcx_main_context_source *usr1_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGUSR1,
                                                   info_cb,
                                                   &data);
        struct pcx_main_context_source *usr2_source =
                pcx_main_context_add_signal_source(NULL,
                                                   SIGUSR2,
                                                   info_cb,
                                                   &data);

        do
                pcx_main_context_poll(NULL);
        while (!data.quit);

        pcx_main_context_remove_source(usr2_source);
        pcx_main_context_remove_source(usr1_source);
        pcx_main_context_remove_source(term_source);
        pcx_main_context_remove_source(int_source);

done:
        destroy_main(&data);
        pcx_log_close();

        pcx_main_context_free(pcx_main_context_get_default());

        return ret;
}
