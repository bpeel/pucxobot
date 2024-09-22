/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2022  Neil Roberts
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

#include <time.h>
#include <stdint.h>

/* This is a hack for the unit tests to simulate time advancing */

static int
time_offset = 0;

static int
override_clock_gettime(clockid_t clockid, struct timespec *tp)
{
        if (clock_gettime(clockid, tp) == -1)
                return -1;

        tp->tv_sec += time_offset;

        return 0;
}

#define clock_gettime override_clock_gettime

#include "pcx-main-context.c"

#undef clock_gettime

#include "test-time-hack.h"

void
test_time_hack_add_time(int seconds)
{
        time_offset += seconds;

        struct pcx_main_context *mc = pcx_main_context_get_default();

        mc->monotonic_time_valid = false;
}
