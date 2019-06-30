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

#ifndef PCX_GAME_H
#define PCX_GAME_H

#include <stddef.h>

#include "pcx-text.h"

#define PCX_GAME_MAX_PLAYERS 6

struct pcx_game;

enum pcx_game_message_format {
        PCX_GAME_MESSAGE_FORMAT_PLAIN,
        PCX_GAME_MESSAGE_FORMAT_HTML
};

struct pcx_game_button {
        const char *text;
        const char *data;
};

struct pcx_game_callbacks {
        void (* send_private_message)(int user_num,
                                      enum pcx_game_message_format format,
                                      const char *message,
                                      size_t n_buttons,
                                      const struct pcx_game_button *buttons,
                                      void *user_data);
        void (* send_message)(enum pcx_game_message_format format,
                              const char *message,
                              size_t n_buttons,
                              const struct pcx_game_button *buttons,
                              void *user_data);
        void (* game_over)(void *user_data);
};

struct pcx_game *
pcx_game_new(const struct pcx_game_callbacks *callbacks,
             void *user_data,
             enum pcx_text_language language,
             int n_players,
             const char * const *names);

void
pcx_game_handle_callback_data(struct pcx_game *game,
                              int player_num,
                              const char *callback_data);

void
pcx_game_free(struct pcx_game *game);

#endif /* PCX_GAME_H */
