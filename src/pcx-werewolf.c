/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2024  Neil Roberts
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

#include "pcx-werewolf.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"
#include "pcx-werewolf-help.h"

#define PCX_WEREWOLF_MIN_PLAYERS 3
#define PCX_WEREWOLF_MAX_PLAYERS 10

#define PCX_WEREWOLF_N_EXTRA_CARDS 3
#define PCX_WEREWOLF_N_WEREWOLVES 2

#define PCX_WEREWOLF_PICK_MODE_PHASE -2
#define PCX_WEREWOLF_PICK_WAITING_PHASE -1

#define PCX_WEREWOLF_SEE_CENTER_CARDS_FLAG 16
#define PCX_WEREWOLF_N_SEE_CENTER_CHOICES \
        (PCX_WEREWOLF_N_EXTRA_CARDS * (PCX_WEREWOLF_N_EXTRA_CARDS - 1) / 2)

struct pcx_werewolf_player {
        char *name;

        /* Card that the player currently has in front of them. */
        enum pcx_werewolf_role card;
        /* Role that the player believes they have for the purposes of
         * waking up at the right phase. This can be different from
         * their actual card if someone has changed it during another
         * phase.
         */
        enum pcx_werewolf_role wakeup_role;

        int vote;
};

struct pcx_werewolf {
        struct pcx_werewolf_player players[PCX_WEREWOLF_MAX_PLAYERS];
        int n_players;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        struct pcx_main_context_source *game_over_source;
        enum pcx_text_language language;
        struct pcx_buffer buffer;

        int deck_mode;

        struct pcx_main_context_source *timeout_source;
        int current_phase;

        enum pcx_werewolf_role extra_cards[PCX_WEREWOLF_N_EXTRA_CARDS];
        uint32_t available_roles;

        int first_choice;

        uint32_t voted_mask;

        enum pcx_werewolf_role *card_overrides;
};

/* An in-progress deck that is being built up */
struct pcx_werewolf_deck {
        enum pcx_werewolf_role *cards;
        /* Total number of cards that will be added to this deck */
        int n_cards;
        /* The current number of cards that are in the deck */
        int card_num;
        /* A bitmask of roles that can be added to the deck */
        int available_roles;
};

struct pcx_werewolf_role_data {
        const char *symbol;
        enum pcx_text_string name;
        void (* phase_cb)(struct pcx_werewolf *werewof);
        void (* add_card_cb)(struct pcx_werewolf_deck *deck);
};
;
struct pcx_werewolf_deck_mode_role {
        int min_players;
        enum pcx_werewolf_role role;
};

struct pcx_werewolf_deck_mode_data {
        enum pcx_text_string name;
        int min_players;
        int max_players;
        int n_roles;
        const struct pcx_werewolf_deck_mode_role *roles;
};

static const struct pcx_werewolf_deck_mode_data
deck_modes[] = {
        {
                .name = PCX_TEXT_STRING_DECK_MODE_BASIC,
                .min_players = 3,
                .max_players = 5,
                .n_roles = 5,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_SEER },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_MOONSTRUCK,
                .min_players = 3,
                .max_players = 6,
                .n_roles = 6,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_INSOMNIAC },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                        { 5, PCX_WEREWOLF_ROLE_SEER },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_LONELY_NIGHT,
                .min_players = 3,
                .max_players = 4,
                .n_roles = 4,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_SEER },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_CONFUSION,
                .min_players = 3,
                .max_players = 9,
                .n_roles = 10,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_DRUNK },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                        { 0, PCX_WEREWOLF_ROLE_INSOMNIAC },
                        { 5, PCX_WEREWOLF_ROLE_SEER },
                        { 8, PCX_WEREWOLF_ROLE_MINION },
                        { 9, PCX_WEREWOLF_ROLE_MASON },
                        { 9, PCX_WEREWOLF_ROLE_MASON },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_PAYBACK,
                .min_players = 4,
                .max_players = 7,
                .n_roles = 8,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_HUNTER },
                        { 0, PCX_WEREWOLF_ROLE_SEER },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_DRUNK },
                        { 0, PCX_WEREWOLF_ROLE_INSOMNIAC },
                        { 5, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_SECRET_COMPANIONS,
                .min_players = 6,
                .max_players = 7,
                .n_roles = 9,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_MINION },
                        { 0, PCX_WEREWOLF_ROLE_HUNTER },
                        { 0, PCX_WEREWOLF_ROLE_SEER },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                        { 0, PCX_WEREWOLF_ROLE_MASON },
                        { 0, PCX_WEREWOLF_ROLE_MASON },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_HOUSE_OF_DESPAIR,
                .min_players = 4,
                .max_players = 10,
                .n_roles = 12,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_TANNER },
                        { 0, PCX_WEREWOLF_ROLE_SEER },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_DRUNK },
                        { 0, PCX_WEREWOLF_ROLE_INSOMNIAC },
                        { 5, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                        { 7, PCX_WEREWOLF_ROLE_MASON },
                        { 7, PCX_WEREWOLF_ROLE_MASON },
                        { 8, PCX_WEREWOLF_ROLE_HUNTER },
                        { 9, PCX_WEREWOLF_ROLE_MINION },
                },
        },
        {
                .name = PCX_TEXT_STRING_DECK_MODE_TWILIGHT_ALLIANCE,
                .min_players = 5,
                .max_players = 10,
                .n_roles = 11,
                .roles = (const struct pcx_werewolf_deck_mode_role[]) {
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_WEREWOLF },
                        { 0, PCX_WEREWOLF_ROLE_MASON },
                        { 0, PCX_WEREWOLF_ROLE_MASON },
                        { 0, PCX_WEREWOLF_ROLE_MINION },
                        { 0, PCX_WEREWOLF_ROLE_ROBBER },
                        { 0, PCX_WEREWOLF_ROLE_TROUBLEMAKER },
                        { 0, PCX_WEREWOLF_ROLE_INSOMNIAC },
                        { 6, PCX_WEREWOLF_ROLE_DRUNK },
                        { 7, PCX_WEREWOLF_ROLE_SEER },
                        { 10, PCX_WEREWOLF_ROLE_TANNER },
                },
        },
};

static void
remove_card_availability(struct pcx_werewolf_deck *deck,
                         enum pcx_werewolf_role card)
{
       deck->available_roles &= ~(1 << card);
}

static void
add_card_to_deck(struct pcx_werewolf_deck *deck,
                 enum pcx_werewolf_role card)
{
        assert(deck->card_num < deck->n_cards);
        deck->cards[deck->card_num++] = card;
        remove_card_availability(deck, card);
}

static void
next_phase_cb(struct pcx_main_context_source *source,
              void *user_data);

static void
append_role(struct pcx_werewolf *werewolf,
            enum pcx_werewolf_role role);

static void
append_text_string(struct pcx_werewolf *werewolf,
                   enum pcx_text_string string)
{
        const char *value = pcx_text_get(werewolf->language, string);
        pcx_buffer_append_string(&werewolf->buffer, value);
}

static void
queue_next_phase(struct pcx_werewolf *werewolf,
                 int n_seconds)
{
        assert(werewolf->timeout_source == NULL);

        werewolf->timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             n_seconds * 1000,
                                             next_phase_cb,
                                             werewolf);
}

