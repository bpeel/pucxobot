/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
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

#include "pcx-fox.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <strings.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-fox-help.h"

#define PCX_FOX_N_PLAYERS 2

#define PCX_FOX_N_SUITS 3
#define PCX_FOX_N_CARDS_IN_SUIT 11
#define PCX_FOX_N_CARDS (PCX_FOX_N_SUITS * PCX_FOX_N_CARDS_IN_SUIT)
#define PCX_FOX_HAND_SIZE 13
#define PCX_FOX_END_POINTS 21

typedef uint8_t pcx_fox_card_t;

#define PCX_FOX_CARD_SUIT(c) ((enum pcx_fox_suit) ((c) >> 4))
#define PCX_FOX_CARD_VALUE(c) ((int) ((c) & 0xf))
#define PCX_FOX_MAKE_CARD(suit, value) \
        ((pcx_fox_card_t) (((suit) << 4) | (value)))

#define PCX_FOX_CARD_LOSE_BUT_LEAD 1
#define PCX_FOX_CARD_EXCHANGE 3
#define PCX_FOX_CARD_DRAW 5
#define PCX_FOX_CARD_POINT 7
#define PCX_FOX_CARD_OVERRIDE_TRUMP 9
#define PCX_FOX_CARD_FORCE_BEST_CARD 11

enum pcx_fox_suit {
        PCX_FOX_SUIT_BELLS,
        PCX_FOX_SUIT_KEYS,
        PCX_FOX_SUIT_MOONS,
};

struct pcx_fox_player {
        char *name;
        int score;
        int tricks_this_round;
        /* The hand is stored as a bit mask of values for each suit.
         * That way the hand automatically keeps itself in order and
         * it’s easy to check if a player has a card for a particular
         * suit. The bits are the card values, so bit 0 is not used
         * and bit 1 is the bit for the card with the value 1 etc.
         */
        uint16_t hand[PCX_FOX_N_SUITS];
};

struct pcx_fox {
        /* Number of cards in each player’s hand */
        int n_cards_in_hand;

        int n_cards_in_deck;
        pcx_fox_card_t deck[PCX_FOX_N_CARDS];

        /* The player that dealt this round */
        int dealer;
        /* The first player to play a card in this trick */
        int leader;
        /* The number of cards played in this trick (0, 1 or 2) */
        int n_cards_played;
        /* The cards that were played in the order they were played
         * (ie, the card at index 0 is also the lead card).
         */
        pcx_fox_card_t played_cards[PCX_FOX_N_PLAYERS];
        /* If true, then we are currently resolving an action needed
         * by a card, so new cards can’t be played. This happens after
         * the 3 and 5 cards.
         */
        bool resolving_card;

        pcx_fox_card_t trump_card;

        struct pcx_fox_player players[PCX_FOX_N_PLAYERS];
        struct pcx_game_callbacks callbacks;
        void *user_data;
        enum pcx_text_language language;
        struct pcx_main_context_source *game_over_source;

        /* Options for unit testing */
        int (* rand_func)(void);
};

static const char *
suit_symbols[] = {
        [PCX_FOX_SUIT_BELLS] = "🔔",
        [PCX_FOX_SUIT_KEYS] = "🗝",
        [PCX_FOX_SUIT_MOONS] = "🌜",
};

/* For cards that have a special power (the odd cards), there is a
 * symbol to help memorise it.
 */
static const char *
value_symbols[] = {
        [PCX_FOX_CARD_LOSE_BUT_LEAD] = "🔼",
        [PCX_FOX_CARD_EXCHANGE] = "🔄",
        [PCX_FOX_CARD_DRAW] = "📤",
        [PCX_FOX_CARD_POINT] = "💎",
        [PCX_FOX_CARD_OVERRIDE_TRUMP] = "🎩",
        [PCX_FOX_CARD_FORCE_BEST_CARD] = "↕️",
};

static int
get_highest_value_bit(uint16_t value_mask)
{
        for (uint16_t bit = 1 << PCX_FOX_N_CARDS_IN_SUIT; bit; bit >>= 1) {
                if ((value_mask & bit))
                        return bit;
        }

        return 0;
}

