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

#include "pcx-coup.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-coup-character.h"
#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"
#include "pcx-coup-help.h"

#define PCX_COUP_MIN_PLAYERS 2
#define PCX_COUP_MAX_PLAYERS 6

#define PCX_COUP_CARDS_PER_CLAN 3
#define PCX_COUP_CARDS_PER_PLAYER 2
#define PCX_COUP_TOTAL_CARDS (PCX_COUP_CLAN_COUNT * \
                              PCX_COUP_CARDS_PER_CLAN)
#define PCX_COUP_START_COINS 2

#define PCX_COUP_STACK_SIZE 8

#define PCX_COUP_WAIT_TIME (60 * 1000)

typedef void
(* pcx_coup_callback_data_func)(struct pcx_coup *coup,
                                int player_num,
                                const char *data,
                                int extra_data);

typedef void
(* pcx_coup_idle_func)(struct pcx_coup *coup);

/* Called just before popping the stack in order to clean up data.
 */
typedef void
(* pcx_coup_stack_destroy_func)(struct pcx_coup *coup);

struct pcx_coup_stack_entry {
        pcx_coup_callback_data_func func;
        pcx_coup_idle_func idle_func;
        pcx_coup_stack_destroy_func destroy_func;
        union {
                int i;
                void *p;
        } data;
};

struct pcx_coup_card {
        enum pcx_coup_character character;
        bool dead;
};

struct pcx_coup_player {
        char *name;
        int coins;
        struct pcx_coup_card cards[PCX_COUP_CARDS_PER_PLAYER];
};

struct pcx_coup {
        enum pcx_coup_character clan_characters[PCX_COUP_CLAN_COUNT];
        enum pcx_coup_character deck[PCX_COUP_TOTAL_CARDS];
        struct pcx_coup_player players[PCX_COUP_MAX_PLAYERS];
        int n_players;
        int n_cards;
        int current_player;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        struct pcx_buffer buffer;
        struct pcx_coup_stack_entry stack[PCX_COUP_STACK_SIZE];
        int stack_pos;
        bool action_taken;
        struct pcx_main_context_source *game_over_source;
        enum pcx_text_language language;
        int (* rand_func)(void);
};

struct coup_text_button {
        enum pcx_text_string text;
        const char *data;
};

static const struct coup_text_button
coup_button = {
        .text = PCX_TEXT_STRING_COUP,
        .data = "coup"
};

static const struct coup_text_button
income_button = {
        .text = PCX_TEXT_STRING_INCOME,
        .data = "income"
};

static const struct coup_text_button
foreign_aid_button = {
        .text = PCX_TEXT_STRING_FOREIGN_AID,
        .data = "foreign_aid"
};

static const struct coup_text_button
tax_button = {
        .text = PCX_TEXT_STRING_TAX,
        .data = "tax"
};

static const struct coup_text_button
assassinate_button = {
        .text = PCX_TEXT_STRING_ASSASSINATE,
        .data = "assassinate"
};

static const struct coup_text_button
exchange_button = {
        .text = PCX_TEXT_STRING_EXCHANGE,
        .data = "exchange"
};

static const struct coup_text_button
steal_button = {
        .text = PCX_TEXT_STRING_STEAL,
        .data = "steal"
};

static const struct coup_text_button
accept_button = {
        .text = PCX_TEXT_STRING_ACCEPT,
        .data = "accept"
};

static const struct coup_text_button
challenge_button = {
        .text = PCX_TEXT_STRING_CHALLENGE,
        .data = "challenge"
};

static const struct coup_text_button
block_button = {
        .text = PCX_TEXT_STRING_BLOCK,
        .data = "block"
};

static struct pcx_coup_stack_entry *
get_stack_top(struct pcx_coup *coup)
{
        return coup->stack + coup->stack_pos - 1;
}

static void
append_buffer_string(struct pcx_coup *coup,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string)
{
        const char *value = pcx_text_get(coup->language, string);
        pcx_buffer_append_string(buffer, value);
}

static void
append_buffer_vprintf(struct pcx_coup *coup,
                      struct pcx_buffer *buffer,
                      enum pcx_text_string string,
                      va_list ap)
{
        const char *format = pcx_text_get(coup->language, string);
        pcx_buffer_append_vprintf(buffer, format, ap);
}

static void
append_buffer_printf(struct pcx_coup *coup,
                     struct pcx_buffer *buffer,
                     enum pcx_text_string string,
                     ...)
{
        va_list ap;
        va_start(ap, string);
        append_buffer_vprintf(coup, buffer, string, ap);
        va_end(ap);
}

static struct pcx_coup_stack_entry *
stack_push(struct pcx_coup *coup,
           pcx_coup_callback_data_func func,
           pcx_coup_idle_func idle_func)
{
        assert(coup->stack_pos < PCX_N_ELEMENTS(coup->stack));

        coup->stack_pos++;

        struct pcx_coup_stack_entry *entry = get_stack_top(coup);

        entry->func = func;
        entry->idle_func = idle_func;
        entry->destroy_func = NULL;
        memset(&entry->data, 0, sizeof entry->data);

        return entry;
}

static void
stack_push_int(struct pcx_coup *coup,
               pcx_coup_callback_data_func func,
               pcx_coup_idle_func idle_func,
               int data)
{
        struct pcx_coup_stack_entry *entry = stack_push(coup, func, idle_func);

        entry->data.i = data;
}

static void
stack_push_pointer(struct pcx_coup *coup,
                   pcx_coup_callback_data_func func,
                   pcx_coup_idle_func idle_func,
                   pcx_coup_stack_destroy_func destroy_func,
                   void *data)
{
        struct pcx_coup_stack_entry *entry = stack_push(coup, func, idle_func);

        entry->destroy_func = destroy_func;
        entry->data.p = data;
}

static void
stack_pop(struct pcx_coup *coup)
{
        assert(coup->stack_pos > 0);

        struct pcx_coup_stack_entry *entry = get_stack_top(coup);

        if (entry->destroy_func)
                entry->destroy_func(coup);

        coup->stack_pos--;
}

static void
free_game(struct pcx_coup *coup)
{
        while (coup->stack_pos > 0)
                stack_pop(coup);

        pcx_buffer_destroy(&coup->buffer);

        for (int i = 0; i < coup->n_players; i++)
                pcx_free(coup->players[i].name);

        if (coup->game_over_source)
                pcx_main_context_remove_source(coup->game_over_source);

        pcx_free(coup);
}

