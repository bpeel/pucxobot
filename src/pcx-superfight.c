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

#include "pcx-superfight.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"
#include "pcx-superfight-help.h"
#include "pcx-superfight-deck.h"

#define PCX_SUPERFIGHT_MIN_PLAYERS 3
#define PCX_SUPERFIGHT_MAX_PLAYERS 6

#define N_CARD_CHOICE 3

/* Time before showing first vote message */
#define SHORT_VOTE_TIMEOUT (30 * 1000)
/* Time before showing subsequent vote messages */
#define LONG_VOTE_TIMEOUT (60 * 1000)

#define POINTS_TO_WIN 3

struct pcx_superfight;

struct pcx_superfight_player {
        char *name;
        int vote;
        int score;
};

struct pcx_superfight_fighter {
        const char *roles[N_CARD_CHOICE];
        const char *attributes[N_CARD_CHOICE];
        const char *chosen_role;
        const char *chosen_attribute;
        const char *forced_attribute;
        int player_num;
        bool complete;
};

struct pcx_superfight {
        struct pcx_superfight_player players[PCX_SUPERFIGHT_MAX_PLAYERS];
        int n_players;
        int current_player;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        struct pcx_main_context_source *game_over_source;
        struct pcx_main_context_source *vote_timeout;
        enum pcx_text_language language;

        struct pcx_superfight_fighter fighters[2];

        struct pcx_superfight_deck *roles;
        struct pcx_superfight_deck *attributes;

        bool sent_first_vote_message;
};

static void
set_vote_timeout(struct pcx_superfight *superfight);

static void
start_argument(struct pcx_superfight *superfight);

static void
append_buffer_string(struct pcx_superfight *superfight,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string)
{
        const char *value = pcx_text_get(superfight->language, string);
        pcx_buffer_append_string(buffer, value);
}

static void
append_buffer_vprintf(struct pcx_superfight *superfight,
                      struct pcx_buffer *buffer,
                      enum pcx_text_string string,
                      va_list ap)
{
        const char *format = pcx_text_get(superfight->language, string);
        pcx_buffer_append_vprintf(buffer, format, ap);
}

static void
append_buffer_printf(struct pcx_superfight *superfight,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string,
                     ...)
{
        va_list ap;
        va_start(ap, string);
        append_buffer_vprintf(superfight, buffer, string, ap);
        va_end(ap);
}

static void
game_note(struct pcx_superfight *superfight,
          enum pcx_text_string format,
          ...)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        va_list ap;

        va_start(ap, format);
        append_buffer_vprintf(superfight, &buf, format, ap);
        va_end(ap);

        superfight->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                           (const char *) buf.data,
                                           0, /* n_buttons */
                                           NULL, /* buttons */
                                           superfight->user_data);

        pcx_buffer_destroy(&buf);
}

static void
remove_vote_timeout(struct pcx_superfight *superfight)
{
        if (superfight->vote_timeout == NULL)
                return;

        pcx_main_context_remove_source(superfight->vote_timeout);
        superfight->vote_timeout = NULL;
}

static void
free_game(struct pcx_superfight *superfight)
{
        for (int i = 0; i < superfight->n_players; i++)
                pcx_free(superfight->players[i].name);

        pcx_superfight_deck_free(superfight->roles);
        pcx_superfight_deck_free(superfight->attributes);

        remove_vote_timeout(superfight);

        if (superfight->game_over_source)
                pcx_main_context_remove_source(superfight->game_over_source);

        pcx_free(superfight);
}

static void
count_votes(struct pcx_superfight *superfight,
            int *votes)
{
        for (unsigned i = 0; i < superfight->n_players; i++) {
                if (superfight->players[i].vote != -1)
                        votes[superfight->players[i].vote]++;
        }
}

static void
append_votes_for_fighter(struct pcx_superfight *superfight,
                         struct pcx_buffer *buf,
                         int fighter,
                         int n_votes)
{
        int vote_num = 0;

        for (unsigned i = 0; i < superfight->n_players; i++) {
                if (superfight->players[i].vote != fighter)
                        continue;

                if (vote_num > 0) {
                        if (vote_num == n_votes - 1) {
                                enum pcx_text_string s =
                                        PCX_TEXT_STRING_FINAL_CONJUNCTION;
                                append_buffer_string(superfight, buf, s);
                        } else {
                                pcx_buffer_append_string(buf, ",");
                        }
                }

                pcx_buffer_append_string(buf, superfight->players[i].name);

                vote_num++;
        }
}