static bool
mask_has_card(const uint16_t *card_mask,
              pcx_fox_card_t card)
{
        enum pcx_fox_suit suit = PCX_FOX_CARD_SUIT(card);
        int value = PCX_FOX_CARD_VALUE(card);

        return (card_mask[suit] & (UINT16_C(1) << value)) != 0;
}

static int
get_next_card_player(struct pcx_fox *fox)
{
        assert(fox->n_cards_played < PCX_FOX_N_PLAYERS);
        return (fox->leader + fox->n_cards_played) % PCX_FOX_N_PLAYERS;
}

static int
get_last_card_player(struct pcx_fox *fox)
{
        assert(fox->n_cards_played > 0);
        return (fox->leader + fox->n_cards_played - 1) % PCX_FOX_N_PLAYERS;
}

static void
get_playable_cards(struct pcx_fox *fox,
                   int player_num,
                   uint16_t *cards)
{
        const uint16_t *hand = fox->players[player_num].hand;

        enum pcx_fox_suit lead_suit = PCX_FOX_CARD_SUIT(fox->played_cards[0]);

        /* If the player is leading the trick they can play any cards
         * they have. This is also the case if they don’t have any
         * cards of the lead suit.
         */
        if (fox->leader == player_num ||
            hand[lead_suit] == 0) {
                memcpy(cards, hand, sizeof fox->players[player_num].hand);
                return;
        }

        memset(cards, 0, sizeof fox->players[player_num].hand);

        /* If the lead card is 11 then they have to player their
         * highest card or the 1.
         */
        if (PCX_FOX_CARD_VALUE(fox->played_cards[0]) ==
            PCX_FOX_CARD_FORCE_BEST_CARD) {
                cards[lead_suit] =
                        (get_highest_value_bit(hand[lead_suit]) |
                         (hand[lead_suit] & (UINT16_C(1) << 1)));
        } else {
                cards[lead_suit] = hand[lead_suit];
        }
}

static void
add_card_to_hand(struct pcx_fox_player *player,
                 pcx_fox_card_t card)
{
        enum pcx_fox_suit suit = PCX_FOX_CARD_SUIT(card);
        int value = PCX_FOX_CARD_VALUE(card);

        player->hand[suit] |= 1 << value;
}

static void
remove_card_from_hand(struct pcx_fox_player *player,
                      pcx_fox_card_t card)
{
        enum pcx_fox_suit suit = PCX_FOX_CARD_SUIT(card);
        int value = PCX_FOX_CARD_VALUE(card);

        player->hand[suit] &= ~(UINT16_C(1) << value);
}

static void
deal(struct pcx_fox *fox)
{
        pcx_fox_card_t *p = fox->deck;

        for (unsigned suit = 0; suit < PCX_FOX_N_SUITS; suit++) {
                for (unsigned value = 1;
                     value <= PCX_FOX_N_CARDS_IN_SUIT;
                     value++)
                        *(p++) = PCX_FOX_MAKE_CARD(suit, value);
        }

        assert(p == fox->deck + PCX_FOX_N_CARDS);

        for (unsigned i = PCX_FOX_N_CARDS - 1; i > 0; i--) {
                unsigned j = fox->rand_func() % (i + 1);
                pcx_fox_card_t t = fox->deck[j];
                fox->deck[j] = fox->deck[i];
                fox->deck[i] = t;
        }

        for (int player = 0; player < PCX_FOX_N_PLAYERS; player++) {
                memset(fox->players[player].hand,
                       0,
                       sizeof fox->players[player].hand);

                for (int card = 0; card < PCX_FOX_HAND_SIZE; card++)
                        add_card_to_hand(fox->players + player, *(--p));

                fox->players[player].tricks_this_round = 0;
        }

        fox->trump_card = *(--p);

        fox->n_cards_in_hand = PCX_FOX_HAND_SIZE,
        fox->n_cards_in_deck = (PCX_FOX_N_CARDS -
                                PCX_FOX_HAND_SIZE *
                                PCX_FOX_N_PLAYERS -
                                1);
        fox->dealer ^= 1;
        fox->leader = fox->dealer;

        assert(fox->n_cards_in_deck == p - fox->deck);
}