static int
get_stack_data_int(struct pcx_coup *coup)
{
        assert(coup->stack_pos > 0);

        return get_stack_top(coup)->data.i;
}

static void *
get_stack_data_pointer(struct pcx_coup *coup)
{
        assert(coup->stack_pos > 0);

        return get_stack_top(coup)->data.p;
}

static bool
is_alive(const struct pcx_coup_player *player)
{
        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                if (!player->cards[i].dead)
                        return true;
        }

        return false;
}

static bool
is_finished(const struct pcx_coup *coup)
{
        unsigned n_players = 0;

        for (unsigned i = 0; i < coup->n_players; i++) {
                if (is_alive(coup->players + i))
                        n_players++;
        }

        return n_players <= 1;
}

static void
do_idle(struct pcx_coup *coup)
{
        while (coup->action_taken) {
                coup->action_taken = false;

                if (coup->stack_pos <= 0)
                        break;

                struct pcx_coup_stack_entry *entry = get_stack_top(coup);

                if (entry->idle_func == NULL)
                        break;

                entry->idle_func(coup);
        }
}

static void
take_action(struct pcx_coup *coup)
{
        coup->action_taken = true;
}

static void
send_buffer_message_with_buttons_to(struct pcx_coup *coup,
                                    int target_player,
                                    size_t n_buttons,
                                    const struct pcx_game_button *buttons)
{
        const char *msg = (const char *) coup->buffer.data;
        enum pcx_game_message_format format = PCX_GAME_MESSAGE_FORMAT_PLAIN;

        if (target_player < 0) {
                coup->callbacks.send_message(format,
                                             msg,
                                             n_buttons,
                                             buttons,
                                             coup->user_data);
        } else {
                assert(target_player < coup->n_players);
                coup->callbacks.send_private_message(target_player,
                                                     format,
                                                     msg,
                                                     n_buttons,
                                                     buttons,
                                                     coup->user_data);
        }
}

static void
send_buffer_message_with_buttons(struct pcx_coup *coup,
                                 size_t n_buttons,
                                 const struct pcx_game_button *buttons)
{
        send_buffer_message_with_buttons_to(coup,
                                            -1, /* target_player */
                                            n_buttons,
                                            buttons);
}

static void
send_buffer_message_to(struct pcx_coup *coup,
                       int target_player)
{
        send_buffer_message_with_buttons_to(coup,
                                            target_player,
                                            0, /* n_buttons */
                                            NULL /* buttons */);
}

static void
send_buffer_message(struct pcx_coup *coup)
{
        send_buffer_message_with_buttons(coup,
                                         0, /* n_buttons */
                                         NULL /* buttons */);
}

static void
coup_note(struct pcx_coup *coup,
          enum pcx_text_string string,
          ...)
{
        pcx_buffer_set_length(&coup->buffer, 0);

        const char *format = pcx_text_get(coup->language, string);

        va_list ap;

        va_start(ap, string);
        pcx_buffer_append_vprintf(&coup->buffer, format, ap);
        va_end(ap);

        send_buffer_message(coup);
}

static void
make_button(struct pcx_coup *coup,
            struct pcx_game_button *button,
            const struct coup_text_button *text_button)
{
        button->text = pcx_text_get(coup->language, text_button->text);
        button->data = text_button->data;
}

static void
add_button(struct pcx_coup *coup,
           struct pcx_buffer *buffer,
           const struct coup_text_button *text_button)
{
        size_t old_length = buffer->length;
        pcx_buffer_set_length(buffer,
                              old_length + sizeof (struct pcx_game_button));
        make_button(coup,
                    (struct pcx_game_button *) (buffer->data + old_length),
                    text_button);
}

static void
get_buttons(struct pcx_coup *coup,
            struct pcx_buffer *buffer)
{
        const struct pcx_coup_player *player =
                coup->players + coup->current_player;

        if (player->coins >= 10) {
                add_button(coup, buffer, &coup_button);
                return;
        }

        add_button(coup, buffer, &income_button);
        add_button(coup, buffer, &foreign_aid_button);

        if (player->coins >= 7)
                add_button(coup, buffer, &coup_button);

        if (coup->clan_characters[PCX_COUP_CLAN_TAX_COLLECTORS] ==
            PCX_COUP_CHARACTER_DUKE)
                add_button(coup, buffer, &tax_button);

        if (coup->clan_characters[PCX_COUP_CLAN_ASSASSINS] ==
            PCX_COUP_CHARACTER_ASSASSIN &&
            player->coins >= 3)
                add_button(coup, buffer, &assassinate_button);

        if (coup->clan_characters[PCX_COUP_CLAN_NEGOTIATORS] ==
            PCX_COUP_CHARACTER_AMBASSADOR)
                add_button(coup, buffer, &exchange_button);

        if (coup->clan_characters[PCX_COUP_CLAN_THIEVES] ==
            PCX_COUP_CHARACTER_CAPTAIN)
                add_button(coup, buffer, &steal_button);
}

static void
add_cards_status(struct pcx_coup *coup,
                 struct pcx_buffer *buffer,
                 const struct pcx_coup_player *player)
{
        for (unsigned int i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                const struct pcx_coup_card *card = player->cards + i;
                enum pcx_coup_character character = card->character;

                if (card->dead) {
                        enum pcx_text_string name =
                                pcx_coup_characters[character].name;
                        pcx_buffer_append_string(buffer, "☠");
                        append_buffer_string(coup, buffer, name);
                        pcx_buffer_append_string(buffer, "☠");
                } else {
                        pcx_buffer_append_string(buffer, "🂠");
                }
        }
}

static void
add_money_status(struct pcx_coup *coup,
                 struct pcx_buffer *buffer,
                 const struct pcx_coup_player *player)
{
        if (player->coins == 1) {
                append_buffer_string(coup, buffer, PCX_TEXT_STRING_1_COIN);
        } else {
                append_buffer_printf(coup,
                                     buffer,
                                     PCX_TEXT_STRING_PLURAL_COINS,
                        player->coins);
        }
}

