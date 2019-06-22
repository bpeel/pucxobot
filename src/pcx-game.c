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
#include <string.h>
#include <errno.h>

#include "pcx-character.h"
#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"

#define PCX_GAME_CARDS_PER_CHARACTER 3
#define PCX_GAME_CARDS_PER_PLAYER 2
#define PCX_GAME_TOTAL_CARDS (PCX_CHARACTER_COUNT * \
                              PCX_GAME_CARDS_PER_CHARACTER)
#define PCX_GAME_START_COINS 2

#define PCX_GAME_STACK_SIZE 8

#define PCX_GAME_WAIT_TIME (30 * 1000)

typedef void
(* pcx_game_callback_data_func)(struct pcx_game *game,
                                int player_num,
                                const char *data,
                                int extra_data);

typedef void
(* pcx_game_idle_func)(struct pcx_game *game);

/* Called just beforing popping the stack in order to clean up
 * data.
 */
typedef void
(* pcx_game_stack_destroy_func)(struct pcx_game *game);

struct pcx_game_stack_entry {
        pcx_game_callback_data_func func;
        pcx_game_idle_func idle_func;
        pcx_game_stack_destroy_func destroy_func;
        union {
                int i;
                void *p;
        } data;
};

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
        struct pcx_game_stack_entry stack[PCX_GAME_STACK_SIZE];
        int stack_pos;
        bool action_taken;
};

static const struct pcx_game_button
coup_button = {
        .text = "Puĉo",
        .data = "coup"
};

static const struct pcx_game_button
income_button = {
        .text = "Enspezi",
        .data = "income"
};

static const struct pcx_game_button
foreign_aid_button = {
        .text = "Eksterlanda helpo",
        .data = "foreign_aid"
};

static const struct pcx_game_button
tax_button = {
        .text = "Imposto (Duko)",
        .data = "tax"
};

static const struct pcx_game_button
assassinate_button = {
        .text = "Murdi (Murdisto)",
        .data = "assassinate"
};

static const struct pcx_game_button
exchange_button = {
        .text = "Interŝanĝi (Ambasadoro)",
        .data = "exchange"
};

static const struct pcx_game_button
steal_button = {
        .text = "Ŝteli (Kapitano)",
        .data = "steal"
};

static const struct pcx_game_button
accept_button = {
        .text = "Akcepti",
        .data = "accept"
};

static const struct pcx_game_button
challenge_button = {
        .text = "Defii",
        .data = "challenge"
};

static const struct pcx_game_button *
character_buttons[] = {
        &tax_button,
        &assassinate_button,
        &exchange_button,
        &steal_button,
};

static struct pcx_game_stack_entry *
get_stack_top(struct pcx_game *game)
{
        return game->stack + game->stack_pos - 1;
}

static struct pcx_game_stack_entry *
stack_push(struct pcx_game *game,
           pcx_game_callback_data_func func,
           pcx_game_idle_func idle_func)
{
        assert(game->stack_pos < PCX_N_ELEMENTS(game->stack));

        game->stack_pos++;

        struct pcx_game_stack_entry *entry = get_stack_top(game);

        entry->func = func;
        entry->idle_func = idle_func;
        entry->destroy_func = NULL;
        memset(&entry->data, 0, sizeof entry->data);

        return entry;
}

static void
stack_push_int(struct pcx_game *game,
               pcx_game_callback_data_func func,
               pcx_game_idle_func idle_func,
               int data)
{
        struct pcx_game_stack_entry *entry = stack_push(game, func, idle_func);

        entry->data.i = data;
}

static void
stack_push_pointer(struct pcx_game *game,
                   pcx_game_callback_data_func func,
                   pcx_game_idle_func idle_func,
                   pcx_game_stack_destroy_func destroy_func,
                   void *data)
{
        struct pcx_game_stack_entry *entry = stack_push(game, func, idle_func);

        entry->destroy_func = destroy_func;
        entry->data.p = data;
}

static void
stack_pop(struct pcx_game *game)
{
        assert(game->stack_pos > 0);

        struct pcx_game_stack_entry *entry = get_stack_top(game);

        if (entry->destroy_func)
                entry->destroy_func(game);

        game->stack_pos--;
}

static int
get_stack_data_int(struct pcx_game *game)
{
        assert(game->stack_pos > 0);

        return get_stack_top(game)->data.i;
}