static void
add_card(struct pcx_buffer *buf,
         pcx_fox_card_t card)
{
        enum pcx_fox_suit suit = PCX_FOX_CARD_SUIT(card);
        int value = PCX_FOX_CARD_VALUE(card);

        pcx_buffer_append_printf(buf,
                                 "%s%i",
                                 suit_symbols[suit],
                                 value);

        if (value_symbols[value])
                pcx_buffer_append_string(buf, value_symbols[value]);

}

static char *
get_button_data_for_card(const char *keyword,
                         pcx_fox_card_t card)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        pcx_buffer_append_printf(&buf, "%s:%i", keyword, (int) card);
        return (char *) buf.data;
}

static int
get_buttons_for_mask(const uint16_t *card_mask,
                     const char *keyword,
                     struct pcx_game_button *buttons)
{
        int n_buttons = 0;

        for (int suit = 0; suit < PCX_FOX_N_SUITS; suit++) {
                uint16_t values = card_mask[suit];

                while (values) {
                        int value = ffs(values) - 1;

                        values &= ~(1 << value);

                        int card = PCX_FOX_MAKE_CARD(suit, value);

                        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                        add_card(&buf, card);

                        buttons[n_buttons].text = (char *) buf.data;
                        buttons[n_buttons].data =
                                get_button_data_for_card(keyword, card);

                        n_buttons++;
                }
        }

        return n_buttons;
}

static void
add_hand(struct pcx_buffer *buf,
         const struct pcx_fox_player *player)
{
        for (int suit = 0; suit < PCX_FOX_N_SUITS; suit++) {
                uint16_t values = player->hand[suit];
                int n_cards = 0;

                while (values) {
                        int value = ffs(values) - 1;

                        values &= ~(1 << value);

                        if (n_cards++ > 0)
                                pcx_buffer_append_c(buf, ' ');

                        int card = PCX_FOX_MAKE_CARD(suit, value);

                        add_card(buf, card);
                }

                if (n_cards > 0)
                        pcx_buffer_append_string(buf, "\n");
        }
}

static void
show_card_question(struct pcx_fox *fox,
                   int player_num)
{
        struct pcx_game_button buttons[PCX_FOX_HAND_SIZE];
        uint16_t playable_cards[PCX_FOX_N_SUITS];

        get_playable_cards(fox, player_num, playable_cards);

        int n_buttons = get_buttons_for_mask(playable_cards, "play", buttons);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (player_num != fox->leader) {
                const char *note = pcx_text_get(fox->language,
                                                PCX_TEXT_STRING_YOUR_CARDS_ARE);
                pcx_buffer_append_string(&buf, note);
                pcx_buffer_append_c(&buf, '\n');
                add_hand(&buf, fox->players + player_num);
                pcx_buffer_append_string(&buf, "\n");
        }

        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_WHICH_CARD_TO_PLAY);
        pcx_buffer_append_string(&buf, note);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.n_buttons = n_buttons;
        message.buttons = buttons;
        message.target = player_num;

        fox->callbacks.send_message(&message, fox->user_data);

        for (int i = 0; i < n_buttons; i++) {
                pcx_free((char *) buttons[i].data);
                pcx_free((char *) buttons[i].text);
        }

        pcx_buffer_destroy(&buf);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_fox *fox = user_data;
        fox->game_over_source = NULL;
        fox->callbacks.game_over(fox->user_data);
}

static void
end_game(struct pcx_fox *fox)
{
        if (fox->game_over_source == NULL) {
                fox->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     fox);
        }
}

static int
points_for_tricks(int tricks)
{
        if (tricks <= 3)
                return 6; /* humble */
        if (tricks <= 6)
                return tricks - 3; /* defeated */
        if (tricks <= 9)
                return 6; /* victorious */

        return 0; /* greedy */
}

