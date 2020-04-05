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

#include "pcx-zombie.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-main-context.h"
#include "pcx-zombie-help.h"
#include "pcx-html.h"

#define PCX_ZOMBIE_MIN_PLAYERS 1
#define PCX_ZOMBIE_MAX_PLAYERS PCX_GAME_MAX_PLAYERS

#define PCX_ZOMBIE_DICE_PER_THROW 3
#define PCX_ZOMBIE_SHOTGUNS_TO_KILL 3
#define PCX_ZOMBIE_POINTS_TO_END_GAME 13

#define PCX_ZOMBIE_THROW_BUTTON_DATA "throw"
#define PCX_ZOMBIE_STOP_BUTTON_DATA "stop"

enum pcx_zombie_face {
        PCX_ZOMBIE_FACE_FEET,
        PCX_ZOMBIE_FACE_BRAIN,
        PCX_ZOMBIE_FACE_SHOTGUN,
};

#define PCX_ZOMBIE_N_FACES 3

enum pcx_zombie_die {
        PCX_ZOMBIE_DIE_GREEN,
        PCX_ZOMBIE_DIE_YELLOW,
        PCX_ZOMBIE_DIE_RED,
};

#define PCX_ZOMBIE_N_DICE 3

struct pcx_zombie_die_set {
        int dice_count[PCX_ZOMBIE_N_DICE];
};

struct pcx_zombie_player {
        char *name;
        int score;
};

struct pcx_zombie_die_info {
        const char *symbol;
        int start_amount;
        int faces[PCX_ZOMBIE_N_FACES];
};

struct pcx_zombie_die_and_face {
        enum pcx_zombie_die die;
        enum pcx_zombie_face face;
};

struct pcx_zombie {
        struct pcx_zombie_player players[PCX_ZOMBIE_MAX_PLAYERS];
        int n_players;
        int current_player;
        struct pcx_game_callbacks callbacks;
        void *user_data;
        struct pcx_main_context_source *game_over_source;
        enum pcx_text_language language;
        int (* rand_func)(void);

        /* Normally this is -1. When the final round is initiated,
         * this gets set to the player that will take the last turn.
         */
        int last_player;

        /* The dice results are presented one at a time with a pause
         * in between to add a bit of drama.
         */
        struct pcx_main_context_source *drama_source;
        int n_dice_shown;
        int n_dice_thrown;
        struct pcx_zombie_die_and_face dice_thrown[PCX_ZOMBIE_DICE_PER_THROW];

        /* State of current turn */
        struct pcx_zombie_die_set box;
        struct pcx_zombie_die_set brains_thrown;
        struct pcx_zombie_die_set feet_thrown;
        int n_brains;
        int n_shotguns;
};

static const struct pcx_zombie_die_info
die_info[PCX_ZOMBIE_N_DICE] = {
        [PCX_ZOMBIE_DIE_GREEN] = {
                .symbol = "🍏",
                .start_amount = 6,
                .faces = {
                        [PCX_ZOMBIE_FACE_FEET] = 2,
                        [PCX_ZOMBIE_FACE_BRAIN] = 3,
                        [PCX_ZOMBIE_FACE_SHOTGUN] = 1,
                },
        },
        [PCX_ZOMBIE_DIE_YELLOW] = {
                .symbol = "💛",
                .start_amount = 4,
                .faces = {
                        [PCX_ZOMBIE_FACE_FEET] = 2,
                        [PCX_ZOMBIE_FACE_BRAIN] = 2,
                        [PCX_ZOMBIE_FACE_SHOTGUN] = 2,
                },
        },
        [PCX_ZOMBIE_DIE_RED] = {
                .symbol = "🧨",
                .start_amount = 3,
                .faces = {
                        [PCX_ZOMBIE_FACE_FEET] = 2,
                        [PCX_ZOMBIE_FACE_BRAIN] = 1,
                        [PCX_ZOMBIE_FACE_SHOTGUN] = 3,
                },
        },
};