static void
show_cards(struct pcx_coup *coup,
           int player_num)
{
        const struct pcx_coup_player *player = coup->players + player_num;

        if (!is_alive(player))
                return;

        pcx_buffer_set_length(&coup->buffer, 0);
        append_buffer_string(coup,
                             &coup->buffer,
                             PCX_TEXT_STRING_YOUR_CARDS_ARE);
        pcx_buffer_append_string(&coup->buffer, "\n");

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                const struct pcx_coup_card *card = player->cards + i;
                enum pcx_text_string name =
                        pcx_coup_characters[card->character].name;

                if (card->dead)
                        pcx_buffer_append_string(&coup->buffer, "☠");

                append_buffer_string(coup, &coup->buffer, name);

                if (card->dead)
                        pcx_buffer_append_string(&coup->buffer, "☠");

                pcx_buffer_append_c(&coup->buffer, '\n');
        }

        pcx_buffer_append_c(&coup->buffer, '\0');

        send_buffer_message_to(coup, player_num);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_coup *coup = user_data;
        coup->game_over_source = NULL;
        coup->callbacks.game_over(coup->user_data);
}

static void
show_stats(struct pcx_coup *coup)
{
        bool finished = is_finished(coup);
        const struct pcx_coup_player *winner = NULL;

        pcx_buffer_set_length(&coup->buffer, 0);

        for (unsigned i = 0; i < coup->n_players; i++) {
                const struct pcx_coup_player *player = coup->players + i;
                bool alive = is_alive(player);

                if (finished) {
                        if (alive)
                                pcx_buffer_append_string(&coup->buffer, "🏆 ");
                } else if (coup->current_player == i) {
                        pcx_buffer_append_string(&coup->buffer, "👉 ");
                }

                pcx_buffer_append_string(&coup->buffer, player->name);
                pcx_buffer_append_string(&coup->buffer, ":\n");

                add_cards_status(coup, &coup->buffer, player);

                if (alive) {
                        pcx_buffer_append_string(&coup->buffer, ", ");
                        add_money_status(coup, &coup->buffer, player);
                        winner = player;
                }

                pcx_buffer_append_string(&coup->buffer, "\n\n");
        }

        if (finished) {
                const char *winner_name;
                if (winner) {
                        winner_name = winner->name;
                } else {
                        winner_name = pcx_text_get(coup->language,
                                                   PCX_TEXT_STRING_NOONE);
                }
                append_buffer_printf(coup,
                                     &coup->buffer,
                                     PCX_TEXT_STRING_WON_1,
                                     winner_name);
        } else {
                const struct pcx_coup_player *current =
                        coup->players + coup->current_player;
                append_buffer_printf(coup,
                                     &coup->buffer,
                                     PCX_TEXT_STRING_YOUR_GO,
                                     current->name);
        }

        struct pcx_buffer buttons = PCX_BUFFER_STATIC_INIT;

        if (!finished)
                get_buttons(coup, &buttons);

        send_buffer_message_with_buttons(coup,
                                         buttons.length /
                                         sizeof (struct pcx_game_button),
                                         (const struct pcx_game_button *)
                                         buttons.data);

        pcx_buffer_destroy(&buttons);

        if (finished && coup->game_over_source == NULL) {
                coup->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     coup);
        }
}

static void
shuffle_deck(struct pcx_coup *coup)
{
        if (coup->n_cards < 2)
                return;

        for (unsigned i = coup->n_cards - 1; i > 0; i--) {
                int j = coup->rand_func() % (i + 1);
                enum pcx_coup_character t = coup->deck[j];
                coup->deck[j] = coup->deck[i];
                coup->deck[i] = t;
        }
}

static enum pcx_coup_character
take_card(struct pcx_coup *coup)
{
        assert(coup->n_cards > 1);
        return coup->deck[--coup->n_cards];
}

static void
choose_card_to_lose(struct pcx_coup *coup,
                    int player_num,
                    const char *data,
                    int extra_data)
{
        if (strcmp(data, "lose"))
                return;

        if (player_num != get_stack_data_int(coup))
                return;

        struct pcx_coup_player *player = coup->players + player_num;

        if (extra_data < 0 ||
            extra_data >= PCX_COUP_CARDS_PER_PLAYER ||
            player->cards[extra_data].dead)
                return;

        take_action(coup);

        player->cards[extra_data].dead = true;
        show_cards(coup, player_num);
        stack_pop(coup);
}

static bool
is_losing_all_cards(struct pcx_coup *coup,
                    int player_num)
{
        const struct pcx_coup_player *player = coup->players + player_num;
        int n_living = 0;

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                if (!player->cards[i].dead)
                        n_living++;
        }

        if (coup->stack_pos < n_living)
                return false;

        for (unsigned i = coup->stack_pos - n_living;
             i < coup->stack_pos;
             i++) {
                const struct pcx_coup_stack_entry *entry = coup->stack + i;

                if (entry->func != choose_card_to_lose ||
                    entry->data.i != player_num)
                        return false;
        }

        return true;
}

static void
choose_card_to_lose_idle(struct pcx_coup *coup)
{
        int player_num = get_stack_data_int(coup);
        struct pcx_coup_player *player = coup->players + player_num;

        /* Check if the stack contains enough lose card entries for
         * the player to lose of their cards. In that case they don’t
         * need the message.
         */
        if (is_losing_all_cards(coup, player_num)) {
                take_action(coup);
                stack_pop(coup);
                bool changed = false;
                for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                        if (!player->cards[i].dead) {
                                player->cards[i].dead = true;
                                changed = true;
                        }
                }
                if (changed)
                        show_cards(coup, player_num);
                return;
        }

        struct pcx_game_button buttons[PCX_COUP_CARDS_PER_PLAYER];
        size_t n_buttons = 0;

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                if (player->cards[i].dead)
                        continue;

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "lose:%u", i);
                enum pcx_text_string name =
                        pcx_coup_characters[player->cards[i].character].name;

                buttons[n_buttons].text =
                        pcx_text_get(coup->language, name);
                buttons[n_buttons].data = (char *) buf.data;
                n_buttons++;
        }

        pcx_buffer_set_length(&coup->buffer, 0);
        append_buffer_string(coup,
                             &coup->buffer,
                             PCX_TEXT_STRING_WHICH_CARD_TO_LOSE);

        send_buffer_message_with_buttons_to(coup,
                                            player_num,
                                            n_buttons,
                                            buttons);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);
}

static void
lose_card(struct pcx_coup *coup,
          int player_num)
{
        stack_push_int(coup,
                       choose_card_to_lose,
                       choose_card_to_lose_idle,
                       player_num);
}