static void
show_round_end(struct pcx_fox *fox,
               bool *game_is_over)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(fox->language,
                                              PCX_TEXT_STRING_ROUND_OVER));

        pcx_buffer_append_string(&buf, "\n\n");

        const struct pcx_fox_player *end_player = NULL;
        const struct pcx_fox_player *winner = fox->players + 0;

        for (int i = 0; i < PCX_FOX_N_PLAYERS; i++) {
                struct pcx_fox_player *player = fox->players + i;

                int points = points_for_tricks(player->tricks_this_round);

                player->score += points;

                pcx_buffer_append_printf(&buf,
                                         "%s: %i (+ %i)\n",
                                         player->name,
                                         player->score,
                                         points);

                player->tricks_this_round = 0;

                if (player->score >= PCX_FOX_END_POINTS)
                        end_player = player;
                if (player->score > winner->score)
                        winner = player;
        }

        if (end_player) {
                pcx_buffer_append_string(&buf, "\n");
                const char *note = pcx_text_get(fox->language,
                                                PCX_TEXT_STRING_END_POINTS);
                pcx_buffer_append_printf(&buf,
                                         note,
                                         end_player->name,
                                         PCX_FOX_END_POINTS);
                pcx_buffer_append_string(&buf, "\n\n");
                note = pcx_text_get(fox->language,
                                    PCX_TEXT_STRING_WINS_PLAIN);
                pcx_buffer_append_printf(&buf,
                                         note,
                                         winner->name);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        *game_is_over = end_player != NULL;
}

static void
start_round(struct pcx_fox *fox)
{
        if (fox->n_cards_in_hand <= 0) {
                bool game_is_over;

                show_round_end(fox, &game_is_over);

                if (game_is_over) {
                        end_game(fox);
                        return;
                }

                deal(fox);
        }

        fox->n_cards_played = 0;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_TRUMP_CARD_IS);
        pcx_buffer_append_string(&buf, note);
        pcx_buffer_append_c(&buf, ' ');
        add_card(&buf, fox->trump_card);
        pcx_buffer_append_string(&buf, "\n\n");

        note = pcx_text_get(fox->language,
                            PCX_TEXT_STRING_YOU_ARE_LEADER);
        pcx_buffer_append_printf(&buf, note, fox->players[fox->leader].name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        show_card_question(fox, fox->leader);
}

struct pcx_fox *
pcx_fox_new(const struct pcx_game_callbacks *callbacks,
            void *user_data,
            enum pcx_text_language language,
            int n_players,
            const char *const *names,
            const struct pcx_fox_debug_overrides *overrides)
{
        assert(n_players == PCX_FOX_N_PLAYERS);

        struct pcx_fox *fox = pcx_calloc(sizeof *fox);

        for (unsigned i = 0; i < n_players; i++)
                fox->players[i].name = pcx_strdup(names[i]);

        fox->language = language;
        fox->callbacks = *callbacks;
        fox->user_data = user_data;
        fox->rand_func = rand;

        if (overrides) {
                if (overrides->rand_func)
                        fox->rand_func = overrides->rand_func;
        }

        fox->dealer = fox->rand_func() & 1;

        deal(fox);

        start_round(fox);

        return fox;
}

static void *
create_game_cb(const struct pcx_config *config,
               const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        return pcx_fox_new(callbacks,
                           user_data,
                           language,
                           n_players,
                           names,
                           NULL /* debug_overrides */);
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_fox_help[language]);
}

static void
show_follow_player(struct pcx_fox *fox)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        int follow_player = get_next_card_player(fox);

        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_FOLLOW_PLAYER);
        pcx_buffer_append_printf(&buf, note, fox->players[follow_player].name);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        show_card_question(fox, follow_player);
}