static const char *
face_symbols[PCX_ZOMBIE_N_FACES] = {
        [PCX_ZOMBIE_FACE_FEET] = "🐾",
        [PCX_ZOMBIE_FACE_BRAIN] = "🧠",
        [PCX_ZOMBIE_FACE_SHOTGUN] = "💥",
};

static void
start_drama_timeout(struct pcx_zombie *zombie);

static void
escape_string(struct pcx_zombie *zombie,
              struct pcx_buffer *buf,
              enum pcx_text_string string)
{
        const char *value = pcx_text_get(zombie->language, string);
        pcx_html_escape(buf, value);
}

static void
append_special_vformat(struct pcx_zombie *zombie,
                       struct pcx_buffer *buf,
                       enum pcx_text_string format_string,
                       va_list ap)
{
        const char *format = pcx_text_get(zombie->language, format_string);
        const char *last = format;

        while (true) {
                const char *percent = strchr(last, '%');

                if (percent == NULL)
                        break;

                pcx_buffer_append(buf, last, percent - last);

                switch (percent[1]) {
                case 's': {
                        enum pcx_text_string e =
                                va_arg(ap, enum pcx_text_string);
                        const char *s = pcx_text_get(zombie->language, e);
                        pcx_buffer_append_string(buf, s);
                        break;
                }

                case 'p': {
                        const struct pcx_zombie_player *p =
                                va_arg(ap, const struct pcx_zombie_player *);
                        pcx_html_escape(buf, p->name);
                        break;
                }

                case 'i': {
                        int i = va_arg(ap, int);
                        pcx_buffer_append_printf(buf, "%i", i);
                        break;
                }

                default:
                        pcx_fatal("Unknown format character");
                }

                last = percent + 2;
        }

        pcx_buffer_append_string(buf, last);
}

static void
append_special_format(struct pcx_zombie *zombie,
                      struct pcx_buffer *buf,
                      enum pcx_text_string format_string,
                      ...)
{
        va_list ap;

        va_start(ap, format_string);
        append_special_vformat(zombie, buf, format_string, ap);
        va_end(ap);
}

static void
game_note(struct pcx_zombie *zombie,
          enum pcx_text_string format,
          ...)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        va_list ap;

        va_start(ap, format);
        append_special_vformat(zombie, &buf, format, ap);
        va_end(ap);

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       0, /* n_buttons */
                                       NULL, /* buttons */
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);
}

static int
die_set_count(const struct pcx_zombie_die_set *set)
{
        int sum = 0;

        for (unsigned i = 0; i < PCX_ZOMBIE_N_DICE; i++)
                sum += set->dice_count[i];

        return sum;
}

static int
get_random(struct pcx_zombie *zombie,
           int n_values)
{
        return zombie->rand_func() % n_values;
}

static enum pcx_zombie_die
get_die_number(const struct pcx_zombie_die_set *set,
               int num)
{
        int sum = 0;

        for (unsigned i = 0; i < PCX_ZOMBIE_N_DICE; i++) {
                sum += set->dice_count[i];

                if (sum > num)
                        return i;
        }

        assert(!"dice index out of range");
}

static enum pcx_zombie_face
throw_die(struct pcx_zombie *zombie,
          enum pcx_zombie_die die)
{
        const struct pcx_zombie_die_info *info = die_info + die;
        int face = get_random(zombie, 6);
        int sum = 0;

        for (unsigned i = 0; i < PCX_ZOMBIE_N_FACES; i++) {
                sum += info->faces[i];

                if (sum > face)
                        return i;
        }

        assert(!"face index out of range");
}

static void
get_dice(struct pcx_zombie *zombie,
         struct pcx_zombie_die_set *set)
{
        int dice_have = die_set_count(set);

        assert(dice_have <= PCX_ZOMBIE_DICE_PER_THROW);

        int dice_needed = PCX_ZOMBIE_DICE_PER_THROW - dice_have;
        int dice_available = die_set_count(&zombie->box);

        if (dice_available < dice_needed) {
                /* Put all the brains back in the box */
                for (unsigned i = 0; i < PCX_ZOMBIE_N_DICE; i++) {
                        zombie->box.dice_count[i] +=
                                zombie->brains_thrown.dice_count[i];
                        zombie->brains_thrown.dice_count[i] = 0;
                }

                dice_available = die_set_count(&zombie->box);

                dice_needed = MIN(dice_available, dice_needed);
        }