static void *
get_stack_data_pointer(struct pcx_game *game)
{
        assert(game->stack_pos > 0);

        return get_stack_top(game)->data.p;
}

static void
do_idle(struct pcx_game *game)
{
        while (game->action_taken) {
                game->action_taken = false;

                if (game->stack_pos <= 0)
                        break;

                struct pcx_game_stack_entry *entry = get_stack_top(game);

                if (entry->idle_func == NULL)
                        break;

                entry->idle_func(game);
        }
}

static void
take_action(struct pcx_game *game)
{
        game->action_taken = true;
}

static void
send_buffer_message_with_buttons_to(struct pcx_game *game,
                                    int target_player,
                                    size_t n_buttons,
                                    const struct pcx_game_button *buttons)
{
        const char *msg = (const char *) game->buffer.data;
        enum pcx_game_message_format format = PCX_GAME_MESSAGE_FORMAT_PLAIN;

        if (target_player < 0) {
                game->callbacks.send_message(format,
                                             msg,
                                             n_buttons,
                                             buttons,
                                             game->user_data);
        } else {
                assert(target_player < game->n_players);
                game->callbacks.send_private_message(target_player,
                                                     format,
                                                     msg,
                                                     n_buttons,
                                                     buttons,
                                                     game->user_data);
        }
}

static void
send_buffer_message_with_buttons(struct pcx_game *game,
                                 size_t n_buttons,
                                 const struct pcx_game_button *buttons)
{
        send_buffer_message_with_buttons_to(game,
                                            -1, /* target_player */
                                            n_buttons,
                                            buttons);
}

static void
send_buffer_message_to(struct pcx_game *game,
                       int target_player)
{
        send_buffer_message_with_buttons_to(game,
                                            target_player,
                                            0, /* n_buttons */
                                            NULL /* buttons */);
}

static void
send_buffer_message(struct pcx_game *game)
{
        send_buffer_message_with_buttons(game,
                                         0, /* n_buttons */
                                         NULL /* buttons */);
}

PCX_PRINTF_FORMAT(2, 3)
static void
game_note(struct pcx_game *game,
          const char *format,
          ...)
{
        pcx_buffer_set_length(&game->buffer, 0);

        va_list ap;

        va_start(ap, format);
        pcx_buffer_append_vprintf(&game->buffer, format, ap);
        va_end(ap);

        send_buffer_message(game);
}

static void
add_button(struct pcx_buffer *buffer,
           const struct pcx_game_button *button)
{
        pcx_buffer_append(buffer, button, sizeof *button);
}

static void
get_buttons(struct pcx_game *game,
            struct pcx_buffer *buffer)
{
        const struct pcx_game_player *player =
                game->players + game->current_player;

        if (player->coins >= 10) {
                add_button(buffer, &coup_button);
                return;
        }

        add_button(buffer, &income_button);
        add_button(buffer, &foreign_aid_button);

        if (player->coins >= 7)
                add_button(buffer, &coup_button);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(character_buttons); i++)
                add_button(buffer, character_buttons[i]);
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
                        pcx_buffer_append_printf(buffer, "☠%s☠", name);
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
show_cards(struct pcx_game *game,
           int player_num)
{
        const struct pcx_game_player *player = game->players + player_num;

        if (!is_alive(player))
                return;

        pcx_buffer_set_length(&game->buffer, 0);
        pcx_buffer_append_string(&game->buffer,
                                 "Viaj kartoj estas:\n");

        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                const struct pcx_game_card *card = player->cards + i;
                const char *name = pcx_character_get_name(card->character);

                if (card->dead) {
                        pcx_buffer_append_printf(&game->buffer,
                                                 "☠%s☠\n",
                                                 name);
                } else {
                        pcx_buffer_append_printf(&game->buffer,
                                                 "%s\n",
                                                 name);
                }
        }

        send_buffer_message_to(game, player_num);
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

        struct pcx_buffer buttons = PCX_BUFFER_STATIC_INIT;

        if (!finished)
                get_buttons(game, &buttons);

        send_buffer_message_with_buttons(game,
                                         buttons.length /
                                         sizeof (struct pcx_game_button),
                                         (const struct pcx_game_button *)
                                         buttons.data);

        pcx_buffer_destroy(&buttons);
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