static void
get_trick_winner(struct pcx_fox *fox,
                 int *winning_player_out,
                 pcx_fox_card_t *winning_card_out)
{
        enum pcx_fox_suit trump_suit = PCX_FOX_CARD_SUIT(fox->trump_card);
        int override_card = -1;

        for (int i = 0; i < PCX_FOX_N_PLAYERS; i++) {
                if (PCX_FOX_CARD_VALUE(fox->played_cards[i]) !=
                    PCX_FOX_CARD_OVERRIDE_TRUMP)
                        continue;

                if (override_card != -1)
                        goto no_override;

                override_card = i;
        }

        if (override_card != -1) {
                pcx_fox_card_t card = fox->played_cards[override_card];
                trump_suit = PCX_FOX_CARD_SUIT(card);
        }

no_override: (void) 0;

        enum pcx_fox_suit lead_suit = PCX_FOX_CARD_SUIT(fox->played_cards[0]);

        /* Get a score for each card played */
        int card_scores[PCX_FOX_N_PLAYERS];

        for (int i = 0; i < PCX_FOX_N_PLAYERS; i++) {
                pcx_fox_card_t card = fox->played_cards[i];
                int score = PCX_FOX_CARD_VALUE(card);

                /* The trump suit scores more than anything else */
                if (PCX_FOX_CARD_SUIT(card) == trump_suit)
                        score |= 1 << 17;
                /* Otherwise the lead suit is better than the third suit */
                if (PCX_FOX_CARD_SUIT(card) == lead_suit)
                        score |= 1 << 16;

                card_scores[i] = score;
        }

        int winning_card = 0;

        for (int i = 1; i < PCX_FOX_N_PLAYERS; i++) {
                if (card_scores[i] > card_scores[winning_card])
                        winning_card = i;
        }

        *winning_player_out = (fox->leader + winning_card) % PCX_FOX_N_PLAYERS;
        *winning_card_out = fox->played_cards[winning_card];
}

static void
end_card_play(struct pcx_fox *fox)
{
        if (fox->n_cards_played < PCX_FOX_N_PLAYERS) {
                show_follow_player(fox);
                return;
        }

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        int winner;
        pcx_fox_card_t winning_card;
        get_trick_winner(fox, &winner, &winning_card);

        fox->players[winner].tricks_this_round++;

        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_TRICK_WINNER);
        pcx_buffer_append_printf(&buf,
                                 note,
                                 fox->players[winner].name);

        if (PCX_FOX_CARD_VALUE(winning_card) == PCX_FOX_CARD_POINT) {
                pcx_buffer_append_c(&buf, ' ');
                note = pcx_text_get(fox->language,
                                    PCX_TEXT_STRING_WIN_TRICK_SEVEN);
                pcx_buffer_append_string(&buf, note);
                fox->players[winner].score++;
        }

        pcx_buffer_append_string(&buf, "\n\n");

        note = pcx_text_get(fox->language,
                            PCX_TEXT_STRING_TRICKS_IN_ROUND_ARE);
        pcx_buffer_append_string(&buf, note);

        for (int i = 0; i < PCX_FOX_N_PLAYERS; i++) {
                const struct pcx_fox_player *player = fox->players + i;

                pcx_buffer_append_printf(&buf,
                                         "\n%s: %i",
                                         player->name,
                                         player->tricks_this_round);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        pcx_fox_card_t losing_card =
                fox->played_cards[winner == fox->leader ? 1 : 0];

        if (PCX_FOX_CARD_VALUE(losing_card) == PCX_FOX_CARD_LOSE_BUT_LEAD)
                fox->leader = winner ^ 1;
        else
                fox->leader = winner;

        fox->n_cards_in_hand--;

        start_round(fox);
}

static void
show_exchange_question(struct pcx_fox *fox)
{
        int player_num = get_last_card_player(fox);
        struct pcx_fox_player *player = fox->players + player_num;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        struct pcx_game_button buttons[PCX_FOX_HAND_SIZE];

        buttons[0].text =
                pcx_strdup(pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_DONT_EXCHANGE));
        buttons[0].data = get_button_data_for_card("exchange", fox->trump_card);

        int n_buttons = 1 + get_buttons_for_mask(player->hand,
                                                 "exchange",
                                                 buttons + 1);

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(fox->language,
                                              PCX_TEXT_STRING_TRUMP_CARD_IS));
        pcx_buffer_append_c(&buf, ' ');
        add_card(&buf, fox->trump_card);
        pcx_buffer_append_string(&buf, "\n\n");
        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_WHICH_CARD_EXCHANGE);
        pcx_buffer_append_string(&buf, note);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.target = player_num;
        message.n_buttons = n_buttons;
        message.buttons = buttons;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        for (int i = 0; i < n_buttons; i++) {
                pcx_free((char *) buttons[i].data);
                pcx_free((char *) buttons[i].text);
        }
}

