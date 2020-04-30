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
#include <stdint.h>

#include "pcx-text.h"
#include "pcx-config.h"

#define PCX_GAME_MAX_PLAYERS 6

enum pcx_game_message_format {
        PCX_GAME_MESSAGE_FORMAT_PLAIN,
        PCX_GAME_MESSAGE_FORMAT_HTML
};

struct pcx_game_button {
        const char *text;
        const char *data;
};

struct pcx_game_message {
        /* The target player to send to or -1 to send it as a public
         * group message.
         */
        int target;

        const char *text;

        enum pcx_game_message_format format;

        size_t n_buttons;
        const struct pcx_game_button *buttons;
        /* A mask of players that will see the buttons. All other
         * players will just see the message with no buttons. This is
         * useful for global messages that don’t have sensitive
         * information but are asking a question for just one player.
         * This doesn’t work in all of the backends.
         */
        uint32_t button_players;
};

/* The default parameters for a message so that callers don’t have to
 * understand all of them.
 */
#define PCX_GAME_DEFAULT_MESSAGE {                                      \
                .target = -1,                                           \
                .format = PCX_GAME_MESSAGE_FORMAT_PLAIN,                \
                .n_buttons = 0,                                         \
                .button_players = UINT32_MAX                            \
        }

struct pcx_game_callbacks {
        void (* send_message)(const struct pcx_game_message *message,
                              void *user_data);
        void (* game_over)(void *user_data);
};

struct pcx_game {
        const char *name;
        enum pcx_text_string name_string;
        enum pcx_text_string start_command;
        enum pcx_text_string start_command_description;
        int min_players;
        int max_players;
        bool needs_private_messages;
        void *(* create_game_cb)(const struct pcx_config *config,
                                 const struct pcx_game_callbacks *callbacks,
                                 void *user_data,
                                 enum pcx_text_language language,
                                 int n_players,
                                 const char * const *names);
        char *(* get_help_cb)(enum pcx_text_language language);
        void (* handle_callback_data_cb)(void *game,
                                         int player_num,
                                         const char *callback_data);
        void (* free_game_cb)(void *game);
};

/* Null terminated list of games */
extern const struct pcx_game * const
pcx_game_list[];

#endif /* PCX_GAME_H */
