#!/usr/bin/env python3

# Puxcobot - A robot to play Coup in Esperanto (Puĉo)
# Copyright (C) 2024  Neil Roberts
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import re
import sys


game_re = re.compile(r'^ *&pcx_([a-z0-9_]+)_game *,? *$')

with open('pcx-game.c', 'r', encoding='utf-8') as f:
    games = [md.group(1) for md in map(lambda line: game_re.match(line), f)
             if md is not None]

language_re = re.compile(r'^ *\[PCX_TEXT_LANGUAGE_([A-Za-z_0-9]+)] = pcx_text')

with open('pcx-text.c', 'r', encoding='utf-8') as f:
    languages = [md.group(1).lower()
                 for md in map(lambda line: language_re.match(line), f)
                 if md is not None]

available_translations = dict(map(lambda language: (language, set()),
                                  languages))

translation_re = re.compile(r'^ *\[(PCX_TEXT_STRING_[A-Za-z0-9_]+)\] *=')

for language in languages:
    with open(f"pcx-text-{language.replace('_', '-')}.c",
              "r",
              encoding="utf-8") as f:
        for line in f:
            md = translation_re.match(line)

            if md is not None:
                available_translations[language].add(md.group(1))

string_usage_re = re.compile(r'\bPCX_TEXT_STRING_[A-Za-z0-9_]+')

def check_usages(languages, filename):
    usages = set()

    with open(filename, "r", encoding="utf-8") as f:
        for line in f:
            for md in string_usage_re.finditer(line):
                usages.add(md.group(0))

    for usage in usages:
        for lang in languages:
            if usage not in available_translations[lang]:
                print(f"missing translation for {usage} in {lang}")

for game in games:
    def game_is_translated(lang):
        start_key = f"PCX_TEXT_STRING_{game.upper()}_START_COMMAND"
        return start_key in available_translations[lang]

    translations = list(filter(game_is_translated, languages))

    check_usages(translations, f"pcx-{game}.c")

extra_files = ["pcx-bot.c", "pcx-conversation.c"]

for filename in extra_files:
    check_usages(languages, filename)