static void
show_draw_question(struct pcx_fox *fox)
{
        int player_num = get_last_card_player(fox);
        struct pcx_fox_player *player = fox->players + player_num;

        assert(fox->n_cards_in_deck >= 1);

        pcx_fox_card_t drawn_card = fox->deck[--fox->n_cards_in_deck];

        add_card_to_hand(player, drawn_card);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        struct pcx_game_button buttons[PCX_FOX_HAND_SIZE];
        int n_buttons = get_buttons_for_mask(player->hand, "discard", buttons);

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(fox->language,
                                              PCX_TEXT_STRING_YOU_DREW));
        pcx_buffer_append_c(&buf, ' ');
        add_card(&buf, drawn_card);
        pcx_buffer_append_string(&buf, "\n\n");
        const char *note = pcx_text_get(fox->language,
                                        PCX_TEXT_STRING_WHICH_CARD_DISCARD);
        pcx_buffer_append_string(&buf, note);

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;
        message.target = player_num;
        message.n_buttons = n_buttons;
        message.buttons = buttons;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        for (int i = 0; i < n_buttons; i++) {
                pcx_free((char *) buttons[i].data);
                pcx_free((char *) buttons[i].text);
        }
}

static void
show_card_play(struct pcx_fox *fox)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        int player_num = get_last_card_player(fox);

        pcx_buffer_append_printf(&buf,
                                 pcx_text_get(fox->language,
                                              PCX_TEXT_STRING_PLAYER_PLAYED),
                                 fox->players[player_num].name);
        pcx_buffer_append_c(&buf, ' ');
        add_card(&buf, fox->played_cards[fox->n_cards_played - 1]);

        pcx_fox_card_t card = fox->played_cards[fox->n_cards_played - 1];
        int value = PCX_FOX_CARD_VALUE(card);

        if (value == PCX_FOX_CARD_EXCHANGE && fox->n_cards_in_hand > 1) {
                const char *note = pcx_text_get(fox->language,
                                                PCX_TEXT_STRING_PLAYED_THREE);
                pcx_buffer_append_string(&buf, "\n\n");
                pcx_buffer_append_string(&buf, note);
                fox->resolving_card = true;
        } else if (value == PCX_FOX_CARD_DRAW && fox->n_cards_in_hand > 1) {
                const char *note = pcx_text_get(fox->language,
                                                PCX_TEXT_STRING_PLAYED_FIVE);
                pcx_buffer_append_string(&buf, "\n\n");
                pcx_buffer_append_string(&buf, note);
                fox->resolving_card = true;
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        if (fox->resolving_card) {
                if (value == PCX_FOX_CARD_EXCHANGE)
                        show_exchange_question(fox);
                else if (value == PCX_FOX_CARD_DRAW)
                        show_draw_question(fox);
        } else {
                end_card_play(fox);
        }
}

static bool
card_num_is_valid(int card_num)
{
        enum pcx_fox_suit suit = PCX_FOX_CARD_SUIT(card_num);
        int value = PCX_FOX_CARD_VALUE(card_num);

        if (suit < 0 || suit >= PCX_FOX_N_SUITS)
                return false;
        if (value < 1 || value > PCX_FOX_N_CARDS)
                return false;

        return true;
}

static void
play_card(struct pcx_fox *fox,
          int player_num,
          int card_num)
{
        if (fox->resolving_card)
                return;
        if (player_num != get_next_card_player(fox))
                return;
        if (!card_num_is_valid(card_num))
                return;

        uint16_t playable_cards[PCX_FOX_N_SUITS];

        get_playable_cards(fox, player_num, playable_cards);

        if (!mask_has_card(playable_cards, card_num))
                return;

        struct pcx_fox_player *player = fox->players + player_num;

        remove_card_from_hand(player, card_num);

        fox->played_cards[fox->n_cards_played++] = card_num;

        show_card_play(fox);
}