static int
find_player_for_wakeup_role(struct pcx_werewolf *werewolf,
                            enum pcx_werewolf_role role)
{
        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].wakeup_role == role)
                        return i;
        }

        return -1;
}

static void
send_message_to_wakeup_role(struct pcx_werewolf *werewolf,
                            enum pcx_werewolf_role role,
                            struct pcx_game_message *message)
{
        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].wakeup_role == role) {
                        message->target = i;
                        werewolf->callbacks.send_message(message,
                                                         werewolf->user_data);
                }
        }
}

static void
send_buffer_to_wakeup_role(struct pcx_werewolf *werewolf,
                           enum pcx_werewolf_role role)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) werewolf->buffer.data;

        send_message_to_wakeup_role(werewolf, role, &message);
}

static void
send_lone_wolf_message(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        static const struct pcx_game_button buttons[] = {
                { .text = "A", .data = "wolfsee:0" },
                { .text = "B", .data = "wolfsee:1" },
                { .text = "C", .data = "wolfsee:2" },
        };

        _Static_assert(PCX_N_ELEMENTS(buttons) == PCX_WEREWOLF_N_EXTRA_CARDS,
                       "Need to have a button for each extra card");

        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_LONE_WOLF);
        message.buttons = buttons;
        message.n_buttons = PCX_N_ELEMENTS(buttons);

        send_message_to_wakeup_role(werewolf,
                                    PCX_WEREWOLF_ROLE_WEREWOLF,
                                    &message);
}

static void
make_player_same_role_message(struct pcx_werewolf *werewolf,
                              enum pcx_text_string message_text,
                              enum pcx_werewolf_role role)
{
        pcx_buffer_set_length(&werewolf->buffer, 0);
        append_text_string(werewolf, message_text);
        pcx_buffer_append_c(&werewolf->buffer, '\n');

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].wakeup_role == role) {
                        pcx_buffer_append_c(&werewolf->buffer, '\n');
                        pcx_buffer_append_string(&werewolf->buffer,
                                                 werewolf->players[i].name);
                }
        }
}

static void
send_player_same_role_message(struct pcx_werewolf *werewolf,
                              enum pcx_text_string message_text,
                              enum pcx_werewolf_role role)
{
        make_player_same_role_message(werewolf, message_text, role);

        send_buffer_to_wakeup_role(werewolf, role);
}

static void
send_wolf_pack_message(struct pcx_werewolf *werewolf)
{
        send_player_same_role_message(werewolf,
                                      PCX_TEXT_STRING_WEREWOLVES_ARE,
                                      PCX_WEREWOLF_ROLE_WEREWOLF);
}

static int
count_wakeup_wolves(struct pcx_werewolf *werewolf)
{
        int n_werewolves = 0;

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].wakeup_role ==
                    PCX_WEREWOLF_ROLE_WEREWOLF)
                        n_werewolves++;
        }

        return n_werewolves;
}

static void
werewolf_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_WEREWOLF_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int n_werewolves = count_wakeup_wolves(werewolf);

        if (n_werewolves == 1) {
                send_lone_wolf_message(werewolf);
        } else {
                if (n_werewolves > 1)
                        send_wolf_pack_message(werewolf);
                queue_next_phase(werewolf, rand() % 10 + 5);
        }
}

static void
send_minion_message(struct pcx_werewolf *werewolf,
                    int minion)
{
        if (find_player_for_wakeup_role(werewolf,
                                        PCX_WEREWOLF_ROLE_WEREWOLF) == -1) {
                pcx_buffer_set_length(&werewolf->buffer, 0);
                append_text_string(werewolf, PCX_TEXT_STRING_NO_WEREWOLVES);
        } else {
                make_player_same_role_message(werewolf,
                                              PCX_TEXT_STRING_WEREWOLVES_ARE,
                                              PCX_WEREWOLF_ROLE_WEREWOLF);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.target = minion;
        message.text = (const char *) werewolf->buffer.data;

        werewolf->callbacks.send_message(&message, werewolf->user_data);
}

static void
minion_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_MINION_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int minion = find_player_for_wakeup_role(werewolf,
                                                 PCX_WEREWOLF_ROLE_MINION);

        if (minion != -1)
                send_minion_message(werewolf, minion);

        queue_next_phase(werewolf, 10);
}

static int
n_bits_in_filter(int filter)
{
        int count = 0;

        while (filter) {
                filter &= ~1 << (ffs(filter) - 1);
                count++;
        }

        return count;
}

static void
send_target_choice(struct pcx_werewolf *werewolf,
                   enum pcx_text_string message_text,
                   int active_player,
                   int filter_mask,
                   const char *command,
                   enum pcx_text_string extra_button_text,
                   const char *extra_button_command)
{
        filter_mask |= 1 << active_player;

        int n_player_buttons =
                werewolf->n_players - n_bits_in_filter(filter_mask);

        struct pcx_game_button *buttons =
                pcx_alloc((n_player_buttons + 1) * sizeof *buttons);
        int button_num = 0;

        for (int i = 0; i < werewolf->n_players; i++) {
                if ((filter_mask & (1 << i)))
                        continue;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&buf, "%s:%i", command, i);

                buttons[button_num].text = werewolf->players[i].name;
                buttons[button_num].data = (char *) buf.data;

                button_num++;
        }

        buttons[button_num].text =
                pcx_text_get(werewolf->language, extra_button_text);
        buttons[button_num].data = extra_button_command;

        assert(button_num == n_player_buttons);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.buttons = buttons;
        message.n_buttons = n_player_buttons + 1;
        message.target = active_player;
        message.text = pcx_text_get(werewolf->language, message_text);

        werewolf->callbacks.send_message(&message, werewolf->user_data);

        for (int i = 0; i < n_player_buttons; i++)
                pcx_free((char *) buttons[i].data);

        pcx_free(buttons);
}

static void
send_seer_choice(struct pcx_werewolf *werewolf,
                 int seer_player)
{
        send_target_choice(werewolf,
                           PCX_TEXT_STRING_WHO_SEE_CARD,
                           seer_player,
                           0, /* filter_mask */
                           "see",
                           PCX_TEXT_STRING_TWO_CARDS_FROM_THE_CENTER,
                           "see:center");
}

static void
send_multiple_masons_message(struct pcx_werewolf *werewolf)
{
        send_player_same_role_message(werewolf,
                                      PCX_TEXT_STRING_MASONS_ARE,
                                      PCX_WEREWOLF_ROLE_MASON);
}

static void
send_lone_mason_message(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_LONE_MASON);

        send_message_to_wakeup_role(werewolf,
                                    PCX_WEREWOLF_ROLE_MASON,
                                    &message);
}

static void
mason_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_MASON_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int n_masons = 0;

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].wakeup_role == PCX_WEREWOLF_ROLE_MASON)
                        n_masons++;
        }

        if (n_masons == 1)
                send_lone_mason_message(werewolf);
        else if (n_masons > 1)
                send_multiple_masons_message(werewolf);

        queue_next_phase(werewolf, 10);
}

static void
mason_add_card_cb(struct pcx_werewolf_deck *deck)
{
        /* We can only add the mason card if there’s space for two of them */
        if (deck->card_num + 2 <= deck->n_cards) {
                add_card_to_deck(deck, PCX_WEREWOLF_ROLE_MASON);
                add_card_to_deck(deck, PCX_WEREWOLF_ROLE_MASON);
        } else {
                remove_card_availability(deck, PCX_WEREWOLF_ROLE_MASON);
        }
}