static void
get_challenged_cards(struct pcx_coup *coup,
                     struct pcx_buffer *buf,
                     uint32_t cards)
{
        const char *final_separator =
                pcx_text_get(coup->language,
                             PCX_TEXT_STRING_FINAL_DISJUNCTION);

        for (unsigned i = 0; cards; i++) {
                if ((cards & (UINT32_C(1) << i)) == 0)
                        continue;

                cards &= ~(UINT32_C(1) << i);

                if (buf->length > 0) {
                        if (cards)
                                pcx_buffer_append_string(buf, ", ");
                        else
                                pcx_buffer_append_string(buf, final_separator);
                }

                enum pcx_coup_character character = coup->clan_characters[i];
                enum pcx_text_string name =
                        pcx_coup_characters[character].object_name;

                append_buffer_string(coup, buf, name);
        }
}

static bool
get_single_card(const struct pcx_coup_player *player,
                enum pcx_coup_character *single_card)
{
        bool found_card = false;

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                if (player->cards[i].dead)
                        continue;
                if (found_card)
                        return false;
                found_card = true;
                *single_card = player->cards[i].character;
        }

        return found_card;
}

static void
change_card(struct pcx_coup *coup,
            int player_num,
            enum pcx_coup_character character)
{
        struct pcx_coup_player *player = coup->players + player_num;

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                struct pcx_coup_card *card = player->cards + i;

                if (!card->dead && card->character == character) {
                        coup->deck[coup->n_cards++] = character;
                        shuffle_deck(coup);
                        card->character = take_card(coup);
                        show_cards(coup, player_num);
                        return;
                }
        }

        pcx_fatal("card not found in change_card");
}

typedef void
(* action_cb)(struct pcx_coup *coup,
              void *user_data);

enum challenge_flag {
        CHALLENGE_FLAG_CHALLENGE = (1 << 0),
        CHALLENGE_FLAG_BLOCK = (1 << 1),
};

struct challenge_data {
        action_cb cb;
        void *user_data;
        struct pcx_main_context_source *timeout_source;
        char *message;
        uint32_t accepted_players;
        enum challenge_flag flags;
        int player_num;

        /* If CHALLENGE_FLAG_CHALLENGE is set */
        uint32_t challenged_clans;

        /* If CHALLENGE_FLAG_BLOCK is set */
        uint32_t blocking_clans;
        int target_player;
        action_cb block_cb;
};

struct reveal_data {
        action_cb cb;
        void *user_data;
        int challenging_player;
        int challenged_player;
        uint32_t challenged_clans;
};

static void
do_challenge_action(struct pcx_coup *coup,
                    action_cb cb,
                    void *user_data)
{
        take_action(coup);
        stack_pop(coup);

        cb(coup, user_data);
}

static void
do_reveal(struct pcx_coup *coup,
          struct reveal_data *data,
          enum pcx_coup_character character)
{
        struct pcx_coup_player *challenged_player =
                coup->players + data->challenged_player;
        struct pcx_coup_player *challenging_player =
                coup->players + data->challenging_player;
        enum pcx_coup_clan clan = pcx_coup_characters[character].clan;

        if ((data->challenged_clans & (UINT32_C(1) << clan))) {
                enum pcx_text_string character_name =
                        pcx_coup_characters[character].object_name;
                const char *character_name_string =
                        pcx_text_get(coup->language, character_name);
                coup_note(coup,
                          PCX_TEXT_STRING_CHALLENGE_FAILED,
                          challenging_player->name,
                          challenged_player->name,
                          character_name_string,
                          challenging_player->name);
                change_card(coup, data->challenged_player, character);
                stack_pop(coup);
                struct challenge_data *challenge_data =
                        get_stack_data_pointer(coup);
                challenge_data->flags &= ~CHALLENGE_FLAG_CHALLENGE;
                take_action(coup);
                lose_card(coup, challenging_player - coup->players);
        } else {
                struct pcx_buffer card_buf = PCX_BUFFER_STATIC_INIT;
                get_challenged_cards(coup,
                                     &card_buf,
                                     data->challenged_clans);
                coup_note(coup,
                          PCX_TEXT_STRING_CHALLENGE_SUCCEEDED,
                          challenging_player->name,
                          challenged_player->name,
                          (char *) card_buf.data,
                          challenged_player->name);
                pcx_buffer_destroy(&card_buf);

                for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                        struct pcx_coup_card *card =
                                challenged_player->cards + i;
                        if (card->character == character && !card->dead) {
                                card->dead = true;
                                break;
                        }
                }
                show_cards(coup, data->challenged_player);

                take_action(coup);
                stack_pop(coup);
                /* Also pop the challenge */
                stack_pop(coup);
        }
}

static void
reveal_callback_data(struct pcx_coup *coup,
                     int player_num,
                     const char *command,
                     int extra_data)
{
        struct reveal_data *data = get_stack_data_pointer(coup);

        if (player_num != data->challenged_player)
                return;
        if (strcmp(command, "reveal"))
                return;
        if (extra_data < 0 || extra_data >= PCX_COUP_CARDS_PER_PLAYER)
                return;

        struct pcx_coup_player *player = coup->players + player_num;
        struct pcx_coup_card *card = player->cards + extra_data;

        if (card->dead)
                return;

        do_reveal(coup, data, card->character);
}

static void
reveal_idle(struct pcx_coup *coup)
{
        struct reveal_data *data = get_stack_data_pointer(coup);
        struct pcx_coup_player *challenged_player =
                coup->players + data->challenged_player;
        enum pcx_coup_character single_card;

        if (get_single_card(challenged_player, &single_card)) {
                do_reveal(coup, data, single_card);
                return;
        }

        struct pcx_buffer cards = PCX_BUFFER_STATIC_INIT;
        get_challenged_cards(coup, &cards, data->challenged_clans);

        pcx_buffer_set_length(&coup->buffer, 0);
        append_buffer_printf(coup,
                             &coup->buffer,
                             PCX_TEXT_STRING_ANNOUNCE_CHALLENGE,
                             coup->players[data->challenging_player].name,
                             (char *) cards.data);

        pcx_buffer_destroy(&cards);

        struct pcx_game_button buttons[PCX_COUP_CARDS_PER_PLAYER];
        int n_buttons = 0;

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                const struct pcx_coup_card *card = challenged_player->cards + i;

                if (card->dead)
                        continue;

                enum pcx_text_string name =
                        pcx_coup_characters[card->character].name;

                buttons[n_buttons].text =
                        pcx_text_get(coup->language, name);

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "reveal:%u", i);
                buttons[n_buttons].data = (char *) buf.data;

                n_buttons++;
        }

        send_buffer_message_with_buttons_to(coup,
                                            data->challenged_player,
                                            n_buttons,
                                            buttons);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);
}