static void
append_current_votes(struct pcx_superfight *superfight,
                     struct pcx_buffer *buf)
{
        int votes[PCX_N_ELEMENTS(superfight->fighters)] = { 0 };

        count_votes(superfight, votes);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(votes); i++) {
                struct pcx_superfight_player *player =
                        superfight->players +
                        superfight->fighters[i].player_num;
                pcx_buffer_append_printf(buf, "%s: ", player->name);

                if (votes[i] == 0) {
                        append_buffer_string(superfight,
                                             buf,
                                             PCX_TEXT_STRING_NOONE);
                } else {
                        append_votes_for_fighter(superfight, buf, i, votes[i]);
                }

                pcx_buffer_append_string(buf, "\n");
        }
}

static void
get_vote_buttons(struct pcx_superfight *superfight,
                 struct pcx_game_button *buttons)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(superfight->fighters); i++) {
                struct pcx_superfight_player *player =
                        superfight->players +
                        superfight->fighters[i].player_num;

                buttons[i].text = player->name;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "vote:%i", i);
                buttons[i].data = (char *) buf.data;
        }
}

static void
vote_timeout_cb(struct pcx_main_context_source *source,
                void *user_data)
{
        struct pcx_superfight *superfight = user_data;

        superfight->vote_timeout = NULL;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (superfight->sent_first_vote_message) {
                append_buffer_string(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_DONT_FORGET_TO_VOTE);
                pcx_buffer_append_string(&buf, "\n\n");
                append_current_votes(superfight, &buf);
        } else {
                append_buffer_string(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_YOU_CAN_VOTE);
                superfight->sent_first_vote_message = true;
        }

        struct pcx_game_button buttons[PCX_N_ELEMENTS(superfight->fighters)];

        get_vote_buttons(superfight, buttons);

        superfight->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                           (const char *) buf.data,
                                           PCX_N_ELEMENTS(buttons),
                                           buttons,
                                           superfight->user_data);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(buttons); i++)
                pcx_free((char *) buttons[i].data);

        pcx_buffer_destroy(&buf);

        set_vote_timeout(superfight);
}

static void
set_vote_timeout(struct pcx_superfight *superfight)
{
        long ms = (superfight->sent_first_vote_message ?
                   LONG_VOTE_TIMEOUT :
                   SHORT_VOTE_TIMEOUT);

        remove_vote_timeout(superfight);

        superfight->vote_timeout = pcx_main_context_add_timeout(NULL,
                                                                ms,
                                                                vote_timeout_cb,
                                                                superfight);
}

static void
append_card_choice(struct pcx_superfight *superfight,
                   struct pcx_buffer *buf,
                   enum pcx_text_string note,
                   const char **cards)
{
        append_buffer_string(superfight, buf, note);
        pcx_buffer_append_string(buf, "\n\n");

        for (unsigned i = 0; i < N_CARD_CHOICE; i++)
                pcx_buffer_append_printf(buf, "%c) %s\n", 'A' + i, cards[i]);

        pcx_buffer_append_string(buf, "\n");
}

static void
get_card_buttons(struct pcx_superfight *superfight,
                 const char **cards,
                 const char *keyword,
                 struct pcx_game_button *buttons)
{
        for (unsigned i = 0; i < N_CARD_CHOICE; i++) {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&buf, "%c", 'A' + i);
                buttons[i].text = (char *) buf.data;

                pcx_buffer_init(&buf);
                pcx_buffer_append_printf(&buf, "%s:%u", keyword, i);
                buttons[i].data = (char *) buf.data;
        }
}

static void
append_fighter(struct pcx_superfight_fighter *fighter,
               struct pcx_buffer *buf)
{
        pcx_buffer_append_string(buf, fighter->chosen_role);

        if (fighter->chosen_attribute) {
                pcx_buffer_append_c(buf, '\n');
                pcx_buffer_append_string(buf, fighter->chosen_attribute);
        }

        if (fighter->forced_attribute) {
                pcx_buffer_append_c(buf, '\n');
                pcx_buffer_append_string(buf, fighter->forced_attribute);
        }
}

static void
send_chosen_fighter(struct pcx_superfight *superfight,
                    struct pcx_superfight_fighter *fighter)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_string(superfight, &buf, PCX_TEXT_STRING_YOUR_FIGHTER_IS);

        pcx_buffer_append_string(&buf, "\n\n");

        append_fighter(fighter, &buf);

        enum pcx_game_message_format format = PCX_GAME_MESSAGE_FORMAT_PLAIN;

        superfight->callbacks.send_private_message(fighter->player_num,
                                                   format,
                                                   (const char *) buf.data,
                                                   0, /* n_buttons */
                                                   NULL, /* buttons */
                                                   superfight->user_data);

        pcx_buffer_destroy(&buf);
}