static void
seer_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_SEER_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int seer_player = find_player_for_wakeup_role(werewolf,
                                                      PCX_WEREWOLF_ROLE_SEER);

        if (seer_player == -1)
                queue_next_phase(werewolf, rand() % 10 + 5);
        else
                send_seer_choice(werewolf, seer_player);
}

static void
send_robber_choice(struct pcx_werewolf *werewolf,
                   int robber_player)
{
        send_target_choice(werewolf,
                           PCX_TEXT_STRING_WHO_TO_ROB,
                           robber_player,
                           0, /* filter_mask */
                           "rob",
                           PCX_TEXT_STRING_NOBODY,
                           "rob:nobody");
}

static void
robber_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_ROBBER_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int robber_player =
                find_player_for_wakeup_role(werewolf, PCX_WEREWOLF_ROLE_ROBBER);

        if (robber_player == -1)
                queue_next_phase(werewolf, rand() % 10 + 5);
        else
                send_robber_choice(werewolf, robber_player);
}

static void
send_troublemaker_choice(struct pcx_werewolf *werewolf,
                         int troublemaker_player)
{
        int filter_mask = 0;

        if (werewolf->first_choice != -1)
                filter_mask |= (1 << werewolf->first_choice);

        send_target_choice(werewolf,
                           werewolf->first_choice == -1 ?
                           PCX_TEXT_STRING_FIRST_SWAP :
                           PCX_TEXT_STRING_SECOND_SWAP,
                           troublemaker_player,
                           filter_mask,
                           "swap",
                           PCX_TEXT_STRING_NOBODY,
                           "swap:nobody");
}

static void
troublemaker_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_TROUBLEMAKER_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int troublemaker_player =
                find_player_for_wakeup_role(werewolf,
                                            PCX_WEREWOLF_ROLE_TROUBLEMAKER);

        werewolf->first_choice = -1;

        if (troublemaker_player == -1)
                queue_next_phase(werewolf, rand() % 10 + 5);
        else
                send_troublemaker_choice(werewolf, troublemaker_player);
}

static void
drunk_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_DRUNK_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int drunk_player =
                find_player_for_wakeup_role(werewolf, PCX_WEREWOLF_ROLE_DRUNK);

        if (drunk_player != -1) {
                message.text =
                        pcx_text_get(werewolf->language,
                                     PCX_TEXT_STRING_WHICH_CARD_DO_YOU_WANT);
                message.target = drunk_player;

                static const struct pcx_game_button buttons[] = {
                        { .text = "A", .data = "take:0" },
                        { .text = "B", .data = "take:1" },
                        { .text = "C", .data = "take:2" },
                };

                _Static_assert(PCX_N_ELEMENTS(buttons) ==
                               PCX_WEREWOLF_N_EXTRA_CARDS,
                               "There needs to be a button for each extra "
                               "card");

                message.n_buttons = PCX_N_ELEMENTS(buttons);
                message.buttons = buttons;

                werewolf->callbacks.send_message(&message, werewolf->user_data);
        } else {
                queue_next_phase(werewolf, rand() % 10 + 5);
        }
}

static void
send_insomniac_message(struct pcx_werewolf *werewolf,
                       int insomniac_player)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        enum pcx_werewolf_role current_role =
                werewolf->players[insomniac_player].card;

        if (current_role == PCX_WEREWOLF_ROLE_INSOMNIAC) {
                message.text = pcx_text_get(werewolf->language,
                                            PCX_TEXT_STRING_STILL_INSOMNIAC);
        } else {
                pcx_buffer_set_length(&werewolf->buffer, 0);
                append_text_string(werewolf, PCX_TEXT_STRING_YOU_ARE_NOW);
                pcx_buffer_append_c(&werewolf->buffer, ' ');
                append_role(werewolf, current_role);
                message.text = (const char *) werewolf->buffer.data;
        }

        message.target = insomniac_player;

        werewolf->callbacks.send_message(&message, werewolf->user_data);
}

static void
insomniac_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_INSOMNIAC_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        int insomniac_player =
                find_player_for_wakeup_role(werewolf,
                                            PCX_WEREWOLF_ROLE_INSOMNIAC);

        if (insomniac_player != -1)
                send_insomniac_message(werewolf, insomniac_player);

        queue_next_phase(werewolf, 10);
}

static void
insomniac_add_card_cb(struct pcx_werewolf_deck *deck)
{
        /* The insomniac needs one of these cards to also be in the
         * deck otherwise the card is useless.
         */
        int needed_roles =
                (1 << PCX_WEREWOLF_ROLE_ROBBER) |
                (1 << PCX_WEREWOLF_ROLE_TROUBLEMAKER);

        for (int i = 0; i < deck->card_num; i++) {
                if (((1 << deck->cards[i]) & needed_roles)) {
                        /* One of the cards is already there so we can
                         * just add the insomniac.
                         */
                        add_card_to_deck(deck, PCX_WEREWOLF_ROLE_INSOMNIAC);

                        return;
                }
        }

        int available_roles = needed_roles & deck->available_roles;

        /* Can we just add one of the needed roles? */
        if (available_roles && deck->card_num + 2 <= deck->n_cards) {
                add_card_to_deck(deck, ffs(available_roles) - 1);
                add_card_to_deck(deck, PCX_WEREWOLF_ROLE_INSOMNIAC);
        } else {
                remove_card_availability(deck, PCX_WEREWOLF_ROLE_INSOMNIAC);
        }
}

static const struct pcx_werewolf_role_data
roles[] = {
        [PCX_WEREWOLF_ROLE_VILLAGER] = {
                .symbol = "🧑‍🌾",
                .name = PCX_TEXT_STRING_VILLAGER,
        },
        [PCX_WEREWOLF_ROLE_WEREWOLF] = {
                .symbol = "🐺",
                .name = PCX_TEXT_STRING_WEREWOLF,
                .phase_cb = werewolf_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_MINION] = {
                .symbol = "🦺",
                .name = PCX_TEXT_STRING_MINION,
                .phase_cb = minion_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_MASON] = {
                .symbol = "⚒️",
                .name = PCX_TEXT_STRING_MASON,
                .phase_cb = mason_phase_cb,
                .add_card_cb = mason_add_card_cb,
        },
        [PCX_WEREWOLF_ROLE_SEER] = {
                .symbol = "🔮",
                .name = PCX_TEXT_STRING_SEER,
                .phase_cb = seer_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_ROBBER] = {
                .symbol = "🤏",
                .name = PCX_TEXT_STRING_ROBBER,
                .phase_cb = robber_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_TROUBLEMAKER] = {
                .symbol = "🐈",
                .name = PCX_TEXT_STRING_TROUBLEMAKER,
                .phase_cb = troublemaker_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_DRUNK] = {
                .symbol = "🍺",
                .name = PCX_TEXT_STRING_DRUNK,
                .phase_cb = drunk_phase_cb,
        },
        [PCX_WEREWOLF_ROLE_INSOMNIAC] = {
                .symbol = "🥱",
                .name = PCX_TEXT_STRING_INSOMNIAC,
                .phase_cb = insomniac_phase_cb,
                .add_card_cb = insomniac_add_card_cb,
        },
        [PCX_WEREWOLF_ROLE_TANNER] = {
                .symbol = "🙍‍♂️",
                .name = PCX_TEXT_STRING_TANNER,
        },
        [PCX_WEREWOLF_ROLE_HUNTER] = {
                .symbol = "🔫",
                .name = PCX_TEXT_STRING_HUNTER,
        },
};