static void
discard_card(struct pcx_fox *fox,
             int player_num,
             int card_num)
{
        if (!fox->resolving_card)
                return;
        if (player_num != get_last_card_player(fox))
                return;
        if (!card_num_is_valid(card_num))
                return;

        pcx_fox_card_t card_played = fox->played_cards[fox->n_cards_played - 1];

        if (PCX_FOX_CARD_VALUE(card_played) != PCX_FOX_CARD_DRAW)
                return;

        struct pcx_fox_player *player = fox->players + player_num;

        if (!mask_has_card(player->hand, card_num))
                return;

        fox->resolving_card = false;

        remove_card_from_hand(player, card_num);

        assert(fox->n_cards_in_deck < PCX_N_ELEMENTS(fox->deck));

        /* Put the card at the bottom of the deck */
        memmove(fox->deck + 1,
                fox->deck,
                fox->n_cards_in_deck * sizeof fox->deck[0]);
        fox->deck[0] = card_num;
        fox->n_cards_in_deck++;

        end_card_play(fox);
}

static void
exchange_card(struct pcx_fox *fox,
              int player_num,
              int card_num)
{
        if (!fox->resolving_card)
                return;
        if (player_num != get_last_card_player(fox))
                return;
        if (!card_num_is_valid(card_num))
                return;

        pcx_fox_card_t card_played = fox->played_cards[fox->n_cards_played - 1];

        if (PCX_FOX_CARD_VALUE(card_played) != PCX_FOX_CARD_EXCHANGE)
                return;

        struct pcx_fox_player *player = fox->players + player_num;

        if (!mask_has_card(player->hand, card_num) &&
            card_num != fox->trump_card)
                return;

        fox->resolving_card = false;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (card_num == fox->trump_card) {
                const char *note =
                        pcx_text_get(fox->language,
                                     PCX_TEXT_STRING_DOESNT_EXCHANGE);
                pcx_buffer_append_printf(&buf, note, player->name);
        } else {
                remove_card_from_hand(player, card_num);
                add_card_to_hand(player, fox->trump_card);
                fox->trump_card = card_num;

                const char *note =
                        pcx_text_get(fox->language,
                                     PCX_TEXT_STRING_TRUMP_CARD_IS);
                pcx_buffer_append_printf(&buf, note, player->name);
                pcx_buffer_append_c(&buf, ' ');
                add_card(&buf, card_num);
        }

        struct pcx_game_message message = PCX_GAME_DEFAULT_MESSAGE;

        message.text = (char *) buf.data;

        fox->callbacks.send_message(&message, fox->user_data);

        pcx_buffer_destroy(&buf);

        end_card_play(fox);
}

static bool
is_button(const char *callback_data,
          const char *colon,
          const char *button)
{
        int len = strlen(button);

        return (colon - callback_data == len &&
                !memcmp(button, callback_data, len));
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_fox *fox = user_data;

        assert(player_num >= 0 && player_num < PCX_FOX_N_PLAYERS);

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

        if (is_button(callback_data, colon, "play"))
                play_card(fox, player_num, extra_data);
        else if (is_button(callback_data, colon, "discard"))
                discard_card(fox, player_num, extra_data);
        else if (is_button(callback_data, colon, "exchange"))
                exchange_card(fox, player_num, extra_data);
}

static void
free_game_cb(void *data)
{
        struct pcx_fox *fox = data;

        for (int i = 0; i < PCX_FOX_N_PLAYERS; i++)
                pcx_free(fox->players[i].name);

        if (fox->game_over_source)
                pcx_main_context_remove_source(fox->game_over_source);

        pcx_free(fox);
}

const struct pcx_game
pcx_fox_game = {
        .name = "fox",
        .name_string = PCX_TEXT_STRING_NAME_FOX,
        .start_command = PCX_TEXT_STRING_FOX_START_COMMAND,
        .start_command_description =
        PCX_TEXT_STRING_FOX_START_COMMAND_DESCRIPTION,
        .min_players = PCX_FOX_N_PLAYERS,
        .max_players = PCX_FOX_N_PLAYERS,
        .needs_private_messages = true,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