static void
choose_card_to_lose(struct pcx_game *game,
                    int player_num,
                    const char *data,
                    int extra_data)
{
        if (strcmp(data, "lose"))
                return;

        if (player_num != get_stack_data_int(game))
                return;

        struct pcx_game_player *player = game->players + player_num;

        if (extra_data < 0 ||
            extra_data >= PCX_GAME_CARDS_PER_PLAYER ||
            player->cards[extra_data].dead)
                return;

        take_action(game);

        player->cards[extra_data].dead = true;
        show_cards(game, player_num);
        stack_pop(game);
}

static bool
is_losing_all_cards(struct pcx_game *game,
                    int player_num)
{
        const struct pcx_game_player *player = game->players + player_num;
        int n_living = 0;

        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                if (!player->cards[i].dead)
                        n_living++;
        }

        if (game->stack_pos < n_living)
                return false;

        for (unsigned i = game->stack_pos - n_living;
             i < game->stack_pos;
             i++) {
                const struct pcx_game_stack_entry *entry = game->stack + i;

                if (entry->func != choose_card_to_lose ||
                    entry->data.i != player_num)
                        return false;
        }

        return true;
}

static void
choose_card_to_lose_idle(struct pcx_game *game)
{
        int player_num = get_stack_data_int(game);
        struct pcx_game_player *player = game->players + player_num;

        /* Check if the stack contains enough lose card entries for
         * the player to lose of their cards. In that case they don’t
         * need the message.
         */
        if (is_losing_all_cards(game, player_num)) {
                take_action(game);
                stack_pop(game);
                bool changed = false;
                for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                        if (!player->cards[i].dead) {
                                player->cards[i].dead = true;
                                changed = true;
                        }
                }
                if (changed)
                        show_cards(game, player_num);
                return;
        }

        struct pcx_game_button buttons[PCX_GAME_CARDS_PER_PLAYER];
        size_t n_buttons = 0;

        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                if (player->cards[i].dead)
                        continue;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "lose:%u", i);

                buttons[n_buttons].text =
                        pcx_character_get_name(player->cards[i].character);
                buttons[n_buttons].data = pcx_strdup((char *) buf.data);
                n_buttons++;
        }

        pcx_buffer_set_length(&game->buffer, 0);
        pcx_buffer_append_string(&game->buffer, "Kiun karton vi volas perdi?");

        send_buffer_message_with_buttons_to(game,
                                            player_num,
                                            n_buttons,
                                            buttons);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);
}

static void
lose_card(struct pcx_game *game,
          int player_num)
{
        stack_push_int(game,
                       choose_card_to_lose,
                       choose_card_to_lose_idle,
                       player_num);
}

typedef void
(* action_cb)(struct pcx_game *game,
              void *user_data);

struct challenge_data {
        action_cb cb;
        void *user_data;
        struct pcx_main_context_source *timeout_source;
        int player_num;
        enum pcx_character character;
        uint32_t accepted_players;
};

static void
remove_challenge_timeout(struct challenge_data *data)
{
        if (data->timeout_source) {
                pcx_main_context_remove_source(data->timeout_source);
                data->timeout_source = NULL;
        }
}

static void
do_challenge_action(struct pcx_game *game,
                    struct challenge_data *data)
{
        action_cb cb = data->cb;
        void *user_data = data->user_data;

        take_action(game);
        stack_pop(game);

        cb(game, user_data);
}

static bool
has_challenged_card(struct pcx_game *game,
                    struct challenge_data *data)
{
        const struct pcx_game_player *player = game->players + data->player_num;

        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                const struct pcx_game_card *card = player->cards + i;

                if (card->dead)
                        continue;

                if (card->character == data->character)
                        return true;
        }

        return false;
}

static void
change_card(struct pcx_game *game,
            int player_num,
            enum pcx_character character)
{
        struct pcx_game_player *player = game->players + player_num;

        for (unsigned i = 0; i < PCX_GAME_CARDS_PER_PLAYER; i++) {
                struct pcx_game_card *card = player->cards + i;

                if (!card->dead && card->character == character) {
                        game->deck[game->n_cards++] = character;
                        shuffle_deck(game);
                        card->character = take_card(game);
                        return;
                }
        }

        pcx_fatal("card not found in change_card");
}