static void
append_role(struct pcx_werewolf *werewolf,
            enum pcx_werewolf_role role)
{
        pcx_buffer_append_string(&werewolf->buffer, roles[role].symbol);
        pcx_buffer_append_string(&werewolf->buffer, " ");
        append_text_string(werewolf, roles[role].name);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_werewolf *werewolf = user_data;
        werewolf->game_over_source = NULL;
        werewolf->callbacks.game_over(werewolf->user_data);
}

static void
start_voting_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct pcx_werewolf *werewolf = user_data;

        werewolf->timeout_source = NULL;

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text =
                pcx_text_get(werewolf->language,
                             PCX_TEXT_STRING_YOU_CAN_VOTE_FOR_A_WEREWOLF);

        message.n_buttons = werewolf->n_players;

        struct pcx_game_button *buttons =
                pcx_alloc(werewolf->n_players * sizeof *buttons);

        for (int i = 0; i < werewolf->n_players; i++) {
                buttons[i].text = werewolf->players[i].name;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "vote:%i", i);
                buttons[i].data = (const char *) buf.data;
        }

        message.buttons = buttons;

        werewolf->callbacks.send_message(&message, werewolf->user_data);

        for (int i = 0; i < werewolf->n_players; i++)
                pcx_free((char *) buttons[i].data);

        pcx_free(buttons);

        werewolf->timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             3 * 60 * 1000,
                                             start_voting_cb,
                                             werewolf);
}

static void
make_deck_from_mode(enum pcx_werewolf_role *cards,
                    const struct pcx_werewolf_deck_mode_data *mode,
                    int n_cards)
{
        int card_num = 0;

        for (int i = 0; i < mode->n_roles; i++) {
                const struct pcx_werewolf_deck_mode_role *role =
                        mode->roles + i;
                if (n_cards - PCX_WEREWOLF_N_EXTRA_CARDS >= role->min_players)
                        cards[card_num++] = role->role;
        }

        /* The rest of the cards are villagers */
        while (card_num < n_cards)
                cards[card_num++] = PCX_WEREWOLF_ROLE_VILLAGER;
}

static void
make_anarchy_deck(enum pcx_werewolf_role *cards,
                  int n_cards)
{
        struct pcx_werewolf_deck deck = {
                .cards = cards,
                .n_cards = n_cards,
                .available_roles = (1 << (int) PCX_N_ELEMENTS(roles)) - 1,
        };

        /* Add the werewolves */
        for (int i = 0; i < PCX_WEREWOLF_N_WEREWOLVES; i++)
                add_card_to_deck(&deck, PCX_WEREWOLF_ROLE_WEREWOLF);

        /* At least one villager for a 3-player game, two for a
         * 4-player game and three for any other game.
         */
        int n_base_villagers = n_cards - PCX_WEREWOLF_N_EXTRA_CARDS - 2;

        if (n_base_villagers > 3)
                n_base_villagers = 3;

        for (int i = 0; i < n_base_villagers; i++)
                add_card_to_deck(&deck, PCX_WEREWOLF_ROLE_VILLAGER);

        /* Add a random selection of the available roles */
        while (deck.card_num < n_cards && deck.available_roles) {
                int n_roles = n_bits_in_filter(deck.available_roles);
                int role_index = rand() % n_roles;
                int role_mask = deck.available_roles;

                while (role_index--)
                        role_mask &= ~(1 << (ffs(role_mask) - 1));

                int role = ffs(role_mask) - 1;

                if (roles[role].add_card_cb)
                        roles[role].add_card_cb(&deck);
                else
                        add_card_to_deck(&deck, role);
        }

        /* The rest of the cards are villagers */
        while (deck.card_num < n_cards)
                add_card_to_deck(&deck, PCX_WEREWOLF_ROLE_VILLAGER);
}

static void
deal_roles(struct pcx_werewolf *werewolf)
{
        enum pcx_werewolf_role cards[PCX_WEREWOLF_MAX_PLAYERS +
                                     PCX_WEREWOLF_N_EXTRA_CARDS];
        int n_cards = werewolf->n_players + PCX_WEREWOLF_N_EXTRA_CARDS;

        if (werewolf->deck_mode < PCX_N_ELEMENTS(deck_modes)) {
                make_deck_from_mode(cards,
                                    deck_modes + werewolf->deck_mode,
                                    n_cards);
        } else {
                make_anarchy_deck(cards, n_cards);
        }

        /* Shuffle the deck */
        for (int i = n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                enum pcx_werewolf_role t = cards[j];
                cards[j] = cards[i];
                cards[i] = t;
        }

        if (werewolf->card_overrides) {
                memcpy(cards,
                       werewolf->card_overrides,
                       n_cards * sizeof cards[0]);
        }

        /* Give a card to each player */
        for (int i = 0; i < werewolf->n_players; i++) {
                werewolf->players[i].wakeup_role = cards[i];
                werewolf->players[i].card = cards[i];
        }

        /* The rest of the cards are extra */
        memcpy(werewolf->extra_cards,
               cards + werewolf->n_players,
               PCX_WEREWOLF_N_EXTRA_CARDS * sizeof cards[0]);

        for (int i = 0; i < n_cards; i++)
                werewolf->available_roles |= 1 << cards[i];
}

static void
show_village_roles(struct pcx_werewolf *werewolf)
{
        int role_counts[PCX_N_ELEMENTS(roles)] = { 0, };

        for (int i = 0; i < werewolf->n_players; i++)
                role_counts[werewolf->players[i].card]++;

        for (int i = 0; i < PCX_WEREWOLF_N_EXTRA_CARDS; i++)
                role_counts[werewolf->extra_cards[i]]++;

        pcx_buffer_set_length(&werewolf->buffer, 0);
        append_text_string(werewolf, PCX_TEXT_STRING_SHOW_ROLES);
        pcx_buffer_append_c(&werewolf->buffer, '\n');

        for (unsigned i = 0; i < PCX_N_ELEMENTS(roles); i++) {
                if (role_counts[i] <= 0)
                        continue;

                pcx_buffer_append_c(&werewolf->buffer, '\n');

                append_role(werewolf, i);

                if (role_counts[i] > 1) {
                        pcx_buffer_append_printf(&werewolf->buffer,
                                                 " × %i",
                                                 role_counts[i]);
                }
        }

        pcx_buffer_append_string(&werewolf->buffer, "\n\n");
        append_text_string(werewolf, PCX_TEXT_STRING_FALL_ASLEEP);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) werewolf->buffer.data;

        werewolf->callbacks.send_message(&message, werewolf->user_data);
}