static void
send_card_choice(struct pcx_superfight *superfight,
                 struct pcx_superfight_fighter *fighter)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (fighter->chosen_role == NULL) {
                append_card_choice(superfight,
                                   &buf,
                                   PCX_TEXT_STRING_POSSIBLE_ROLES,
                                   fighter->roles);
        }

        append_card_choice(superfight,
                           &buf,
                           PCX_TEXT_STRING_POSSIBLE_ATTRIBUTES,
                           fighter->attributes);

        struct pcx_game_button buttons[N_CARD_CHOICE];

        if (fighter->chosen_role == NULL) {
                append_buffer_string(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_CHOOSE_ROLE);
                get_card_buttons(superfight, fighter->roles, "role", buttons);
        } else {
                append_buffer_string(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_CHOOSE_ATTRIBUTE);
                get_card_buttons(superfight,
                                 fighter->attributes,
                                 "attribute",
                                 buttons);
        }

        enum pcx_game_message_format format = PCX_GAME_MESSAGE_FORMAT_PLAIN;

        superfight->callbacks.send_private_message(fighter->player_num,
                                                   format,
                                                   (const char *) buf.data,
                                                   N_CARD_CHOICE,
                                                   buttons,
                                                   superfight->user_data);

        for (unsigned i = 0; i < PCX_N_ELEMENTS(buttons); i++) {
                pcx_free((char *) buttons[i].text);
                pcx_free((char *) buttons[i].data);
        }

        pcx_buffer_destroy(&buf);
}

static void
draw_cards(struct pcx_superfight_deck *deck,
           const char **cards,
           size_t n_cards)
{
        for (size_t i = 0; i < n_cards; i++)
                cards[i] = pcx_superfight_deck_draw_card(deck);
}

static void
start_fight(struct pcx_superfight *superfight)
{
        game_note(superfight,
                  PCX_TEXT_STRING_FIGHTERS_ARE,
                  superfight->players[superfight->fighters[0].player_num].name,
                  superfight->players[superfight->fighters[1].player_num].name);

        for (unsigned fighter_num = 0;
             fighter_num < PCX_N_ELEMENTS(superfight->fighters);
             fighter_num++) {
                struct pcx_superfight_fighter *fighter =
                        superfight->fighters + fighter_num;

                if (fighter->complete)
                        continue;

                draw_cards(superfight->roles,
                           fighter->roles,
                           PCX_N_ELEMENTS(fighter->roles));
                draw_cards(superfight->attributes,
                           fighter->attributes,
                           PCX_N_ELEMENTS(fighter->attributes));

                fighter->chosen_role = NULL;
                fighter->chosen_attribute = NULL;

                send_card_choice(superfight, fighter);
        }
}

static void *
create_game_cb(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        assert(n_players > 0 && n_players <= PCX_SUPERFIGHT_MAX_PLAYERS);

        struct pcx_superfight *superfight = pcx_calloc(sizeof *superfight);

        superfight->language = language;
        superfight->callbacks = *callbacks;
        superfight->user_data = user_data;

        superfight->n_players = n_players;
        superfight->current_player = rand() % n_players;
        superfight->fighters[0].player_num = superfight->current_player;
        superfight->fighters[1].player_num =
                (superfight->current_player + 1) % n_players;

        superfight->roles =
                pcx_superfight_deck_load(language, "roles");
        superfight->attributes =
                pcx_superfight_deck_load(language, "attributes");

        for (unsigned i = 0; i < n_players; i++)
                superfight->players[i].name = pcx_strdup(names[i]);

        start_fight(superfight);

        return superfight;
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_superfight_help[language]);
}

static struct pcx_superfight_fighter *
find_fighter(struct pcx_superfight *superfight,
             int player_num)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(superfight->fighters); i++) {
                if (superfight->fighters[i].player_num == player_num)
                        return superfight->fighters + i;
        }

        return NULL;
}

static bool
all_roles_chosen(struct pcx_superfight *superfight)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(superfight->fighters); i++) {
                struct pcx_superfight_fighter *fighter =
                        superfight->fighters + i;

                if (!fighter->complete)
                        return false;
        }

        return true;
}

