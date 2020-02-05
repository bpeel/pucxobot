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

#include "pcx-coup-character.h"

const struct pcx_coup_character_data
pcx_coup_characters[] = {
        [PCX_COUP_CHARACTER_DUKE] = {
                PCX_TEXT_STRING_CHARACTER_NAME_DUKE,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE,
                PCX_COUP_CLAN_TAX_COLLECTORS,
        },
        [PCX_COUP_CHARACTER_ASSASSIN] = {
                PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN,
                PCX_COUP_CLAN_ASSASSINS,
        },
        [PCX_COUP_CHARACTER_CONTESSA] = {
                PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA,
                PCX_COUP_CLAN_INTOUCHABLES,
        },
        [PCX_COUP_CHARACTER_CAPTAIN] = {
                PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN,
                PCX_COUP_CLAN_THIEVES,
        },
        [PCX_COUP_CHARACTER_AMBASSADOR] = {
                PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR,
                PCX_COUP_CLAN_NEGOTIATORS,
        },
        [PCX_COUP_CHARACTER_INSPECTOR] = {
                PCX_TEXT_STRING_CHARACTER_NAME_INSPECTOR,
                PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_INSPECTOR,
                PCX_COUP_CLAN_NEGOTIATORS,
        },
};