static void
send_roles(struct pcx_werewolf *werewolf)
{
        for (int i = 0; i < werewolf->n_players; i++) {
                pcx_buffer_set_length(&werewolf->buffer, 0);

                append_text_string(werewolf, PCX_TEXT_STRING_TELL_ROLE);
                pcx_buffer_append_c(&werewolf->buffer, ' ');
                append_role(werewolf, werewolf->players[i].card);

                struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

                message.text = (const char *) werewolf->buffer.data;
                message.target = i;

                werewolf->callbacks.send_message(&message, werewolf->user_data);
        }
}

static void
start_next_phase(struct pcx_werewolf *werewolf)
{
        while (++werewolf->current_phase < PCX_N_ELEMENTS(roles)) {
                enum pcx_werewolf_role role = werewolf->current_phase;

                if ((werewolf->available_roles & (1 << role)) &&
                    roles[role].phase_cb) {
                        roles[role].phase_cb(werewolf);
                        return;
                }
        }

        /* If we make it here then all the phases are complete and we
         * need to switch to the discussion phase.
         */

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_EVERYONE_WAKES_UP);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        werewolf->timeout_source =
                pcx_main_context_add_timeout(NULL,
                                             60 * 1000,
                                             start_voting_cb,
                                             werewolf);
}

static void
next_phase_cb(struct pcx_main_context_source *source,
              void *user_data)
{
        struct pcx_werewolf *werewolf = user_data;

        werewolf->timeout_source = NULL;

        start_next_phase(werewolf);
}

static void
send_pick_deck_mode_message(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        struct pcx_game_button buttons[PCX_N_ELEMENTS(deck_modes) + 1];

        for (int i = 0; i < PCX_N_ELEMENTS(deck_modes); i++) {
                if (werewolf->n_players >= deck_modes[i].min_players &&
                    werewolf->n_players <= deck_modes[i].max_players) {
                        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                        pcx_buffer_set_length(&buf, 0);
                        pcx_buffer_append_printf(&buf, "mode:%i", i);

                        buttons[message.n_buttons].text =
                                pcx_text_get(werewolf->language,
                                             deck_modes[i].name);
                        buttons[message.n_buttons].data =
                                (const char *) buf.data;

                        message.n_buttons++;
                }
        }

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_set_length(&buf, 0);
        pcx_buffer_append_printf(&buf,
                                 "mode:%zu",
                                 PCX_N_ELEMENTS(deck_modes));
        buttons[message.n_buttons].text =
                pcx_text_get(werewolf->language,
                             PCX_TEXT_STRING_DECK_MODE_ANARCHY);
        buttons[message.n_buttons].data = (const char *) buf.data;
        message.n_buttons++;

        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_WHICH_DECK_MODE);
        message.buttons = buttons;

        werewolf->callbacks.send_message(&message,
                                         werewolf->user_data);

        for (int i = 0; i < message.n_buttons; i++)
                pcx_free((char *) buttons[i].data);
}

struct pcx_werewolf *
pcx_werewolf_new(const struct pcx_game_callbacks *callbacks,
                 void *user_data,
                 enum pcx_text_language language,
                 int n_players,
                 const char *const *names,
                 const struct pcx_werewolf_debug_overrides *overrides)
{
        assert(n_players > 0 && n_players <= PCX_WEREWOLF_MAX_PLAYERS);

        struct pcx_werewolf *werewolf = pcx_calloc(sizeof *werewolf);

        werewolf->language = language;
        werewolf->callbacks = *callbacks;
        werewolf->user_data = user_data;
        pcx_buffer_init(&werewolf->buffer);

        werewolf->current_phase = PCX_WEREWOLF_PICK_MODE_PHASE;

        werewolf->n_players = n_players;

        if (overrides && overrides->cards) {
                werewolf->card_overrides =
                        pcx_memdup(overrides->cards,
                                   sizeof (enum pcx_werewolf_role) *
                                   (PCX_WEREWOLF_N_EXTRA_CARDS + n_players));
        }

        for (unsigned i = 0; i < n_players; i++)
                werewolf->players[i].name = pcx_strdup(names[i]);

        send_pick_deck_mode_message(werewolf);

        return werewolf;
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        return pcx_werewolf_new(callbacks,
                                user_data,
                                language,
                                n_players,
                                names,
                                NULL /* overrides */);
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_werewolf_help[language]);
}

static int
extract_int(const char *extra_data)
{
        if (extra_data == NULL)
                return -1;

        const char *p;
        int value = 0;

        for (p = extra_data; *p; p++) {
                if (*p < '0' || *p > '9')
                        return -1;

                value = value * 10 + *p - '0';
        }

        if (p == extra_data)
                return -1;

        return value;
}

static void
handle_see_center_cards(struct pcx_werewolf *werewolf,
                        int player_num,
                        int mask)
{
        mask &= ~PCX_WEREWOLF_SEE_CENTER_CARDS_FLAG;

        if ((mask & ~((1 << PCX_WEREWOLF_N_SEE_CENTER_CHOICES) - 1)))
                return;

        if (n_bits_in_filter(mask) != 2)
                return;

        pcx_buffer_set_length(&werewolf->buffer, 0);

        append_text_string(werewolf,
                           PCX_TEXT_STRING_SHOW_TWO_CARDS_FROM_CENTER);

        pcx_buffer_append_c(&werewolf->buffer, '\n');

        while (mask) {
                int card_num = ffs(mask) - 1;

                pcx_buffer_append_printf(&werewolf->buffer,
                                         "\n"
                                         "%c: ",
                                         card_num + 'A');
                append_role(werewolf, werewolf->extra_cards[card_num]);

                mask &= ~(1 << card_num);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (const char *) werewolf->buffer.data;
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
send_center_card_choice(struct pcx_werewolf *werewolf,
                        int player_num)
{
        static const struct pcx_game_button buttons[] = {
                { "A+B", "see:19" },
                { "A+C", "see:21" },
                { "B+C", "see:22" },
        };

        assert(PCX_N_ELEMENTS(buttons) == PCX_WEREWOLF_N_SEE_CENTER_CHOICES);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_SEE_WHICH_CENTER_CARDS);
        message.target = player_num;
        message.n_buttons = PCX_N_ELEMENTS(buttons);
        message.buttons = buttons;
        werewolf->callbacks.send_message(&message, werewolf->user_data);
}

static void
handle_see_player_card(struct pcx_werewolf *werewolf,
                       int player_num,
                       int target)
{
        if (target == player_num)
                return;

        pcx_buffer_set_length(&werewolf->buffer, 0);

        const char *msg = pcx_text_get(werewolf->language,
                                       PCX_TEXT_STRING_SHOW_PLAYER_CARD);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 msg,
                                 werewolf->players[target].name);

        pcx_buffer_append_c(&werewolf->buffer, ' ');

        append_role(werewolf, werewolf->players[target].card);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (const char *) werewolf->buffer.data;
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_see_cb(struct pcx_werewolf *werewolf,
              int player_num,
              const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_ROLE_SEER)
                return;

        if (werewolf->players[player_num].wakeup_role != PCX_WEREWOLF_ROLE_SEER)
                return;

        if (strcmp(extra_data, "center")) {
                int target = extract_int(extra_data);

                if (target == -1)
                        return;

                if ((target & PCX_WEREWOLF_SEE_CENTER_CARDS_FLAG))
                        handle_see_center_cards(werewolf, player_num, target);
                else
                        handle_see_player_card(werewolf, player_num, target);
        } else {
                send_center_card_choice(werewolf, player_num);
        }
}

