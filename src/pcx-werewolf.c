/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

struct pcx_werewolf_player {
        char *name;
        enum pcx_werewolf_role role;
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

        struct pcx_main_context_source *timeout_source;
        int current_phase;

        enum pcx_werewolf_role extra_cards[PCX_WEREWOLF_N_EXTRA_CARDS];
        uint32_t available_roles;

        uint32_t voted_mask;
};

struct pcx_werewolf_role_data {
        const char *symbol;
        enum pcx_text_string name;
        void (* phase_cb)(struct pcx_werewolf *werewof);
};

static void
next_phase_cb(struct pcx_main_context_source *source,
              void *user_data);

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

static void
werewolf_phase_cb(struct pcx_werewolf *werewolf)
{
        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;
        message.text = pcx_text_get(werewolf->language,
                                    PCX_TEXT_STRING_WEREWOLF_PHASE);
        werewolf->callbacks.send_message(&message, werewolf->user_data);

        pcx_buffer_set_length(&werewolf->buffer, 0);
        append_text_string(werewolf, PCX_TEXT_STRING_WEREWOLVES_ARE);
        pcx_buffer_append_c(&werewolf->buffer, '\n');

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].role == PCX_WEREWOLF_ROLE_WEREWOLF) {
                        pcx_buffer_append_c(&werewolf->buffer, '\n');
                        pcx_buffer_append_string(&werewolf->buffer,
                                                 werewolf->players[i].name);
                }
        }

        message.text = (const char *) werewolf->buffer.data;

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].role == PCX_WEREWOLF_ROLE_WEREWOLF) {
                        message.target = i;
                        werewolf->callbacks.send_message(&message,
                                                         werewolf->user_data);
                }
        }

        queue_next_phase(werewolf, 10);
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
deal_roles(struct pcx_werewolf *werewolf,
           const enum pcx_werewolf_role *card_overrides)
{
        enum pcx_werewolf_role cards[PCX_WEREWOLF_MAX_PLAYERS +
                                     PCX_WEREWOLF_N_EXTRA_CARDS];
        int n_cards = werewolf->n_players + PCX_WEREWOLF_N_EXTRA_CARDS;

        int card_num = 0;

        /* Add the werewolves */
        for (int i = 0; i < PCX_WEREWOLF_N_WEREWOLVES; i++)
                cards[card_num++] = PCX_WEREWOLF_ROLE_WEREWOLF;

        /* The rest of the cards are villagers */
        while (card_num < n_cards)
                cards[card_num++] = PCX_WEREWOLF_ROLE_VILLAGER;

        /* Shuffle the deck */
        for (int i = n_cards - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                enum pcx_werewolf_role t = cards[j];
                cards[j] = cards[i];
                cards[i] = t;
        }

        if (card_overrides)
                memcpy(cards, card_overrides, n_cards * sizeof cards[0]);

        /* Give a card to each player */
        for (int i = 0; i < werewolf->n_players; i++)
                werewolf->players[i].role = cards[i];

        /* The rest of the cards are extra */
        memcpy(werewolf->extra_cards,
               cards + werewolf->n_players,
               PCX_WEREWOLF_N_EXTRA_CARDS * sizeof cards[0]);

        werewolf->available_roles =
                (1 << PCX_WEREWOLF_ROLE_VILLAGER) |
                (1 << PCX_WEREWOLF_ROLE_WEREWOLF);
}

static void
show_village_roles(struct pcx_werewolf *werewolf)
{
        int role_counts[PCX_N_ELEMENTS(roles)] = { 0, };

        for (int i = 0; i < werewolf->n_players; i++)
                role_counts[werewolf->players[i].role]++;

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
                append_role(werewolf, werewolf->players[i].role);

                struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

                message.text = (const char *) werewolf->buffer.data;
                message.target = i;

                werewolf->callbacks.send_message(&message, werewolf->user_data);
        }
}

static void
next_phase_cb(struct pcx_main_context_source *source,
              void *user_data)
{
        struct pcx_werewolf *werewolf = user_data;

        werewolf->timeout_source = NULL;

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

        werewolf->current_phase = -1;

        werewolf->n_players = n_players;

        for (unsigned i = 0; i < n_players; i++)
                werewolf->players[i].name = pcx_strdup(names[i]);

        deal_roles(werewolf, overrides ? overrides->cards : NULL);
        show_village_roles(werewolf);
        send_roles(werewolf);

        queue_next_phase(werewolf, 10);

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
                if (werewolf->players[i].role == PCX_WEREWOLF_ROLE_WEREWOLF)
                        n_werewolves++;
        }

        const char *message =
                pcx_text_get(werewolf->language,
                             PCX_TEXT_STRING_HOWEVER_MULTIPLE_WEREWOLVES);
        const char *split = strstr(message, "%s");

        assert(split != NULL);

        pcx_buffer_append(&werewolf->buffer, message, split - message);

        for (int i = 0; i < werewolf->n_players; i++) {
                if (werewolf->players[i].role != PCX_WEREWOLF_ROLE_WEREWOLF)
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
                if (werewolf->players[i].role == PCX_WEREWOLF_ROLE_WEREWOLF) {
                        if (werewolf_player == -1) {
                                werewolf_player = i;
                        } else {
                                add_multiple_failed_peace_message(werewolf);
                                return;
                        }
                }
        }

        if (werewolf_player == -1) {
                append_text_string(werewolf, PCX_TEXT_STRING_NO_WEREWOLVES);
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
        enum pcx_werewolf_role dead_role = werewolf->players[dead_player].role;

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

        if (dead_role == PCX_WEREWOLF_ROLE_WEREWOLF)
                append_text_string(werewolf, PCX_TEXT_STRING_VILLAGERS_WIN);
        else
                append_text_string(werewolf, PCX_TEXT_STRING_WEREWOLVES_WIN);
}

static void
add_multiple_deaths_message(struct pcx_werewolf *werewolf,
                            const struct vote_count *dead_votes,
                            int n_dead_votes)
{
        bool had_werewolf = false;

        append_text_string(werewolf, PCX_TEXT_STRING_MULTIPLE_SACRIFICES);
        pcx_buffer_append_string(&werewolf->buffer, "\n\n");

        for (int i = 0; i < n_dead_votes; i++) {
                const struct pcx_werewolf_player *player =
                        werewolf->players + dead_votes[i].player_num;

                pcx_buffer_append_string(&werewolf->buffer, player->name);
                pcx_buffer_append_string(&werewolf->buffer, " (");
                append_role(werewolf, player->role);
                pcx_buffer_append_string(&werewolf->buffer, ")\n");

                if (player->role == PCX_WEREWOLF_ROLE_WEREWOLF)
                        had_werewolf = true;
        }

        pcx_buffer_append_string(&werewolf->buffer, "\n");

        if (had_werewolf)
                append_text_string(werewolf, PCX_TEXT_STRING_VILLAGERS_WIN);
        else
                append_text_string(werewolf, PCX_TEXT_STRING_WEREWOLVES_WIN);
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
                pcx_buffer_append_string(&werewolf->buffer, " 👉 ");

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
        } else if (n_deaths == 1) {
                add_one_death_message(werewolf, vote_counts[0].player_num);
        } else {
                add_multiple_deaths_message(werewolf, vote_counts, n_deaths);
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