static void
reveal_destroy(struct pcx_coup *coup)
{
        struct reveal_data *data = get_stack_data_pointer(coup);

        pcx_free(data);
}

static void
reveal_card(struct pcx_coup *coup,
            int challenging_player,
            int challenged_player,
            uint32_t challenged_clans,
            action_cb cb,
            void *user_data)
{
        struct reveal_data *data = pcx_calloc(sizeof *data);

        data->challenging_player = challenging_player;
        data->challenged_player = challenged_player;
        data->challenged_clans = challenged_clans;
        data->cb = cb;
        data->user_data = user_data;

        stack_push_pointer(coup,
                           reveal_callback_data,
                           reveal_idle,
                           reveal_destroy,
                           data);
}

static struct challenge_data *
check_challenge(struct pcx_coup *coup,
                enum challenge_flag flags,
                int player_num,
                action_cb cb,
                void *user_data,
                enum pcx_text_string string,
                ...);

static void
remove_challenge_timeout(struct challenge_data *data)
{
        if (data->timeout_source) {
                pcx_main_context_remove_source(data->timeout_source);
                data->timeout_source = NULL;
        }
}

static void
do_block(struct pcx_coup *coup,
         void *user_data)
{
        struct challenge_data *data = get_stack_data_pointer(coup);

        if (data->block_cb)
                data->block_cb(coup, data->user_data);

        coup_note(coup, PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK);
        stack_pop(coup);
        take_action(coup);
}

static bool
is_accepted(struct pcx_coup *coup,
            const struct challenge_data *data)
{
        if (data->flags == 0)
                return true;

        uint32_t alive_players = 0;

        for (unsigned i = 0; i < coup->n_players; i++) {
                if (i == data->player_num)
                        continue;

                if (is_alive(coup->players + i))
                        alive_players |= UINT32_C(1) << i;
        }

        return (data->accepted_players & alive_players) == alive_players;
}

static void
check_challenge_callback_data(struct pcx_coup *coup,
                              int player_num,
                              const char *command,
                              int extra_data)
{
        struct challenge_data *data = get_stack_data_pointer(coup);

        if (!strcmp(command, accept_button.data)) {
                data->accepted_players |= UINT32_C(1) << player_num;

                if (is_accepted(coup, data))
                        do_challenge_action(coup, data->cb, data->user_data);
        } else if (!strcmp(command, challenge_button.data)) {
                if ((data->flags & CHALLENGE_FLAG_CHALLENGE) == 0)
                        return;
                if (player_num == data->player_num)
                        return;
                if (!is_alive(coup->players + player_num))
                        return;

                remove_challenge_timeout(data);

                int challenged_player = data->player_num;
                uint32_t challenged_clans = data->challenged_clans;
                action_cb cb = data->cb;
                void *user_data = data->user_data;

                reveal_card(coup,
                            player_num,
                            challenged_player,
                            challenged_clans,
                            cb,
                            user_data);

                take_action(coup);
        } else if (!strcmp(command, block_button.data)) {
                if ((data->flags & CHALLENGE_FLAG_BLOCK) == 0)
                        return;
                if (player_num == data->player_num)
                        return;
                if (!is_alive(coup->players + player_num))
                        return;
                if (data->target_player != -1 &&
                    player_num != data->target_player)
                        return;

                remove_challenge_timeout(data);

                struct pcx_buffer blocked_names = PCX_BUFFER_STATIC_INIT;
                get_challenged_cards(coup,
                                     &blocked_names,
                                     data->blocking_clans);

                struct challenge_data *block_data =
                        check_challenge(coup,
                                        CHALLENGE_FLAG_CHALLENGE,
                                        player_num,
                                        do_block,
                                        NULL, /* user_data */
                                        PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK,
                                        coup->players[player_num].name,
                                        (char *) blocked_names.data);

                pcx_buffer_destroy(&blocked_names);

                block_data->challenged_clans = data->blocking_clans;
        }
}

static void
check_challenge_timeout(struct pcx_main_context_source *source,
                        void *user_data)
{
        struct pcx_coup *coup = user_data;
        struct challenge_data *data = get_stack_data_pointer(coup);

        assert(data->timeout_source == source);

        data->timeout_source = NULL;

        do_challenge_action(coup, data->cb, data->user_data);

        do_idle(coup);
}

static void
check_challenge_idle(struct pcx_coup *coup)
{
        struct challenge_data *data = get_stack_data_pointer(coup);

        /* Restart from zero accepted players every time we ask the
         * question.
         */
        data->accepted_players = 0;

        /* Check if the action is already accepted. This can happen if
         * there was a block that caused a death and we return to this
         * challenge.
         */
        if (is_accepted(coup, data)) {
                do_challenge_action(coup, data->cb, data->user_data);
                return;
        }

        struct pcx_game_button buttons[3];
        int n_buttons = 0;

        if ((data->flags & CHALLENGE_FLAG_CHALLENGE))
                make_button(coup, buttons + n_buttons++, &challenge_button);
        if ((data->flags & CHALLENGE_FLAG_BLOCK))
                make_button(coup, buttons + n_buttons++, &block_button);
        make_button(coup, buttons + n_buttons++, &accept_button);

        pcx_buffer_set_length(&coup->buffer, 0);
        pcx_buffer_append_string(&coup->buffer, data->message);

        if ((data->flags & (CHALLENGE_FLAG_CHALLENGE))) {
                pcx_buffer_append_c(&coup->buffer, '\n');
                append_buffer_printf(coup,
                                     &coup->buffer,
                                     PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE);
        }

        if ((data->flags & (CHALLENGE_FLAG_BLOCK))) {
                struct pcx_buffer blocking_cards = PCX_BUFFER_STATIC_INIT;

                get_challenged_cards(coup,
                                     &blocking_cards,
                                     data->blocking_clans);

                pcx_buffer_append_c(&coup->buffer, '\n');

                if (data->target_player == -1) {
                        enum pcx_text_string format;

                        if ((data->flags & ~(CHALLENGE_FLAG_BLOCK)))
                                format = PCX_TEXT_STRING_OR_BLOCK_NO_TARGET;
                        else
                                format = PCX_TEXT_STRING_BLOCK_NO_TARGET;

                        append_buffer_printf(coup,
                                             &coup->buffer,
                                             format,
                                             (char *) blocking_cards.data);
                } else {
                        enum pcx_text_string format;
                        const struct pcx_coup_player *target_player =
                                coup->players + data->target_player;

                        if ((data->flags & ~(CHALLENGE_FLAG_BLOCK)))
                                format = PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET;
                        else
                                format = PCX_TEXT_STRING_BLOCK_WITH_TARGET;

                        append_buffer_printf(coup,
                                             &coup->buffer,
                                             format,
                                             target_player->name,
                                             (char *) blocking_cards.data);
                }

                pcx_buffer_destroy(&blocking_cards);
        }

        if (data->timeout_source == NULL) {
                data->timeout_source =
                        pcx_main_context_add_timeout(NULL,
                                                     PCX_COUP_WAIT_TIME,
                                                     check_challenge_timeout,
                                                     coup);
        }

        send_buffer_message_with_buttons(coup, n_buttons, buttons);
}