static void
check_challenge_callback_data(struct pcx_game *game,
                              int player_num,
                              const char *command,
                              int extra_data)
{
        struct challenge_data *data = get_stack_data_pointer(game);

        if (!strcmp(command, accept_button.data)) {
                data->accepted_players |= UINT32_C(1) << player_num;

                if (data->accepted_players ==
                    (UINT32_C(1) << game->n_players) - 1)
                        do_challenge_action(game, data);
        } else if (!strcmp(command, challenge_button.data)) {
                if (player_num == data->player_num)
                        return;
                if (!is_alive(game->players + player_num))
                        return;

                int challenged_player = data->player_num;
                enum pcx_character challenged_card = data->character;

                if (has_challenged_card(game, data)) {
                        game_note(game,
                                  "%s defiis sed %s ja havis la %sn kaj %s "
                                  "perdas karton",
                                  game->players[player_num].name,
                                  game->players[challenged_player].name,
                                  pcx_character_get_name(challenged_card),
                                  game->players[player_num].name);
                        do_challenge_action(game, data);
                        change_card(game, challenged_player, challenged_card);
                        lose_card(game, player_num);
                } else {
                        game_note(game,
                                  "%s defiis kaj %s ne havis la %sn kaj %s "
                                  "perdas karton",
                                  game->players[player_num].name,
                                  game->players[challenged_player].name,
                                  pcx_character_get_name(challenged_card),
                                  game->players[challenged_player].name);
                        take_action(game);
                        stack_pop(game);
                        lose_card(game, challenged_player);
                }
        }
}

static void
check_challenge_timeout(struct pcx_main_context_source *source,
                        void *user_data)
{
        struct pcx_game *game = user_data;
        struct challenge_data *data = get_stack_data_pointer(game);

        assert(data->timeout_source == source);

        data->timeout_source = NULL;

        do_challenge_action(game, data);

        do_idle(game);
}

static void
check_challenge_destroy(struct pcx_game *game)
{
        struct challenge_data *data = get_stack_data_pointer(game);

        remove_challenge_timeout(data);
        pcx_free(data);
}

PCX_PRINTF_FORMAT(6, 7)
static void
check_challenge(struct pcx_game *game,
                int player_num,
                enum pcx_character character,
                action_cb cb,
                void *user_data,
                const char *message,
                ...)

{
        struct challenge_data *data = pcx_alloc(sizeof *data);

        data->timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             PCX_GAME_WAIT_TIME,
                                             check_challenge_timeout,
                                             game);
        data->player_num = player_num;
        data->character = character;
        data->cb = cb;
        data->user_data = user_data;
        data->accepted_players = UINT32_C(1) << player_num;

        take_action(game);

        pcx_buffer_set_length(&game->buffer, 0);

        va_list ap;
        va_start(ap, message);
        pcx_buffer_append_vprintf(&game->buffer, message, ap);
        va_end(ap);

        const struct pcx_game_button buttons[] = {
                challenge_button,
                accept_button
        };

        send_buffer_message_with_buttons(game,
                                         PCX_N_ELEMENTS(buttons),
                                         buttons);

        stack_push_pointer(game,
                           check_challenge_callback_data,
                           NULL, /* idle_cb */
                           check_challenge_destroy,
                           data);
}

static void
send_select_target(struct pcx_game *game,
                   const char *message,
                   const char *data)
{
        size_t n_buttons = 0;
        struct pcx_game_button buttons[PCX_GAME_MAX_PLAYERS];
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (unsigned i = 0; i < game->n_players; i++) {
                if (i == game->current_player || !is_alive(game->players + i))
                        continue;

                pcx_buffer_set_length(&buf, 0);
                pcx_buffer_append_printf(&buf, "%s:%u", data, i);
                buttons[n_buttons].text = game->players[i].name;
                buttons[n_buttons].data = pcx_strdup((const char *) buf.data);
                n_buttons++;
        }

        pcx_buffer_set_length(&game->buffer, 0);
        pcx_buffer_append_printf(&game->buffer,
                                 message,
                                 game->players[game->current_player].name);

        send_buffer_message_with_buttons(game,
                                         n_buttons,
                                         buttons);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);

        pcx_buffer_destroy(&buf);
}

