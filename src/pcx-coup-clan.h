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

#ifndef PCX_COUP_CLAN_H
#define PCX_COUP_CLAN_H

enum pcx_coup_clan {
        PCX_COUP_CLAN_TAX_COLLECTORS,
        PCX_COUP_CLAN_THIEVES,
        PCX_COUP_CLAN_INTOUCHABLES,
        PCX_COUP_CLAN_ASSASSINS,
        PCX_COUP_CLAN_NEGOTIATORS,
};

#define PCX_COUP_CLAN_COUNT 5

#endif /* PCX_COUP_CLAN_H */