        for (int i = 0; i < dice_needed; i++) {
                enum pcx_zombie_die die =
                        get_die_number(&zombie->box,
                                       get_random(zombie, dice_available));
                dice_available--;
                zombie->box.dice_count[die]--;
                set->dice_count[die]++;
        }
}

static void
start_turn(struct pcx_zombie *zombie)
{
        memset(&zombie->brains_thrown, 0, sizeof zombie->brains_thrown);
        memset(&zombie->feet_thrown, 0, sizeof zombie->feet_thrown);
        zombie->n_brains = 0;
        zombie->n_shotguns = 0;

        for (unsigned i = 0; i < PCX_ZOMBIE_N_DICE; i++)
                zombie->box.dice_count[i] = die_info[i].start_amount;

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        for (int i = 0; i < zombie->n_players; i++) {
                if (i == zombie->current_player)
                        pcx_buffer_append_string(&buf, "👉 ");

                pcx_html_escape(&buf, zombie->players[i].name);
                pcx_buffer_append_printf(&buf,
                                         ": %i\n",
                                         zombie->players[i].score);
        }

        pcx_buffer_append_c(&buf, '\n');
        append_special_format(zombie,
                              &buf,
                              PCX_TEXT_STRING_THROW_FIRST_DICE,
                              zombie->players + zombie->current_player);

        struct pcx_game_button throw_button = {
                .text = pcx_text_get(zombie->language,
                                     PCX_TEXT_STRING_THROW),
                .data = PCX_ZOMBIE_THROW_BUTTON_DATA,
        };

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       1, /* n_buttons */
                                       &throw_button, /* buttons */
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);
}

static void
free_game(struct pcx_zombie *zombie)
{
        for (int i = 0; i < zombie->n_players; i++)
                pcx_free(zombie->players[i].name);

        if (zombie->game_over_source)
                pcx_main_context_remove_source(zombie->game_over_source);

        if (zombie->drama_source)
                pcx_main_context_remove_source(zombie->drama_source);

        pcx_free(zombie);
}

static void
game_over_cb(struct pcx_main_context_source *source,
             void *user_data)
{
        struct pcx_zombie *zombie = user_data;
        zombie->game_over_source = NULL;
        zombie->callbacks.game_over(zombie->user_data);
}

struct pcx_zombie *
pcx_zombie_new(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names,
               const struct pcx_zombie_debug_overrides *overrides)
{
        assert(n_players > 0 && n_players <= PCX_ZOMBIE_MAX_PLAYERS);

        struct pcx_zombie *zombie = pcx_calloc(sizeof *zombie);

        zombie->language = language;
        zombie->callbacks = *callbacks;
        zombie->user_data = user_data;

        if (overrides && overrides->rand_func)
                zombie->rand_func = overrides->rand_func;
        else
                zombie->rand_func = rand;

        zombie->n_players = n_players;
        zombie->current_player = zombie->rand_func() % n_players;
        zombie->last_player = -1;

        for (unsigned i = 0; i < n_players; i++)
                zombie->players[i].name = pcx_strdup(names[i]);

        start_turn(zombie);

        return zombie;
}

static void *
create_game_cb(const struct pcx_game_callbacks *callbacks,
               void *user_data,
               enum pcx_text_language language,
               int n_players,
               const char * const *names)
{
        return pcx_zombie_new(callbacks,
                              user_data,
                              language,
                              n_players,
                              names,
                              NULL /* overrides */);
}

static char *
get_help_cb(enum pcx_text_language language)
{
        return pcx_strdup(pcx_zombie_help[language]);
}