static void
check_challenge_destroy(struct pcx_coup *coup)
{
        struct challenge_data *data = get_stack_data_pointer(coup);

        remove_challenge_timeout(data);
        pcx_free(data->message);
        pcx_free(data);
}

static struct challenge_data *
check_challenge(struct pcx_coup *coup,
                enum challenge_flag flags,
                int player_num,
                action_cb cb,
                void *user_data,
                enum pcx_text_string message,
                ...)
{
        struct challenge_data *data = pcx_calloc(sizeof *data);

        assert((flags & (CHALLENGE_FLAG_CHALLENGE |
                         CHALLENGE_FLAG_BLOCK)) != 0);

        data->flags = flags;
        data->player_num = player_num;
        data->cb = cb;
        data->user_data = user_data;
        data->target_player = -1;

        pcx_buffer_set_length(&coup->buffer, 0);

        va_list ap;
        va_start(ap, message);
        append_buffer_vprintf(coup, &coup->buffer, message, ap);
        va_end(ap);

        data->message = pcx_strdup((char *) coup->buffer.data);

        take_action(coup);

        stack_push_pointer(coup,
                           check_challenge_callback_data,
                           check_challenge_idle, /* idle_cb */
                           check_challenge_destroy,
                           data);

        return data;
}

static void
send_select_target(struct pcx_coup *coup,
                   enum pcx_text_string message,
                   const char *data)
{
        size_t n_buttons = 0;
        struct pcx_game_button buttons[PCX_COUP_MAX_PLAYERS];
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (unsigned i = 0; i < coup->n_players; i++) {
                if (i == coup->current_player || !is_alive(coup->players + i))
                        continue;

                pcx_buffer_set_length(&buf, 0);
                pcx_buffer_append_printf(&buf, "%s:%u", data, i);
                buttons[n_buttons].text = coup->players[i].name;
                buttons[n_buttons].data = pcx_strdup((const char *) buf.data);
                n_buttons++;
        }

        pcx_buffer_set_length(&coup->buffer, 0);
        append_buffer_printf(coup,
                             &coup->buffer,
                             message,
                             coup->players[coup->current_player].name);

        send_buffer_message_with_buttons(coup,
                                         n_buttons,
                                         buttons);

        for (unsigned i = 0; i < n_buttons; i++)
                pcx_free((char *) buttons[i].data);

        pcx_buffer_destroy(&buf);
}

static void
do_coup(struct pcx_coup *coup,
        int extra_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        if (player->coins < 7)
                return;

        if (extra_data == -1) {
                send_select_target(coup,
                                   PCX_TEXT_STRING_WHO_TO_COUP,
                                   coup_button.data);
                return;
        }

        if (extra_data >= coup->n_players || extra_data == coup->current_player)
                return;

        struct pcx_coup_player *target = coup->players + extra_data;

        if (!is_alive(target))
                return;

        take_action(coup);

        coup_note(coup,
                  PCX_TEXT_STRING_DOING_COUP,
                  player->name,
                  target->name);

        player->coins -= 7;
        lose_card(coup, extra_data);
}

static void
do_income(struct pcx_coup *coup)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        coup_note(coup,
                  PCX_TEXT_STRING_DOING_INCOME,
                  player->name);
        player->coins++;

        take_action(coup);
}

static void
do_accepted_foreign_aid(struct pcx_coup *coup,
                        void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        coup_note(coup,
                  PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID,
                  player->name);

        player->coins += 2;

        take_action(coup);
}

static void
do_foreign_add(struct pcx_coup *coup)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        struct challenge_data *data =
                check_challenge(coup,
                                CHALLENGE_FLAG_BLOCK,
                                coup->current_player,
                                do_accepted_foreign_aid,
                                NULL, /* user_data */
                                PCX_TEXT_STRING_DOING_FOREIGN_AID,
                                player->name);

        data->blocking_clans = (1 << PCX_COUP_CLAN_TAX_COLLECTORS);
}

static void
do_accepted_tax(struct pcx_coup *coup,
                void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        coup_note(coup,
                  PCX_TEXT_STRING_REALLY_DOING_TAX,
                  player->name);

        player->coins += 3;

        take_action(coup);
}

static void
do_tax(struct pcx_coup *coup)
{
        if (coup->clan_characters[PCX_COUP_CLAN_TAX_COLLECTORS] !=
            PCX_COUP_CHARACTER_DUKE)
                return;

        struct pcx_coup_player *player = coup->players + coup->current_player;

        struct challenge_data *data =
                check_challenge(coup,
                                CHALLENGE_FLAG_CHALLENGE,
                                coup->current_player,
                                do_accepted_tax,
                                NULL, /* user_data */
                                PCX_TEXT_STRING_DOING_TAX,
                                player->name);

        data->challenged_clans = (1 << PCX_COUP_CLAN_TAX_COLLECTORS);
}

static void
block_assassinate(struct pcx_coup *coup,
                  void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        player->coins -= 3;
}

static void
do_accepted_assassinate(struct pcx_coup *coup,
                        void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;
        struct pcx_coup_player *target = user_data;

        coup_note(coup,
                  PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION,
                  player->name,
                  target->name);

        player->coins -= 3;
        lose_card(coup, target - coup->players);

        take_action(coup);
}