static void
do_coup(struct pcx_game *game,
        int extra_data)
{
        struct pcx_game_player *player = game->players + game->current_player;

        if (player->coins < 7)
                return;

        if (extra_data == -1) {
                send_select_target(game,
                                   "%s, kiun vi volas mortigi dum la puĉo?",
                                   coup_button.data);
                return;
        }

        if (extra_data >= game->n_players || extra_data == game->current_player)
                return;

        struct pcx_game_player *target = game->players + extra_data;

        if (!is_alive(target))
                return;

        take_action(game);

        game_note(game,
                  "💣 %s faras puĉon kontraŭ %s",
                  player->name,
                  target->name);

        player->coins -= 7;
        lose_card(game, extra_data);
}

static void
do_income(struct pcx_game *game)
{
        struct pcx_game_player *player = game->players + game->current_player;

        game_note(game,
                  "💲 %s enspezas 1 moneron",
                  player->name);
        player->coins++;

        take_action(game);
}

static void
do_foreign_add(struct pcx_game *game)
{
}

static void
do_accepted_tax(struct pcx_game *game,
                void *user_data)
{
        struct pcx_game_player *player = game->players + game->current_player;

        game_note(game,
                  "Neniu defiis, %s prenas la 3 monerojn",
                  player->name);

        player->coins += 3;

        take_action(game);
}

static void
do_tax(struct pcx_game *game)
{
        struct pcx_game_player *player = game->players + game->current_player;

        check_challenge(game,
                        game->current_player,
                        PCX_CHARACTER_DUKE,
                        do_accepted_tax,
                        NULL, /* user_data */
                        "💸 %s pretendas havi la dukon kaj prenas 3 monerojn "
                        "per imposto.\n"
                        "Ĉu iu volas defii rin?",
                        player->name);
}

static void
do_assassinate(struct pcx_game *game)
{
}

static void
do_exchange(struct pcx_game *game)
{
}

static void
do_steal(struct pcx_game *game)
{
}

static bool
is_button(const char *data,
          const struct pcx_game_button *button)
{
        return !strcmp(data, button->data);
}

static void
choose_action(struct pcx_game *game,
              int player_num,
              const char *data,
              int extra_data)
{
        if (player_num != game->current_player)
                return;

        if (is_button(data, &coup_button)) {
                do_coup(game, extra_data);
        } else if (game->players[player_num].coins < 10) {
                if (is_button(data, &income_button))
                        do_income(game);
                else if (is_button(data, &foreign_aid_button))
                        do_foreign_add(game);
                else if (is_button(data, &tax_button))
                        do_tax(game);
                else if (is_button(data, &assassinate_button))
                        do_assassinate(game);
                else if (is_button(data, &exchange_button))
                        do_exchange(game);
                else if (is_button(data, &steal_button))
                        do_steal(game);
        }
}

static void
choose_action_idle(struct pcx_game *game)
{
        /* If the game becomes idle when the top of the stack is to
         * choose an action then the turn is over.
         */

        int next_player = game->current_player;

        while (true) {
                next_player = (next_player + 1) % game->n_players;

                if (next_player == game->current_player ||
                    is_alive(game->players + next_player))
                        break;
        }

        game->current_player = next_player;

        show_stats(game);
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

        stack_push(game,
                   choose_action,
                   choose_action_idle);

        for (unsigned i = 0; i < n_players; i++)
                show_cards(game, i);

        show_stats(game);

        return game;
}

void
pcx_game_handle_callback_data(struct pcx_game *game,
                              int player_num,
                              const char *callback_data)
{
        assert(player_num >= 0 && player_num < game->n_players);

        if (game->stack_pos <= 0 || is_finished(game))
                return;

        int extra_data;
        const char *colon = strchr(callback_data, ':');

        if (colon == NULL) {
                extra_data = -1;
                colon = callback_data + strlen(callback_data);
        } else {
                char *tail;

                errno = 0;
                extra_data = strtol(colon + 1, &tail, 10);

                if (*tail || errno || extra_data < 0)
                        return;
        }

        if (colon <= callback_data)
                return;

        char *main_data = pcx_strndup(callback_data, colon - callback_data);
        struct pcx_game_stack_entry *entry = get_stack_top(game);

        entry->func(game, player_num, main_data, extra_data);

        pcx_free(main_data);

        do_idle(game);
}

void
pcx_game_free(struct pcx_game *game)
{
        while (game->stack_pos > 0)
                stack_pop(game);

        pcx_buffer_destroy(&game->buffer);

        for (int i = 0; i < game->n_players; i++)
                pcx_free(game->players[i].name);

        pcx_free(game);
}