static void
end_game(struct pcx_zombie *zombie)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf,
                                 pcx_text_get(zombie->language,
                                              PCX_TEXT_STRING_FINAL_SCORES));
        pcx_buffer_append_string(&buf, "\n\n");

        for (int i = 0; i < zombie->n_players; i++) {
                pcx_html_escape(&buf, zombie->players[i].name);
                pcx_buffer_append_printf(&buf,
                                         ": %i\n",
                                         zombie->players[i].score);
        }

        int winner = 0;

        for (int i = 1; i < zombie->n_players; i++) {
                if (zombie->players[i].score > zombie->players[winner].score)
                        winner = i;
        }

        pcx_buffer_append_string(&buf, "\n");
        append_special_format(zombie,
                              &buf,
                              PCX_TEXT_STRING_WINS,
                              zombie->players + winner);

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       0, /* n_buttons */
                                       NULL, /* buttons */
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);

        if (zombie->game_over_source == NULL) {
                zombie->game_over_source =
                        pcx_main_context_add_timeout(NULL,
                                                     0, /* ms */
                                                     game_over_cb,
                                                     zombie);
        }
}

static void
end_turn(struct pcx_zombie *zombie)
{
        int current_score = zombie->players[zombie->current_player].score;

        if (zombie->last_player == -1 &&
            current_score >= PCX_ZOMBIE_POINTS_TO_END_GAME) {
                game_note(zombie,
                          PCX_TEXT_STRING_START_LAST_ROUND,
                          zombie->players + zombie->current_player,
                          current_score);
                zombie->last_player = ((zombie->current_player +
                                        zombie->n_players - 1) %
                                       zombie->n_players);
        }

        if (zombie->current_player == zombie->last_player) {
                end_game(zombie);
                return;
        }

        zombie->current_player =
                (zombie->current_player + 1) % zombie->n_players;
        start_turn(zombie);
}

static void
add_score_so_far(struct pcx_zombie *zombie,
                 struct pcx_buffer *buf)
{
        pcx_buffer_append_string(buf,
                                 pcx_text_get(zombie->language,
                                              PCX_TEXT_STRING_SCORE_SO_FAR));
        pcx_buffer_append_printf(buf,
                                 " %s %i %s %i\n"
                                 "\n",
                                 face_symbols[PCX_ZOMBIE_FACE_BRAIN],
                                 zombie->n_brains,
                                 face_symbols[PCX_ZOMBIE_FACE_SHOTGUN],
                                 zombie->n_shotguns);
}

static void
show_die(struct pcx_zombie *zombie,
         const struct pcx_zombie_die_and_face *daf)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_string(&buf, die_info[daf->die].symbol);
        pcx_buffer_append_string(&buf, face_symbols[daf->face]);

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       0, /* n_buttons */
                                       NULL, /* buttons */
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);
}

static void
drama_cb(struct pcx_main_context_source *source,
         void *user_data)
{
        struct pcx_zombie *zombie = user_data;
        zombie->drama_source = NULL;

        if (zombie->n_dice_shown < zombie->n_dice_thrown) {
                show_die(zombie, zombie->dice_thrown + zombie->n_dice_shown);
                zombie->n_dice_shown++;
                start_drama_timeout(zombie);
                return;
        }

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        add_score_so_far(zombie, &buf);

        enum pcx_text_string note;
        int n_buttons;

        if (zombie->n_shotguns >= PCX_ZOMBIE_SHOTGUNS_TO_KILL) {
                note = PCX_TEXT_STRING_YOU_ARE_DEAD;
                n_buttons = 0;
        } else {
                note = PCX_TEXT_STRING_THROW_OR_STOP;
                n_buttons = 2;
        }

        pcx_buffer_append_string(&buf, pcx_text_get(zombie->language, note));

        struct pcx_game_button buttons[] = {
                {
                        .text = pcx_text_get(zombie->language,
                                             PCX_TEXT_STRING_THROW),
                        .data = PCX_ZOMBIE_THROW_BUTTON_DATA,
                },
                {
                        .text = pcx_text_get(zombie->language,
                                             PCX_TEXT_STRING_STOP),
                        .data = PCX_ZOMBIE_STOP_BUTTON_DATA,
                },
        };

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       n_buttons,
                                       buttons,
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);

        if (zombie->n_shotguns >= PCX_ZOMBIE_SHOTGUNS_TO_KILL)
                end_turn(zombie);
}