static void
handle_wolf_see_cb(struct pcx_werewolf *werewolf,
                   int player_num,
                   const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_ROLE_WEREWOLF)
                return;

        if (werewolf->players[player_num].wakeup_role !=
            PCX_WEREWOLF_ROLE_WEREWOLF)
                return;

        if (count_wakeup_wolves(werewolf) != 1)
                return;

        int target = extract_int(extra_data);

        if (target < 0 || target >= PCX_WEREWOLF_N_EXTRA_CARDS)
                return;

        pcx_buffer_set_length(&werewolf->buffer, 0);
        const char *msg = pcx_text_get(werewolf->language,
                                       PCX_TEXT_STRING_CENTER_CARD_IS);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 msg,
                                 target + 'A');
        pcx_buffer_append_c(&werewolf->buffer, ' ');
        append_role(werewolf, werewolf->extra_cards[target]);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.target = player_num;
        message.text = (const char *) werewolf->buffer.data;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_rob_player_card(struct pcx_werewolf *werewolf,
                       int player_num,
                       int target)
{
        if (target == player_num)
                return;

        enum pcx_werewolf_role stolen_role = werewolf->players[target].card;

        pcx_buffer_set_length(&werewolf->buffer, 0);

        const char *msg = pcx_text_get(werewolf->language,
                                       PCX_TEXT_STRING_STEAL_FROM);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 msg,
                                 werewolf->players[target].name);

        pcx_buffer_append_c(&werewolf->buffer, ' ');

        append_role(werewolf, stolen_role);

        werewolf->players[target].card =
                werewolf->players[player_num].card;
        werewolf->players[player_num].card = stolen_role;

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (const char *) werewolf->buffer.data;
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_rob_nobody(struct pcx_werewolf *werewolf,
                  int player_num)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_ROBBED_NOBODY);
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_rob_cb(struct pcx_werewolf *werewolf,
              int player_num,
              const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_ROLE_ROBBER)
                return;

        if (werewolf->players[player_num].wakeup_role !=
            PCX_WEREWOLF_ROLE_ROBBER)
                return;

        if (strcmp(extra_data, "nobody")) {
                int target = extract_int(extra_data);

                if (target == -1)
                        return;

                handle_rob_player_card(werewolf, player_num, target);
        } else {
                handle_rob_nobody(werewolf, player_num);
        }
}

static void
handle_first_swap_choice(struct pcx_werewolf *werewolf,
                         int player_num,
                         int target)
{
        werewolf->first_choice = target;

        send_troublemaker_choice(werewolf, player_num);
}

static void
handle_swap(struct pcx_werewolf *werewolf,
            int player_num,
            int swap_a,
            int swap_b)
{
        pcx_buffer_set_length(&werewolf->buffer, 0);

        const char *msg = pcx_text_get(werewolf->language,
                                       PCX_TEXT_STRING_SWAP_CARDS_OF);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 msg,
                                 werewolf->players[swap_a].name,
                                 werewolf->players[swap_b].name);

        enum pcx_werewolf_role temp = werewolf->players[swap_a].card;
        werewolf->players[swap_a].card = werewolf->players[swap_b].card;
        werewolf->players[swap_b].card = temp;

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = (const char *) werewolf->buffer.data;
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_swap_player_card(struct pcx_werewolf *werewolf,
                        int player_num,
                        int target)
{
        if (target == player_num || target == werewolf->first_choice)
                return;

        if (werewolf->first_choice == -1) {
                handle_first_swap_choice(werewolf, player_num, target);
        } else {
                handle_swap(werewolf,
                            player_num,
                            werewolf->first_choice,
                            target);
        }
}

static void
handle_swap_nobody(struct pcx_werewolf *werewolf,
                   int player_num)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_SWAPPED_NOBODY);
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_swap_cb(struct pcx_werewolf *werewolf,
               int player_num,
               const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_ROLE_TROUBLEMAKER)
                return;

        if (werewolf->players[player_num].wakeup_role !=
            PCX_WEREWOLF_ROLE_TROUBLEMAKER)
                return;

        if (strcmp(extra_data, "nobody")) {
                int target = extract_int(extra_data);

                if (target == -1)
                        return;

                handle_swap_player_card(werewolf, player_num, target);
        } else {
                handle_swap_nobody(werewolf, player_num);
        }
}

static void
handle_take_cb(struct pcx_werewolf *werewolf,
               int player_num,
               const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_ROLE_DRUNK)
                return;

        if (werewolf->players[player_num].wakeup_role !=
            PCX_WEREWOLF_ROLE_DRUNK)
                return;

        int target = extract_int(extra_data);

        if (target < 0 || target >= PCX_WEREWOLF_N_EXTRA_CARDS)
                return;

        enum pcx_werewolf_role temp = werewolf->players[player_num].card;
        werewolf->players[player_num].card = werewolf->extra_cards[target];
        werewolf->extra_cards[target] = temp;

        pcx_buffer_set_length(&werewolf->buffer, 0);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 pcx_text_get(werewolf->language,
                                              PCX_TEXT_STRING_YOU_TAKE_CARD),
                                 'A' + target);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.target = player_num;
        message.text = (const char *) werewolf->buffer.data;
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        start_next_phase(werewolf);
}

static void
handle_mode_cb(struct pcx_werewolf *werewolf,
               int player_num,
               const char *extra_data)
{
        if (extra_data == NULL)
                return;

        if (werewolf->current_phase != PCX_WEREWOLF_PICK_MODE_PHASE)
                return;

        int mode = extract_int(extra_data);

        if (mode < 0) {
                return;
        } else if (mode < PCX_N_ELEMENTS(deck_modes)) {
                if (werewolf->n_players < deck_modes[mode].min_players ||
                    werewolf->n_players > deck_modes[mode].max_players)
                        return;
        } else if (mode != PCX_N_ELEMENTS(deck_modes)) {
                /* The player can select mode
                 * PCX_N_ELEMENTS(deck_modes) to mean anarchy mode.
                 */
                return;
        }

        werewolf->deck_mode = mode;
        werewolf->current_phase = PCX_WEREWOLF_PICK_WAITING_PHASE;

        deal_roles(werewolf);
        show_village_roles(werewolf);
        send_roles(werewolf);

        queue_next_phase(werewolf, 10);
}

struct vote_count {
        uint8_t player_num;
        uint8_t vote_count;
};

static int
compare_vote_count_cb(const void *ap, const void *bp)
{
        const struct vote_count *a = ap;
        const struct vote_count *b = bp;

        return (int) b->vote_count - (int) a->vote_count;
}

static void
add_multiple_failed_peace_message(struct pcx_werewolf *werewolf)
{
        int n_werewolves = 0;

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].card == PCX_WEREWOLF_ROLE_WEREWOLF)
                        n_werewolves++;
        }

        const char *message =
                pcx_text_get(werewolf->language,
                             PCX_TEXT_STRING_HOWEVER_MULTIPLE_WEREWOLVES);
        const char *split = strstr(message, "%s");

        assert(split != NULL);

        pcx_buffer_append(&werewolf->buffer, message, split - message);

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].card != PCX_WEREWOLF_ROLE_WEREWOLF)
                        continue;

                pcx_buffer_append_string(&werewolf->buffer,
                                         werewolf->players[i].name);

                if (--n_werewolves > 1) {
                        pcx_buffer_append_string(&werewolf->buffer, ", ");
                } else if (n_werewolves == 1) {
                        append_text_string(werewolf,
                                           PCX_TEXT_STRING_FINAL_CONJUNCTION);
                }
        }

        pcx_buffer_append_string(&werewolf->buffer, split + 2);
        pcx_buffer_append_string(&werewolf->buffer, "\n\n");
        append_text_string(werewolf, PCX_TEXT_STRING_WEREWOLVES_WIN);
}

