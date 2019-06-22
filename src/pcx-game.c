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

#include "pcx-game.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "pcx-character.h"
#include "pcx-util.h"
#include "pcx-buffer.h"

#define PCX_GAME_CARDS_PER_CHARACTER 3
#define PCX_GAME_CARDS_PER_PLAYER 2
#define PCX_GAME_TOTAL_CARDS (PCX_CHARACTER_COUNT * \
                              PCX_GAME_CARDS_PER_CHARACTER)
#define PCX_GAME_START_COINS 2

struct pcx_game_card {
        enum pcx_character character;
        bool dead;
};

struct pcx_game_player {
        char *name;
        int coins;
        struct pcx_game_card cards[PCX_GAME_CARDS_PER_PLAYER];
};

struct pcx_game {
        enum pcx_character deck[PCX_GAME_TOTAL_CARDS];
        struct pcx_game_player players[PCX_GAME_MAX_PLAYERS];
        int n_players;
        int n_cards;
        int current_player;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        struct pcx_buffer buffer;
};

static void
send_buffer_message(struct pcx_game *game)
{
        game->callbacks.send_message((const char *) game->buffer.data,
                                     PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                     0, /* n_buttons */
                                     NULL, /* buttons */
                                     game->user_data);
}

static bool
is_alive(const struct pcx_game_player *player)
{
        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                if (!player->cards[i].dead)
                        return true;
        }

        return false;
}

static bool
is_finished(const struct pcx_game *game)
{
        unsigned n_players = 0;

        for (unsigned i = 0; i < game->n_players; i++) {
                if (is_alive(game->players + i))
                        n_players++;
        }

        return n_players <= 1;
}

static void
add_cards_status(struct pcx_buffer *buffer,
                 const struct pcx_game_player *player)
{
        for (unsigned int i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                const struct pcx_game_card *card = player->cards + i;
                enum pcx_character character = card->character;

                if (card->dead) {
                        const char *name = pcx_character_get_name(character);
                        pcx_buffer_append_printf(buffer, "☠%s", name);
                } else {
                        pcx_buffer_append_string(buffer, "🂠");
                }
        }
}

static void
add_money_status(struct pcx_buffer *buffer,
                 const struct pcx_game_player *player)
{
        if (player->coins == 1)
                pcx_buffer_append_string(buffer, "1 monero");
        else
                pcx_buffer_append_printf(buffer, "%i moneroj", player->coins);
}

static void
show_stats(struct pcx_game *game)
{
        bool finished = is_finished(game);
        const struct pcx_game_player *winner = NULL;

        pcx_buffer_set_length(&game->buffer, 0);

        for (unsigned i = 0; i < game->n_players; i++) {
                const struct pcx_game_player *player = game->players + i;
                bool alive = is_alive(player);

                if (finished) {
                        if (alive)
                                pcx_buffer_append_string(&game->buffer, "🏆 ");
                } else if (game->current_player == i) {
                        pcx_buffer_append_string(&game->buffer, "👉 ");
                }

                pcx_buffer_append_string(&game->buffer, player->name);
                pcx_buffer_append_string(&game->buffer, ":\n");

                add_cards_status(&game->buffer, player);

                if (alive) {
                        pcx_buffer_append_string(&game->buffer, ", ");
                        add_money_status(&game->buffer, player);
                        winner = player;
                }

                pcx_buffer_append_string(&game->buffer, "\n\n");
        }

        if (finished) {
                const char *winner_name = winner ? winner->name : "Neniu";
                pcx_buffer_append_printf(&game->buffer,
                                         "%s venkis!",
                                         winner_name);
        } else {
                const struct pcx_game_player *current =
                        game->players + game->current_player;
                pcx_buffer_append_printf(&game->buffer,
                                         "%s, estas via vico, "
                                         "kion vi volas fari?",
                                         current->name);
        }

        send_buffer_message(game);
}

static void
shuffle_deck(struct pcx_game *game)
{
        if (game->n_cards < 2)
                return;

        for (unsigned i = game->n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                enum pcx_character t = game->deck[j];
                game->deck[j] = game->deck[i];
                game->deck[i] = t;
        }
}

static enum pcx_character
take_card(struct pcx_game *game)
{
        assert(game->n_cards > 1);
        return game->deck[--game->n_cards];
}

struct pcx_game *
pcx_game_new(const struct pcx_game_callbacks *callbacks,
             void *user_data,
             int n_players,
             const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_GAME_MAX_PLAYERS);

        struct pcx_game *game = pcx_calloc(sizeof *game);

        game->callbacks = *callbacks;
        game->user_data = user_data;

        game->n_cards = PCX_GAME_TOTAL_CARDS;

        for (unsigned ch = 0; ch < PCX_CHARACTER_COUNT; ch++) {
                for (unsigned c = 0; c < PCX_GAME_CARDS_PER_CHARACTER; c++)
                        game->deck[ch * PCX_GAME_CARDS_PER_CHARACTER + c] = ch;
        }

        shuffle_deck(game);

        game->n_players = n_players;
        game->current_player = rand() % n_players;

        for (unsigned i = 0; i < n_players; i++) {
                game->players[i].coins = PCX_GAME_START_COINS;
                for (unsigned j = 0; j < PCX_GAME_CARDS_PER_PLAYER; j++) {
                        struct pcx_game_card *card = game->players[i].cards + j;
                        card->dead = false;
                        card->character = take_card(game);
                }

                game->players[i].name = pcx_strdup(names[i]);
        }

        show_stats(game);

        return game;
}

void
pcx_game_handle_callback_data(struct pcx_game *game,
                              int player_num,
                              const char *callback_data)
{
        assert(player_num >= 0 && player_num < game->n_players);

        printf("%i %s\n", player_num, callback_data);
}

void
pcx_game_free(struct pcx_game *game)
{
        pcx_buffer_destroy(&game->buffer);

        for (int i = 0; i < game->n_players; i++)
                pcx_free(game->players[i].name);

        pcx_free(game);
}
