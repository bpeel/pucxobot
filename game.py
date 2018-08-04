#!/usr/bin/python3

# Puxcobot - A robot to play Coup in Esperanto (Puĉo)
# Copyright (C) 2018  Neil Roberts
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

import random
import enum

MAX_PLAYERS = 6

class Result:
    def __init__(self, success, message):
        self.success = success
        self.message = message

class Character(enum.Enum):
    DUKE = "Duko"
    ASSASSIN = "Murdisto"
    CONTESSA = "Grafino"
    CAPTAIN = "Kapitano"
    AMBASSADOR = "Ambasadoro"

class Card:
    def __init__(self, character):
        self.character = character
        self.visible = False

class Player:
    def __init__(self, id, name):
        self.id = id
        self.name = name
        self.coins = 2
        self.cards = []

    def is_alive(self):
        for card in self.cards:
            if not card.visible:
                return True

        return False

class Game:
    def __init__(self):
        self._deck = list(Character) * 3
        random.shuffle(self._deck)

        self.players = []
        self.is_running = False
        self.current_player = 0

    def has_player(self, id):
        for player in self.players:
            if player.id == id:
                return True
        return False

    def add_player(self, id, name):
        player = Player(id, name)

        for i in range(2):
            player.cards.append(Card(self._deck.pop()))

        self.players.append(player)

    def start(self):
        self.is_running = True
        self.current_player = random.randrange(len(self.players))

        if len(self.players) == 2:
            self.players[self.current_player].coins -= 1

    def next_player(self):
        next_player = self.current_player

        while True:
            next_player = (next_player + 1) % len(self.players)
            if (next_player == self.current_player or
                self.players[next_player].is_alive()):
                self.current_player = next_player
                break

    def is_finished(self):
        num_players = 0
        for player in self.players:
            if player.is_alive():
                num_players += 1
        return num_players <= 1