static void
start_argument(struct pcx_superfight *superfight)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_string(superfight, &buf, PCX_TEXT_STRING_FIGHTERS_CHOSEN);

        pcx_buffer_append_string(&buf, "\n\n");

        for (unsigned i = 0; i < PCX_N_ELEMENTS(superfight->fighters); i++) {
                struct pcx_superfight_fighter *fighter =
                        superfight->fighters + i;
                const char *name =
                        superfight->players[fighter->player_num].name;
                pcx_buffer_append_printf(&buf, "🔸 %s:\n\n", name);
                append_fighter(fighter, &buf);
                pcx_buffer_append_string(&buf, "\n\n");
        }

        for (unsigned i = 0; i < superfight->n_players; i++)
                superfight->players[i].vote = -1;

        append_buffer_string(superfight, &buf, PCX_TEXT_STRING_NOW_ARGUE);

        superfight->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                           (const char *) buf.data,
                                           0, /* n_buttons */
                                           NULL, /* buttons */
                                           superfight->user_data);

        pcx_buffer_destroy(&buf);

        superfight->sent_first_vote_message = false;
        set_vote_timeout(superfight);
}

static void
choose_role(struct pcx_superfight *superfight,
            int player_num,
            int extra_data)
{
        struct pcx_superfight_fighter *fighter =
                find_fighter(superfight, player_num);

        if (fighter == NULL)
                return;

        if (fighter->complete ||
            fighter->chosen_role)
                return;

        if (extra_data < 0 || extra_data >= N_CARD_CHOICE)
                return;

        fighter->chosen_role = fighter->roles[extra_data];

        send_card_choice(superfight, fighter);
}

static void
choose_attribute(struct pcx_superfight *superfight,
                 int player_num,
                 int extra_data)
{
        struct pcx_superfight_fighter *fighter =
                find_fighter(superfight, player_num);

        if (fighter == NULL)
                return;

        if (fighter->complete ||
            fighter->chosen_role == NULL ||
            fighter->chosen_attribute != NULL)
                return;

        if (extra_data < 0 || extra_data >= N_CARD_CHOICE)
                return;

        fighter->chosen_attribute = fighter->attributes[extra_data];

        fighter->forced_attribute =
                pcx_superfight_deck_draw_card(superfight->attributes);
        fighter->complete = true;

        send_chosen_fighter(superfight, fighter);

        if (all_roles_chosen(superfight))
            start_argument(superfight);
}

static int
get_winner(struct pcx_superfight *superfight)
{
        int votes[PCX_N_ELEMENTS(superfight->fighters)] = { 0 };

        count_votes(superfight, votes);

        bool is_draw = false;
        int best = 0;

        for (unsigned i = 1; i < PCX_N_ELEMENTS(votes); i++) {
                if (votes[i] == votes[best]) {
                        is_draw = true;
                } else if (votes[i] > votes[best]) {
                        is_draw = false;
                        best = i;
                }
        }

        return is_draw ? -1 : best;
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_superfight *superfight = user_data;
        superfight->game_over_source = NULL;
        superfight->callbacks.game_over(superfight->user_data);
}

static void
start_next_fight(struct pcx_superfight *superfight,
                 int winner)
{
        /* Scan through the players until we find the next one that
         * wasn’t involved in the last fight
         */
        do {
                superfight->current_player =
                        (superfight->current_player + 1) %
                        superfight->n_players;
        } while (find_fighter(superfight, superfight->current_player));

        struct pcx_superfight_fighter *loser =
                superfight->fighters + !winner;

        memset(loser, 0, sizeof *loser);
        loser->player_num = superfight->current_player;

        start_fight(superfight);
}

static void
start_decider_fight(struct pcx_superfight *superfight)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(superfight->fighters); i++) {
                struct pcx_superfight_fighter *fighter =
                        superfight->fighters + i;

                fighter->chosen_role =
                        pcx_superfight_deck_draw_card(superfight->roles);
                fighter->chosen_attribute = NULL;
                fighter->forced_attribute = NULL;
        }

        start_argument(superfight);
}