static void
do_assassinate(struct pcx_coup *coup,
               int extra_data)
{
        if (coup->clan_characters[PCX_COUP_CLAN_ASSASSINS] !=
            PCX_COUP_CHARACTER_ASSASSIN)
                return;

        struct pcx_coup_player *player = coup->players + coup->current_player;

        if (player->coins < 3)
                return;

        if (extra_data == -1) {
                send_select_target(coup,
                                   PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION,
                                   assassinate_button.data);
                return;
        }

        if (extra_data >= coup->n_players || extra_data == coup->current_player)
                return;

        struct pcx_coup_player *target = coup->players + extra_data;

        if (!is_alive(target))
                return;

        struct challenge_data *data =
                check_challenge(coup,
                                CHALLENGE_FLAG_CHALLENGE |
                                CHALLENGE_FLAG_BLOCK,
                                coup->current_player,
                                do_accepted_assassinate,
                                target, /* user_data */
                                PCX_TEXT_STRING_DOING_ASSASSINATION,
                                player->name,
                                target->name);

        data->challenged_clans = (1 << PCX_COUP_CLAN_ASSASSINS);
        data->blocking_clans = (1 << PCX_COUP_CLAN_INTOUCHABLES);
        data->block_cb = block_assassinate;
        data->target_player = extra_data;
}

#define CARDS_TAKEN_IN_EXCHANGE 2

struct exchange_data {
        int n_cards_chosen;
        int n_cards_available;
        enum pcx_coup_character available_cards[CARDS_TAKEN_IN_EXCHANGE +
                                           PCX_COUP_CARDS_PER_PLAYER];
};

static void
exchange_callback_data(struct pcx_coup *coup,
                       int player_num,
                       const char *command,
                       int extra_data)
{
        struct exchange_data *data = get_stack_data_pointer(coup);

        if (strcmp(command, "keep") ||
            extra_data < 0 || extra_data >= data->n_cards_available)
                return;

        take_action(coup);

        struct pcx_coup_player *player = coup->players + coup->current_player;

        player->cards[data->n_cards_chosen].character =
                data->available_cards[extra_data];
        player->cards[data->n_cards_chosen].dead = false;
        data->n_cards_chosen++;

        memmove(data->available_cards + extra_data,
                data->available_cards + extra_data + 1,
                (data->n_cards_available - extra_data - 1) *
                sizeof data->available_cards[0]);
        data->n_cards_available--;

        if (data->n_cards_chosen >= PCX_COUP_CARDS_PER_PLAYER) {
                for (unsigned i = 0; i < data->n_cards_available; i++)
                        coup->deck[coup->n_cards++] = data->available_cards[i];
                shuffle_deck(coup);
                stack_pop(coup);
                show_cards(coup, coup->current_player);
        }
}

static void
exchange_idle(struct pcx_coup *coup)
{
        struct exchange_data *data = get_stack_data_pointer(coup);
        struct pcx_game_button buttons[CARDS_TAKEN_IN_EXCHANGE +
                                       PCX_COUP_CARDS_PER_PLAYER];

        for (unsigned i = 0; i < data->n_cards_available; i++) {
                enum pcx_coup_character character = data->available_cards[i];
                enum pcx_text_string name =
                        pcx_coup_characters[character].name;
                buttons[i].text = pcx_text_get(coup->language, name);

                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
                pcx_buffer_append_printf(&buf, "keep:%u", i);
                buttons[i].data = (char *) buf.data;
        }

        pcx_buffer_set_length(&coup->buffer, 0);
        append_buffer_string(coup,
                             &coup->buffer,
                             PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP);

        send_buffer_message_with_buttons_to(coup,
                                            coup->current_player,
                                            data->n_cards_available,
                                            buttons);

        for (unsigned i = 0; i < data->n_cards_available; i++)
                pcx_free((char *) buttons[i].data);
}


static void
exchange_destroy(struct pcx_coup *coup)
{
        pcx_free(get_stack_data_pointer(coup));
}

static void
do_accepted_exchange(struct pcx_coup *coup,
                     void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;

        coup_note(coup,
                  PCX_TEXT_STRING_REALLY_DOING_EXCHANGE,
                  player->name);

        struct exchange_data *data = pcx_calloc(sizeof *data);

        for (unsigned i = 0; i < PCX_COUP_CARDS_PER_PLAYER; i++) {
                const struct pcx_coup_card *card = player->cards + i;

                if (card->dead) {
                        player->cards[data->n_cards_chosen++] = *card;
                } else {
                        data->available_cards[data->n_cards_available++] =
                                card->character;
                }
        }

        for (unsigned i = 0; i < CARDS_TAKEN_IN_EXCHANGE; i++) {
                data->available_cards[data->n_cards_available++] =
                        take_card(coup);
        }

        stack_push_pointer(coup,
                           exchange_callback_data,
                           exchange_idle,
                           exchange_destroy,
                           data);

        take_action(coup);
}

static void
do_exchange(struct pcx_coup *coup)
{
        if (coup->clan_characters[PCX_COUP_CLAN_NEGOTIATORS] !=
            PCX_COUP_CHARACTER_AMBASSADOR)
                return;

        struct pcx_coup_player *player = coup->players + coup->current_player;

        struct challenge_data *data =
                check_challenge(coup,
                                CHALLENGE_FLAG_CHALLENGE,
                                coup->current_player,
                                do_accepted_exchange,
                                NULL, /* user_data */
                                PCX_TEXT_STRING_DOING_EXCHANGE,
                                player->name);

        data->challenged_clans = (1 << PCX_COUP_CLAN_NEGOTIATORS);
}

static void
do_accepted_steal(struct pcx_coup *coup,
                  void *user_data)
{
        struct pcx_coup_player *player = coup->players + coup->current_player;
        struct pcx_coup_player *target = user_data;

        coup_note(coup,
                  PCX_TEXT_STRING_REALLY_DOING_STEAL,
                  player->name,
                  target->name);

        int amount = MIN(2, target->coins);
        player->coins += amount;
        target->coins -= amount;

        take_action(coup);
}

static void
do_steal(struct pcx_coup *coup,
         int extra_data)
{
        if (coup->clan_characters[PCX_COUP_CLAN_THIEVES] !=
            PCX_COUP_CHARACTER_CAPTAIN)
                return;

        struct pcx_coup_player *player = coup->players + coup->current_player;

        if (extra_data == -1) {
                send_select_target(coup,
                                   PCX_TEXT_STRING_SELECT_TARGET_STEAL,
                                   steal_button.data);
                return;
        }

        if (extra_data >= coup->n_players || extra_data == coup->current_player)
                return;

        struct pcx_coup_player *target = coup->players + extra_data;

        if (!is_alive(target))
                return;

        struct challenge_data *data =
                check_challenge(coup,
                                CHALLENGE_FLAG_CHALLENGE |
                                CHALLENGE_FLAG_BLOCK,
                                coup->current_player,
                                do_accepted_steal,
                                target, /* user_data */
                                PCX_TEXT_STRING_DOING_STEAL,
                                player->name,
                                target->name);

        data->challenged_clans = (1 << PCX_COUP_CLAN_THIEVES);
        data->blocking_clans = ((1 << PCX_COUP_CLAN_NEGOTIATORS) |
                                (1 << PCX_COUP_CLAN_THIEVES));
        data->target_player = extra_data;
}

