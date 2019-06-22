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
        struct pcx_game_callbacks callbacks;
        void *user_data;
};

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

        for (unsigned i = 0; i < n_players; i++) {
                game->players[i].coins = PCX_GAME_START_COINS;
                for (unsigned j = 0; j < PCX_GAME_CARDS_PER_PLAYER; j++) {
                        struct pcx_game_card *card = game->players[i].cards + j;
                        card->dead = false;
                        card->character = take_card(game);
                }
        }

        return game;
}

void
pcx_game_free(struct pcx_game *game)
{
        for (int i = 0; i < game->n_players; i++)
                pcx_free(game->players[i].name);

        pcx_free(game);
}