static void
set_fight_winner(struct pcx_superfight *superfight,
                 int winner)
{
        struct pcx_superfight_player *player =
                superfight->players +
                superfight->fighters[winner].player_num;

        player->score++;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_printf(superfight,
                             &buf,
                             PCX_TEXT_STRING_FIGHT_WINNER_IS,
                             player->name);
        pcx_buffer_append_string(&buf, "\n\n");

        for (unsigned i = 0; i < superfight->n_players; i++) {
                pcx_buffer_append_printf(&buf,
                                         "%s: %i\n",
                                         superfight->players[i].name,
                                         superfight->players[i].score);
        }

        pcx_buffer_append_c(&buf, '\n');

        if (player->score >= POINTS_TO_WIN) {
                pcx_buffer_append_string(&buf, "🏆 ");
                append_buffer_printf(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_WON_1,
                                     player->name);
        } else {
                append_buffer_printf(superfight,
                                     &buf,
                                     PCX_TEXT_STRING_STAYS_ON,
                                     player->name);
        }

        superfight->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                           (const char *) buf.data,
                                           0, /* n_buttons */
                                           NULL, /* buttons */
                                           superfight->user_data);

        pcx_buffer_destroy(&buf);

        if (player->score >= POINTS_TO_WIN) {
                if (superfight->game_over_source == NULL) {
                        superfight->game_over_source =
                                pcx_main_context_add_timeout(NULL,
                                                             0, /* ms */
                                                             game_over_cb,
                                                             superfight);
                }
        } else {
                start_next_fight(superfight, winner);
        }
}

static void
end_argument(struct pcx_superfight *superfight)
{
        remove_vote_timeout(superfight);

        int winner = get_winner(superfight);

        if (winner == -1) {
                game_note(superfight, PCX_TEXT_STRING_FIGHT_EQUAL_RESULT);
                start_decider_fight(superfight);
        } else {
                set_fight_winner(superfight, winner);
        }
}

static bool
voting_is_finished(struct pcx_superfight *superfight)
{
        for (unsigned i = 0; i < superfight->n_players; i++) {
                if (find_fighter(superfight, i) == NULL &&
                    superfight->players[i].vote == -1)
                        return false;
        }

        return true;
}

static void
send_vote(struct pcx_superfight *superfight,
          int player_num,
          int extra_data)
{
        if (!all_roles_chosen(superfight))
                return;

        struct pcx_superfight_fighter *player_fighter =
                find_fighter(superfight, player_num);

        if (player_fighter != NULL)
                return;

        if (extra_data < 0 ||
            extra_data >= PCX_N_ELEMENTS(superfight->fighters))
                return;

        struct pcx_superfight_player *player = superfight->players + player_num;

        player->vote = extra_data;

        struct pcx_superfight_fighter *fighter =
                superfight->fighters + extra_data;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        append_buffer_printf(superfight,
                             &buf,
                             PCX_TEXT_STRING_X_VOTED_Y,
                             player->name,
                             superfight->players[fighter->player_num].name);
        pcx_buffer_append_string(&buf, "\n\n");

        append_buffer_string(superfight,
                             &buf,
                             PCX_TEXT_STRING_CURRENT_VOTES_ARE);
        pcx_buffer_append_string(&buf, "\n\n");
        append_current_votes(superfight, &buf);

        superfight->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_PLAIN,
                                           (const char *) buf.data,
                                           0, /* n_buttons */
                                           NULL, /* buttons */
                                           superfight->user_data);

        pcx_buffer_destroy(&buf);

        if (voting_is_finished(superfight))
                end_argument(superfight);
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_superfight *superfight = user_data;

        assert(player_num >= 0 && player_num < superfight->n_players);

        int extra_data;
        const char *colon = strchr(callback_data, ':');

        if (colon == NULL)
                return;

        char *tail;

        errno = 0;
        extra_data = strtol(colon + 1, &tail, 10);

        if (*tail || errno || extra_data < 0)
                return;

        if (colon <= callback_data)
                return;

        static const struct {
                const char *keyword;
                void (* func)(struct pcx_superfight *superfight,
                              int player_num,
                              int extra_data);
        } action_cbs[] = {
                { "role", choose_role },
                { "attribute", choose_attribute },
                { "vote", send_vote },
        };

        for (unsigned i = 0; i < PCX_N_ELEMENTS(action_cbs); i++) {
                const char *keyword = action_cbs[i].keyword;
                size_t len = strlen(keyword);

                if (colon - callback_data == len &&
                    !memcmp(keyword, callback_data, len)) {
                        action_cbs[i].func(superfight, player_num, extra_data);
                        break;
                }
        }
}

static void
free_game_cb(void *data)
{
        free_game(data);
}

const struct pcx_game
pcx_superfight_game = {
        .name = "superfight",
        .name_string = PCX_TEXT_STRING_NAME_SUPERFIGHT,
        .start_command = PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND,
        .min_players = PCX_SUPERFIGHT_MIN_PLAYERS,
        .max_players = PCX_SUPERFIGHT_MAX_PLAYERS,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
