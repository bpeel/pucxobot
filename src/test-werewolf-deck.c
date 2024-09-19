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

#include <stdlib.h>
#include <stdbool.h>

static const int *
random_number_queue;

static int random_number_queue_length;

static int
fake_random_number_generator(void)
{
        if (random_number_queue_length > 0) {
                random_number_queue_length--;
                return *(random_number_queue++);
        }

        return 0;
}

#define rand fake_random_number_generator

#include "pcx-werewolf.c"

static bool
check_deck(const enum pcx_werewolf_role *expected,
           const enum pcx_werewolf_role *actual,
           int n_cards)
{
        if (memcmp(expected, actual, n_cards * sizeof *expected)) {
                fprintf(stderr,
                        "deck does not match.\n"
                        "Expected:");
                for (int i = 0; i < n_cards; i++)
                        fprintf(stderr, " %i", expected[i]);
                fprintf(stderr, "\nActual:  ");
                for (int i = 0; i < n_cards; i++)
                        fprintf(stderr, " %i", actual[i]);

                fputc('\n', stdout);

                return false;
        } else {
                return true;
        }
}

static bool
test_can_add_mason(void)
{
        enum pcx_werewolf_role cards[6];

        /* Skip picking the mason once and then pick it as the last
         * two cards.
         */
        static const int number_queue[] = { 1, 0 };
        random_number_queue = number_queue;
        random_number_queue_length = 2;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_MASON,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_cant_add_mason(void)
{
        enum pcx_werewolf_role cards[6];

        /* Skip picking the mason twice and then pick it when there’s
         * only one space left.
         */
        static const int number_queue[] = { 1, 1, 0 };
        random_number_queue = number_queue;
        random_number_queue_length = 3;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_TROUBLEMAKER,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_loads_of_cards(void)
{
        enum pcx_werewolf_role cards[20];

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_MASON,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_TROUBLEMAKER,
                PCX_WEREWOLF_ROLE_DRUNK,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
                PCX_WEREWOLF_ROLE_HUNTER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_VILLAGER,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_insomniac_robber_already_there(void)
{
        enum pcx_werewolf_role cards[6];

        /* Add the seer, the robber and then the insomniac */
        static const int number_queue[] = { 1, 1, 3 };
        random_number_queue = number_queue;
        random_number_queue_length = 3;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_insomniac_troublemaker_already_there(void)
{
        enum pcx_werewolf_role cards[6];

        /* Add the seer, the troublemaker and then the insomniac */
        static const int number_queue[] = { 1, 2, 3 };
        random_number_queue = number_queue;
        random_number_queue_length = 3;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_TROUBLEMAKER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_insomniac_force_robber(void)
{
        enum pcx_werewolf_role cards[6];

        /* Add the seer and then the insomniac */
        static const int number_queue[] = { 1, 4 };
        random_number_queue = number_queue;
        random_number_queue_length = 2;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_ROBBER,
                PCX_WEREWOLF_ROLE_INSOMNIAC,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

static bool
test_cant_add_insomniac(void)
{
        enum pcx_werewolf_role cards[6];

        /* Add the seer, the drunk and then try to add the insomniac.
         * This won’t work and then it will end up adding the robber
         * on it’s own instead.
         */
        static const int number_queue[] = { 1, 3, 3, 0 };
        random_number_queue = number_queue;
        random_number_queue_length = 4;

        make_intermediate_deck(cards, PCX_N_ELEMENTS(cards));

        enum pcx_werewolf_role expected_cards[] = {
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_WEREWOLF,
                PCX_WEREWOLF_ROLE_VILLAGER,
                PCX_WEREWOLF_ROLE_SEER,
                PCX_WEREWOLF_ROLE_DRUNK,
                PCX_WEREWOLF_ROLE_ROBBER,
        };

        return check_deck(expected_cards, cards, PCX_N_ELEMENTS(cards));
}

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        if (!test_can_add_mason())
                ret = EXIT_FAILURE;

        if (!test_cant_add_mason())
                ret = EXIT_FAILURE;

        if (!test_loads_of_cards())
                ret = EXIT_FAILURE;

        if (!test_insomniac_robber_already_there())
                ret = EXIT_FAILURE;

        if (!test_insomniac_troublemaker_already_there())
                ret = EXIT_FAILURE;

        if (!test_insomniac_force_robber())
                ret = EXIT_FAILURE;

        if (!test_cant_add_insomniac())
                ret = EXIT_FAILURE;

        return ret;
}