static bool
is_button(const char *data,
          const struct coup_text_button *button)
{
        return !strcmp(data, button->data);
}

static void
choose_action(struct pcx_coup *coup,
              int player_num,
              const char *data,
              int extra_data)
{
        if (player_num != coup->current_player)
                return;

        if (is_button(data, &coup_button)) {
                do_coup(coup, extra_data);
        } else if (coup->players[player_num].coins < 10) {
                if (is_button(data, &income_button))
                        do_income(coup);
                else if (is_button(data, &foreign_aid_button))
                        do_foreign_add(coup);
                else if (is_button(data, &tax_button))
                        do_tax(coup);
                else if (is_button(data, &assassinate_button))
                        do_assassinate(coup, extra_data);
                else if (is_button(data, &exchange_button))
                        do_exchange(coup);
                else if (is_button(data, &steal_button))
                        do_steal(coup, extra_data);
        }
}

static void
choose_action_idle(struct pcx_coup *coup)
{
        /* If the coup becomes idle when the top of the stack is to
         * choose an action then the turn is over.
         */

        int next_player = coup->current_player;

        while (true) {
                next_player = (next_player + 1) % coup->n_players;

                if (next_player == coup->current_player ||
                    is_alive(coup->players + next_player))
                        break;
        }

        coup->current_player = next_player;

        show_stats(coup);
}

static void
create_deck(struct pcx_coup *coup,
            const struct pcx_coup_debug_overrides *overrides)
{
        coup->n_cards = PCX_COUP_TOTAL_CARDS;

        for (unsigned ch = 0; ch < PCX_COUP_CLAN_COUNT; ch++) {
                for (unsigned c = 0; c < PCX_COUP_CARDS_PER_CLAN; c++)
                        coup->deck[ch * PCX_COUP_CARDS_PER_CLAN + c] =
                                coup->clan_characters[ch];
        }

        shuffle_deck(coup);

        if (overrides == NULL)
                return;

        int dst = coup->n_cards - 1;

        for (int i = 0; i < overrides->n_cards; i++) {
                enum pcx_coup_character card = overrides->cards[i];
                int copy_pos;

                for (copy_pos = 0; copy_pos <= dst; copy_pos++) {
                        if (coup->deck[copy_pos] == card)
                                goto found_card;
                }

                assert(!"couldn’t find override card in deck");

        found_card:
                coup->deck[copy_pos] = coup->deck[dst];
                coup->deck[dst] = card;
                dst--;
        }
}

struct pcx_coup *
pcx_coup_new(const struct pcx_game_callbacks *callbacks,
             void *user_data,
             enum pcx_text_language language,
             int n_players,
             const char * const *names,
             const struct pcx_coup_debug_overrides *overrides)
{
        assert(n_players > 0 && n_players <= PCX_COUP_MAX_PLAYERS);

        struct pcx_coup *coup = pcx_calloc(sizeof *coup);

        coup->clan_characters[PCX_COUP_CLAN_TAX_COLLECTORS] =
                PCX_COUP_CHARACTER_DUKE;
        coup->clan_characters[PCX_COUP_CLAN_THIEVES] =
                PCX_COUP_CHARACTER_CAPTAIN;
        coup->clan_characters[PCX_COUP_CLAN_INTOUCHABLES] =
                PCX_COUP_CHARACTER_CONTESSA;
        coup->clan_characters[PCX_COUP_CLAN_ASSASSINS] =
                PCX_COUP_CHARACTER_ASSASSIN;
        coup->clan_characters[PCX_COUP_CLAN_NEGOTIATORS] =
                PCX_COUP_CHARACTER_AMBASSADOR;

        coup->language = language;
        coup->callbacks = *callbacks;
        coup->user_data = user_data;

        if (overrides && overrides->rand_func)
                coup->rand_func = overrides->rand_func;
        else
                coup->rand_func = rand;

        create_deck(coup, overrides);

        coup->n_players = n_players;
        if (overrides) {
                assert(overrides->start_player >= 0 &&
                       overrides->start_player < n_players);
                coup->current_player = overrides->start_player;
        } else {
                coup->current_player = coup->rand_func() % n_players;
        }

        for (unsigned i = 0; i < n_players; i++) {
                coup->players[i].coins = PCX_COUP_START_COINS;
                for (unsigned j = 0; j < PCX_COUP_CARDS_PER_PLAYER; j++) {
                        struct pcx_coup_card *card = coup->players[i].cards + j;
                        card->dead = false;
                        card->character = take_card(coup);
                }

                coup->players[i].name = pcx_strdup(names[i]);
        }

        if (coup->n_players == 2)
                coup->players[coup->current_player].coins--;

        stack_push(coup,
                   choose_action,
                   choose_action_idle);

        for (unsigned i = 0; i < n_players; i++)
                show_cards(coup, i);

        show_stats(coup);

        return coup;
}

static void *
create_game_cb(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        return pcx_coup_new(callbacks,
                            user_data,
                            language,
                            n_players,
                            names,
                            NULL /* overrides */);
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_coup_help[language]);
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_coup *coup = user_data;

        assert(player_num >= 0 && player_num < coup->n_players);

        if (coup->stack_pos <= 0)
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
        struct pcx_coup_stack_entry *entry = get_stack_top(coup);

        entry->func(coup, player_num, main_data, extra_data);

        pcx_free(main_data);

        do_idle(coup);
}

static void
free_game_cb(void *data)
{
        free_game(data);
}

const struct pcx_game
pcx_coup_game = {
        .name = "coup",
        .name_string = PCX_TEXT_STRING_NAME_COUP,
        .start_command = PCX_TEXT_STRING_COUP_START_COMMAND,
        .min_players = PCX_COUP_MIN_PLAYERS,
        .max_players = PCX_COUP_MAX_PLAYERS,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