static void
add_no_one_dies_message(struct pcx_werewolf *werewolf)
{
        append_text_string(werewolf, PCX_TEXT_STRING_NO_ONE_DIES);
        pcx_buffer_append_c(&werewolf->buffer, ' ');

        int werewolf_player = -1;

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].card == PCX_WEREWOLF_ROLE_WEREWOLF) {
                        if (werewolf_player == -1) {
                                werewolf_player = i;
                        } else {
                                add_multiple_failed_peace_message(werewolf);
                                return;
                        }
                }
        }

        if (werewolf_player == -1) {
                append_text_string(werewolf,
                                   PCX_TEXT_STRING_NO_WEREWOLVES_AT_END);
                pcx_buffer_append_string(&werewolf->buffer, "\n\n");
                append_text_string(werewolf, PCX_TEXT_STRING_VILLAGERS_WIN);
        } else {
                const char *msg =
                        pcx_text_get(werewolf->language,
                                     PCX_TEXT_STRING_HOWEVER_ONE_WEREWOLF);
                const char *werewolf_name =
                        werewolf->players[werewolf_player].name;
                pcx_buffer_append_printf(&werewolf->buffer,
                                         msg,
                                         werewolf_name);
                pcx_buffer_append_string(&werewolf->buffer, "\n\n");
                append_text_string(werewolf, PCX_TEXT_STRING_WEREWOLVES_WIN);
        }
}

static void
add_one_death_message(struct pcx_werewolf *werewolf,
                      int dead_player)
{
        enum pcx_werewolf_role dead_role = werewolf->players[dead_player].card;

        const char *msg =
                pcx_text_get(werewolf->language,
                             PCX_TEXT_STRING_SACRIFICE);
        pcx_buffer_append_printf(&werewolf->buffer,
                                 msg,
                                 werewolf->players[dead_player].name);
        pcx_buffer_append_c(&werewolf->buffer, ' ');
        append_text_string(werewolf, PCX_TEXT_STRING_THEIR_ROLE);
        pcx_buffer_append_c(&werewolf->buffer, ' ');
        append_role(werewolf, dead_role);
        pcx_buffer_append_string(&werewolf->buffer, "\n\n");
}

static void
add_multiple_deaths_message(struct pcx_werewolf *werewolf,
                            const struct vote_count *dead_votes,
                            int n_dead_votes)
{
        append_text_string(werewolf, PCX_TEXT_STRING_MULTIPLE_SACRIFICES);
        pcx_buffer_append_string(&werewolf->buffer, "\n\n");

        for (int i = 0; i < n_dead_votes; i++) {
                const struct pcx_werewolf_player *player =
                        werewolf->players + dead_votes[i].player_num;

                pcx_buffer_append_string(&werewolf->buffer, player->name);
                pcx_buffer_append_string(&werewolf->buffer, " (");
                append_role(werewolf, player->card);
                pcx_buffer_append_string(&werewolf->buffer, ")\n");
        }

        pcx_buffer_append_string(&werewolf->buffer, "\n");
}

static int
get_death_mask(const struct vote_count *dead_votes,
               int n_dead_votes)
{
        int mask = 0;

        for (int i = 0; i < n_dead_votes; i++)
                mask |= 1 << dead_votes[i].player_num;

        return mask;
}

static bool
any_player_has_role(struct pcx_werewolf *werewolf,
                    enum pcx_werewolf_role role)
{
        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].card == role)
                        return true;
        }

        return false;
}

static int
find_dead_role(struct pcx_werewolf *werewolf,
               int death_mask,
               enum pcx_werewolf_role role)
{
        while (death_mask) {
                int player_num = ffs(death_mask) - 1;

                if (werewolf->players[player_num].card == role)
                        return player_num;

                death_mask &= ~(1 << player_num);
        }

        return -1;
}

static void
check_hunter(struct pcx_werewolf *werewolf,
             int *death_mask)
{
        int hunter = find_dead_role(werewolf,
                                    *death_mask,
                                    PCX_WEREWOLF_ROLE_HUNTER);

        if (hunter == -1)
                return;

        int target = werewolf->players[hunter].vote;

        if ((*death_mask & (1 << target)))
                return;

        const char *msg = pcx_text_get(werewolf->language,
                                       PCX_TEXT_STRING_HUNTER_KILLS);
        const char *split = strstr(msg, "%s");

        assert(split != NULL);

        pcx_buffer_append(&werewolf->buffer, msg, split - msg);

        pcx_buffer_append_string(&werewolf->buffer,
                                 werewolf->players[target].name);
        pcx_buffer_append_string(&werewolf->buffer, " (");
        append_role(werewolf, werewolf->players[target].card);
        pcx_buffer_append_c(&werewolf->buffer, ')');

        pcx_buffer_append_string(&werewolf->buffer, split + 2);

        pcx_buffer_append_string(&werewolf->buffer, "\n\n");

        *death_mask |= 1 << target;
}

static void
add_no_dead_werewolf_result(struct pcx_werewolf *werewolf,
                            int death_mask)
{
        bool any_player_werewolf =
                any_player_has_role(werewolf,
                                    PCX_WEREWOLF_ROLE_WEREWOLF);

        if (!any_player_werewolf) {
                append_text_string(werewolf, PCX_TEXT_STRING_NO_WEREWOLVES);
                pcx_buffer_append_string(&werewolf->buffer, "\n\n");
        }

        if (find_dead_role(werewolf,
                           death_mask,
                           PCX_WEREWOLF_ROLE_MINION) == -1) {
                bool any_player_minion =
                        any_player_has_role(werewolf,
                                            PCX_WEREWOLF_ROLE_MINION);

                if (any_player_werewolf || any_player_minion) {
                        append_text_string(werewolf,
                                           any_player_werewolf ?
                                           PCX_TEXT_STRING_WEREWOLVES_WIN :
                                           PCX_TEXT_STRING_MINION_WINS);
                } else {
                        append_text_string(werewolf,
                                           PCX_TEXT_STRING_NOBODY_WINS);
                }
        } else {
                if (any_player_werewolf) {
                        append_text_string(werewolf,
                                           PCX_TEXT_STRING_WEREWOLVES_WIN);
                } else {
                        append_text_string(werewolf,
                                           PCX_TEXT_STRING_VILLAGERS_WIN);
                }
        }
}

static void
add_dead_tanner_result(struct pcx_werewolf *werewolf,
                       int death_mask)
{
        bool any_dead_werewolf =
                find_dead_role(werewolf,
                               death_mask,
                               PCX_WEREWOLF_ROLE_WEREWOLF) != -1;

        append_text_string(werewolf,
                           any_dead_werewolf ?
                           PCX_TEXT_STRING_VILLAGE_AND_TANNER_WIN :
                           PCX_TEXT_STRING_TANNER_WINS);
}