static void
start_drama_timeout(struct pcx_zombie *zombie)
{
        if (zombie->drama_source)
                return;

        zombie->drama_source = pcx_main_context_add_timeout(NULL,
                                                            1000,
                                                            drama_cb,
                                                            zombie);
}

static void
score_dice(struct pcx_zombie *zombie)
{
        for (int i = 0; i < zombie->n_dice_thrown; i++) {
                const struct pcx_zombie_die_and_face *daf =
                        zombie->dice_thrown + i;

                switch (daf->face) {
                case PCX_ZOMBIE_FACE_FEET:
                        break;
                case PCX_ZOMBIE_FACE_BRAIN:
                        zombie->feet_thrown.dice_count[daf->die]--;
                        zombie->brains_thrown.dice_count[daf->die]++;
                        zombie->n_brains++;
                        break;
                case PCX_ZOMBIE_FACE_SHOTGUN:
                        zombie->feet_thrown.dice_count[daf->die]--;
                        zombie->n_shotguns++;
                        break;
                }
        }
}

static void
do_throw(struct pcx_zombie *zombie)
{
        struct pcx_zombie_die_set *die_set = &zombie->feet_thrown;

        get_dice(zombie, die_set);

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        escape_string(zombie, &buf, PCX_TEXT_STRING_YOUR_DICE_ARE);
        pcx_buffer_append_string(&buf, " ");

        int n_dice_thrown = 0;

        for (unsigned i = 0; i < PCX_ZOMBIE_N_DICE; i++) {
                for (int j = 0; j < die_set->dice_count[i]; j++) {
                        pcx_buffer_append_string(&buf,
                                                 die_info[i].symbol);

                        assert(n_dice_thrown < PCX_ZOMBIE_DICE_PER_THROW);
                        zombie->dice_thrown[n_dice_thrown].die = i;
                        zombie->dice_thrown[n_dice_thrown].face =
                                throw_die(zombie, i);
                        n_dice_thrown++;
                }
        }

        zombie->n_dice_thrown = n_dice_thrown;
        zombie->n_dice_shown = 0;

        score_dice(zombie);

        pcx_buffer_append_string(&buf, "\n\n");
        pcx_buffer_append_string(&buf,
                                 pcx_text_get(zombie->language,
                                              PCX_TEXT_STRING_THROWING_DICE));

        zombie->callbacks.send_message(PCX_GAME_MESSAGE_FORMAT_HTML,
                                       (const char *) buf.data,
                                       0, /* n_buttons */
                                       NULL, /* buttons */
                                       zombie->user_data);

        pcx_buffer_destroy(&buf);

        start_drama_timeout(zombie);
}

static void
do_stop(struct pcx_zombie *zombie)
{
        struct pcx_zombie_player *player =
                zombie->players + zombie->current_player;

        player->score += zombie->n_brains;

        end_turn(zombie);
}

static void
handle_callback_data_cb(void *user_data,
                        int player_num,
                        const char *callback_data)
{
        struct pcx_zombie *zombie = user_data;

        assert(player_num >= 0 && player_num < zombie->n_players);

        if (player_num != zombie->current_player)
                return;

        if (zombie->drama_source != NULL)
                return;

        if (!strcmp(callback_data, PCX_ZOMBIE_THROW_BUTTON_DATA))
                do_throw(zombie);
        else if (!strcmp(callback_data, PCX_ZOMBIE_STOP_BUTTON_DATA))
                do_stop(zombie);
}

static void
free_game_cb(void *data)
{
        free_game(data);
}

const struct pcx_game
pcx_zombie_game = {
        .name = "zombie",
        .name_string = PCX_TEXT_STRING_NAME_ZOMBIE,
        .start_command = PCX_TEXT_STRING_ZOMBIE_START_COMMAND,
        .min_players = PCX_ZOMBIE_MIN_PLAYERS,
        .max_players = PCX_ZOMBIE_MAX_PLAYERS,
        .create_game_cb = create_game_cb,
        .get_help_cb = get_help_cb,
        .handle_callback_data_cb = handle_callback_data_cb,
        .free_game_cb = free_game_cb
};
