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

#include "config.h"

#include "pcx-tty-game.h"

#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include "pcx-game.h"
#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"

struct pcx_error_domain
pcx_tty_game_error;

struct pcx_tty_game_player {
        struct pcx_main_context_source *source;
        struct pcx_buffer buffer;
        int fd;
};

struct pcx_tty_game {
        const struct pcx_game *game;
        void *game_data;
        int n_players;
        struct pcx_tty_game_player *players;
        struct pcx_buffer buffer;
};

static void
send_message_cb(const struct pcx_game_message *message,
                void *user_data)
{
        struct pcx_tty_game *game = user_data;

        assert(message->target >= -1 && message->target < game->n_players);

        int fd;

        if (message->target == -1)
                fd = STDOUT_FILENO;
        else
                fd = game->players[message->target].fd;

        pcx_buffer_set_length(&game->buffer, 0);
        pcx_buffer_append_string(&game->buffer, message->text);
        pcx_buffer_append_c(&game->buffer, '\n');

        if (message->n_buttons > 0) {
                pcx_buffer_append_c(&game->buffer, '\n');

                for (unsigned i = 0; i < message->n_buttons; i++) {
                        pcx_buffer_append_printf(&game->buffer,
                                                 "%s) %s\n",
                                                 message->buttons[i].data,
                                                 message->buttons[i].text);
                }
        }

        size_t wrote = 0;

        while (wrote < game->buffer.length) {
                ssize_t w = write(fd,
                                  game->buffer.data + wrote,
                                  game->buffer.length - wrote);
                if (w <= 0)
                        break;
                wrote += w;
        }
}

static void
game_over_cb(void *user_data)
{
}

static const struct pcx_game_callbacks
callbacks = {
        .send_message = send_message_cb,
        .game_over = game_over_cb,
};

static void
poll_cb(struct pcx_main_context_source *source,
        int fd,
        enum pcx_main_context_poll_flags flags,
        void *user_data)
{
        struct pcx_tty_game *game = user_data;
        int player_num;

        for (player_num = 0; player_num < game->n_players; player_num++) {
                if (game->players[player_num].fd == fd)
                        goto found_player_num;
        }

        pcx_fatal("Unexpected FD in poll_cb");

found_player_num: (void) 0;

        struct pcx_tty_game_player *player = game->players + player_num;

        pcx_buffer_ensure_size(&player->buffer, player->buffer.length + 128);

        ssize_t got = read(player->fd,
                           player->buffer.data + player->buffer.length,
                           player->buffer.size - player->buffer.length);

        if (got <= 0) {
                pcx_main_context_remove_source(player->source);
                player->source = NULL;
                return;
        }

        player->buffer.length += got;

        size_t processed = 0;

        while (true) {
                uint8_t *end = memchr(player->buffer.data + processed,
                                      '\n',
                                      player->buffer.length - processed);
                if (end == NULL)
                        break;

                uint8_t *data_end = end;

                while (data_end > player->buffer.data + processed &&
                       (data_end[-1] == '\r' || data_end[-1] == ' ')) {
                        data_end--;
                }

                *data_end = '\0';

                game->game->handle_callback_data_cb(game->game_data,
                                                    player_num,
                                                    (const char *)
                                                    player->buffer.data +
                                                    processed);

                processed = end - player->buffer.data + 1;
        }

        memmove(player->buffer.data,
                player->buffer.data + processed,
                player->buffer.length - processed);
        player->buffer.length -= processed;
}

static const char *
get_basename(const char *filename)
{
        const char *bn = filename + strlen(filename);

        while (bn > filename && bn[-1] != '/' && bn[-1] != '\\')
                bn--;

        return bn;
}

struct pcx_tty_game *
pcx_tty_game_new(const struct pcx_config *config,
                 int n_players,
                 const char * const *files,
                 struct pcx_error **error)
{
        struct pcx_tty_game *game = pcx_calloc(sizeof *game);

        game->game = pcx_game_list[0];

        assert(n_players > 0 && n_players <= game->game->max_players);

        pcx_buffer_init(&game->buffer);

        game->n_players = n_players;
        game->players = pcx_calloc(n_players *
                                   sizeof (struct pcx_tty_game_player));

        for (unsigned i = 0; i < n_players; i++) {
                game->players[i].fd = -1;
                pcx_buffer_init(&game->players[i].buffer);
        }

        for (unsigned i = 0; i < n_players; i++) {
                game->players[i].fd = open(files[i], O_RDWR);
                if (game->players[i].fd == -1) {
                        pcx_set_error(error,
                                      &pcx_tty_game_error,
                                      PCX_TTY_GAME_ERROR_IO,
                                      "Error opening TTY: %s: %s",
                                      files[i],
                                      strerror(errno));
                        goto error;
                }

                game->players[i].source =
                        pcx_main_context_add_poll(NULL,
                                                  game->players[i].fd,
                                                  PCX_MAIN_CONTEXT_POLL_IN |
                                                  PCX_MAIN_CONTEXT_POLL_ERROR,
                                                  poll_cb,
                                                  game);
        }

        const char **names = pcx_alloc(n_players * sizeof *names);

        for (unsigned i = 0; i < n_players; i++)
                names[i] = get_basename(files[i]);

        game->game_data =
                game->game->create_game_cb(config,
                                           &callbacks,
                                           game,
                                           PCX_TEXT_LANGUAGE_ESPERANTO,
                                           n_players,
                                           names);

        pcx_free(names);

        return game;

error:
        pcx_tty_game_free(game);
        return NULL;
}

void
pcx_tty_game_free(struct pcx_tty_game *game)
{
        if (game->game_data)
                game->game->free_game_cb(game->game_data);

        for (unsigned i = 0; i < game->n_players; i++) {
                if (game->players[i].fd != -1)
                        pcx_close(game->players[i].fd);
                if (game->players[i].source)
                        pcx_main_context_remove_source(game->players[i].source);
                pcx_buffer_destroy(&game->players[i].buffer);
        }

        pcx_free(game->players);

        pcx_buffer_destroy(&game->buffer);

        pcx_free(game);
}