static void
add_result_with_death(struct pcx_werewolf *werewolf,
                      int death_mask)
{
        check_hunter(werewolf, &death_mask);

        if (find_dead_role(werewolf,
                           death_mask,
                           PCX_WEREWOLF_ROLE_TANNER) != -1) {
                add_dead_tanner_result(werewolf, death_mask);
        } else if (find_dead_role(werewolf,
                           death_mask,
                           PCX_WEREWOLF_ROLE_WEREWOLF) == -1) {
                add_no_dead_werewolf_result(werewolf, death_mask);
        } else {
                append_text_string(werewolf, PCX_TEXT_STRING_VILLAGERS_WIN);
        }
}

static void
show_vote_results(struct pcx_werewolf *werewolf)
{
        pcx_buffer_set_length(&werewolf->buffer, 0);

        append_text_string(werewolf, PCX_TEXT_STRING_EVERYBODY_VOTED);
        pcx_buffer_append_c(&werewolf->buffer, ' ');
        append_text_string(werewolf, PCX_TEXT_STRING_VOTES_ARE);
        pcx_buffer_append_string(&werewolf->buffer, "\n\n");

        struct vote_count vote_counts[PCX_WEREWOLF_MAX_PLAYERS] = { { 0 } };

        for (int i = 0; i < werewolf->n_players; i++) {
                pcx_buffer_append_string(&werewolf->buffer,
                                         werewolf->players[i].name);
                pcx_buffer_append_c(&werewolf->buffer, ' ');
                enum pcx_werewolf_role role = werewolf->players[i].card;
                pcx_buffer_append_string(&werewolf->buffer,
                                         roles[role].symbol);
                pcx_buffer_append_string(&werewolf->buffer, "👉 ");

                int target = werewolf->players[i].vote;

                pcx_buffer_append_string(&werewolf->buffer,
                                         werewolf->players[target].name);
                pcx_buffer_append_c(&werewolf->buffer, '\n');

                vote_counts[i].player_num = i;
                vote_counts[target].vote_count++;
        }

        qsort(&vote_counts,
              werewolf->n_players,
              sizeof vote_counts[0],
              compare_vote_count_cb);

        int n_deaths;

        for (n_deaths = 1; n_deaths < werewolf->n_players; n_deaths++) {
                if (vote_counts[0].vote_count !=
                    vote_counts[n_deaths].vote_count)
                        break;
        }

        pcx_buffer_append_c(&werewolf->buffer, '\n');

        if (n_deaths >= werewolf->n_players) {
                /* If nobody got more than one vote than no-one dies */
                add_no_one_dies_message(werewolf);
        } else {
                if (n_deaths == 1) {
                        add_one_death_message(werewolf,
                                              vote_counts[0].player_num);
                } else {
                        add_multiple_deaths_message(werewolf,
                                                    vote_counts,
                                                    n_deaths);
                }

                int death_mask = get_death_mask(vote_counts, n_deaths);

                add_result_with_death(werewolf, death_mask);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (const char *) werewolf->buffer.data;

        werewolf->callbacks.send_message(&message, werewolf->user_data);

        werewolf->game_over_source =
                pcx_main_context_add_timeout(NULL,
                                             0, /* ms */
                                             game_over_cb,
                                             werewolf);
}

static void
report_vote_for_self(struct pcx_werewolf *werewolf,
                     int player_num)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_CANT_VOTE_SELF);
        message.target = player_num;
        werewolf->callbacks.send_message(&message, werewolf->user_data);
}

static void
handle_vote_cb(struct pcx_werewolf *werewolf,
               int player_num,
               const char *extra_data)
{
        int vote = extract_int(extra_data);

        if (vote == -1)
                return;

        if (werewolf->current_phase != PCX_N_ELEMENTS(roles))
                return;

        if (vote >= werewolf->n_players)
                return;

        if (vote == player_num) {
                report_vote_for_self(werewolf, player_num);
                return;
        }

        werewolf->players[player_num].vote = vote;

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        if ((werewolf->voted_mask & (1 << player_num))) {
                const char *msg = pcx_text_get(werewolf->language,
                                               PCX_TEXT_STRING_CHANGED_VOTE);
                pcx_buffer_set_length(&werewolf->buffer, 0);
                pcx_buffer_append_printf(&werewolf->buffer,
                                         msg,
                                         werewolf->players[player_num].name);
                message.text = (const char *) werewolf->buffer.data;
                werewolf->callbacks.send_message(&message, werewolf->user_data);
        } else {
                werewolf->voted_mask |= 1 << player_num;

                if (werewolf->voted_mask == (1 << werewolf->n_players) - 1) {
                        show_vote_results(werewolf);
                } else {
                        const char *msg =
                                pcx_text_get(werewolf->language,
                                             PCX_TEXT_STRING_PLAYER_VOTED);
                        pcx_buffer_set_length(&werewolf->buffer, 0);
                        pcx_buffer_append_printf(&werewolf->buffer,
                                                 msg,
                                                 werewolf->players[player_num]
                                                 .name);
                        message.text = (const char *) werewolf->buffer.data;
                        werewolf->callbacks.send_message(&message,
                                                         werewolf->user_data);
                }
        }
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_werewolf *werewolf = user_data;

        assert(player_num >= 0 && player_num < werewolf->n_players);

        const char *colon = strchr(callback_data, ':');
        const char *extra_data;
        size_t command_length;

        if (colon) {
                command_length = colon - callback_data;
                extra_data = colon + 1;
        } else {
                command_length = strlen(callback_data);
                extra_data = NULL;
        }

        static const struct {
                const char *name;
                void (* cb)(struct pcx_werewolf *werewolf,
                            int player_num,
                            const char *extra_data);
        } commands[] = {
                { "vote", handle_vote_cb },
                { "see", handle_see_cb },
                { "wolfsee", handle_wolf_see_cb },
                { "rob", handle_rob_cb },
                { "swap", handle_swap_cb },
                { "take", handle_take_cb },
                { "mode", handle_mode_cb },
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(commands); i++) {
                if (command_length == strlen(commands[i].name) &&
                    !memcmp(callback_data, commands[i].name, command_length)) {
                        commands[i].cb(werewolf, player_num, extra_data);
                        break;
                }
        }
}

static void
free_game_cb(void *user_data)
{
        struct pcx_werewolf *werewolf = user_data;

        for (int i = 0; i < werewolf->n_players; i++)
                pcx_free(werewolf->players[i].name);

        if (werewolf->game_over_source)
                pcx_main_context_remove_source(werewolf->game_over_source);

        if (werewolf->timeout_source)
                pcx_main_context_remove_source(werewolf->timeout_source);

        pcx_buffer_destroy(&werewolf->buffer);

        pcx_free(werewolf->card_overrides);

        pcx_free(werewolf);
}

const struct pcx_game
pcx_werewolf_game = {
        .name = "werewolf",
        .name_string = PCX_TEXT_STRING_NAME_WEREWOLF,
        .start_command = PCX_TEXT_STRING_WEREWOLF_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_WEREWOLF_START_COMMAND_DESCRIPTION,
        .min_players = PCX_WEREWOLF_MIN_PLAYERS,
        .max_players = PCX_WEREWOLF_MAX_PLAYERS,
        .needs_private_messages = true,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
