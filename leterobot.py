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

import urllib.request
import json
import io
import sys
import time
import os
import datetime
import http
import configparser
import html
import random

REGULOJ = """\
<b>RESUMO DE LA REGULOJ:</b>"""

MAX_PLAYERS = 4

class BotException(Exception):
    pass

class GetUpdatesException(BotException):
    pass

class HandleMessageException(BotException):
    pass

class ProcessCommandException(BotException):
    pass

WAIT_TIME = 30
INACTIVITY_TIMEOUT = 5 * 60

# Forget cached chat IDs after 24 hours
CHAT_ID_MAP_TIMEOUT = 24 * 60 * 60

class Character:
    def __init__(self, name, symbol, description, value, count, keyword):
        self.name = name
        self.symbol = symbol
        self.description = description
        self.value = value
        self.count = count
        self.keyword = keyword

    def value_symbol(self):
        return "{}\ufe0f\u20e3".format(self.value)

    def long_name(self, n=False):
        if n:
            n = "n"
        else:
            n = ""
        return "{}{} {}{}".format(self.name,
                                  n,
                                  self.symbol,
                                  self.value_symbol())

GUARD = Character("Gardisto",
                  "👮️",
                  "Nomu karton kiu ne estas gardisto, kaj elektu ludanton. "
                  "Se tiu ludanto havas tiun karton, ri perdas la raŭndon.",
                  1,
                  5,
                  "guard")
SPY = Character("Spiono",
                "🔎",
                "Rigardu la manon de alia ludanto.",
                2,
                2,
                "spy")
BARON = Character("Barono",
                  "⚔️",
                  "Sekrete komparu manojn kun alia ludanto. Tiu de vi ambaŭ, "
                  "kiu havas la malplej valoran karton, perdas la raŭndon.",
                  3,
                  2,
                  "baron")
HANDMAID = Character("Servistino",
                     "💅",
                     "Ĝis via sekva vico, ignoru efikojn de kartoj de ĉiuj "
                     "aliaj ludantoj.",
                     4,
                     2,
                     "handmaid")
PRINCE = Character("Princo",
                   "🤴",
                   "Elektu ludanton (povas esti vi) kiu forĵetos sian manon "
                   "kaj prenos novan karton.",
                   5,
                   2,
                   "prince")
KING = Character("Reĝo",
                 "👑",
                 "Elektu alian ludanton kaj interŝanĝu manojn kun ri.",
                 6,
                 1,
                 "king")
COMTESSE = Character("Grafino",
                     "👩‍💼",
                     "Se vi havas ĉi tiun karton kun la reĝo aŭ la princo en "
                     "via mano, vi devas forĵeti ĉi tiun karton.",
                     7,
                     1,
                     "comtesse")
PRINCESS = Character("Princino",
                     "👸",
                     "Se vi forĵetas ĉi tiun karton, vi perdas la raŭndon.",
                     8,
                     1,
                     "princess")

CHARACTERS = [
    GUARD,
    SPY,
    BARON,
    HANDMAID,
    PRINCE,
    KING,
    COMTESSE,
    PRINCESS,
]

assert(sum(character.count for character in CHARACTERS) == 16)
for i, character in enumerate(CHARACTERS):
    assert(character.value == i + 1)

class Player:
    def __init__(self, id, name, chat_id):
        self.id = id
        self.name = name
        self.chat_id = chat_id
        self.discard_pile = []
        self.is_alive = True
        self.is_protected = False

    def get_score(self):
        return (self.card.value,
                sum(card.value for card in self.discard_pile))

class Bot:
    def __init__(self):
        conf_dir = os.path.expanduser("~/.leterobot")

        config = configparser.ConfigParser()
        config.read(os.path.join(conf_dir, "conf.txt"))

        try:
            self._apikey = config["auth"]["apikey"]
        except KeyError:
            print("Missing apikey option in [auth] section of config",
                  file=sys.stderr)
            sys.exit(1)

        try:
            self._game_chat = config["setup"]["game_chat"]
        except KeyError:
            print("Missing game_chat option in [setup] section of config",
                  file=sys.stderr)
            sys.exit(1)
        try:
            self._game_chat = int(self._game_chat)
        except ValueError:
            pass

        try:
            self._announce_channel = config["setup"]["announce_channel"]
        except KeyError:
            self._announce_channel = None
        else:
            try:
                self._announce_channel = int(self._announce_channel)
            except ValueError:
                pass

        try:
            self._botname = config["setup"]["botname"]
        except KeyError:
            print("Missing botname option in [setup] section of config",
                  file=sys.stderr)
            sys.exit(1)

        self._accepting = []

        self._urlbase = "https://api.telegram.org/bot" + self._apikey + "/"
        self._get_updates_url = self._urlbase + "getUpdates"

        # Mapping of user ID to chat ID. Each value is a tuple with a
        # timestamp and the chat ID so that if the entry is too old
        # then it can be abandoned.
        self._chat_id_map = {}

        self._last_update_id = None
        self._reset_game()
        self._activity()

    def _reset_game(self):
        self._in_game = False
        self._current_player = 0
        self._game_running = False
        self._pending_joins = []
        self._players = []
        self._announce_message = None
        self._deck = []
        self._pending_card = None
        self._set_aside_card = None
        # Cards that are visible and set aside during a two-player game
        self._visible_cards = None

    def _activity(self):
        self._last_activity_time = time.monotonic()

    def _get_updates(self):
        if self._in_game is not None:
            timeout = (INACTIVITY_TIMEOUT + self._last_activity_time -
                       time.monotonic())
        else:
            timeout = 300

        if timeout < 0:
            timeout = 0

        args = {
            'allowed_updates': ['message', 'callback_query'],
            'timeout': timeout
        }

        if self._last_update_id is not None:
            args['offset'] = self._last_update_id + 1

        try:
            req = urllib.request.Request(self._get_updates_url,
                                         json.dumps(args).encode('utf-8'))
            req.add_header('Content-Type', 'application/json; charset=utf-8')
            rep = json.load(io.TextIOWrapper(urllib.request.urlopen(req),
                                             'utf-8'))
        except (urllib.error.URLError, http.client.HTTPException) as e:
            raise GetUpdatesException(e)
        except json.JSONDecodeError as e:
            raise GetUpdatesException(e)

        try:
            if rep['ok'] is not True or not isinstance(rep['result'], list):
                raise GetUpdatesException("Unexpected response from getUpdates "
                                          "request")
        except KeyError as e:
            raise GetUpdatesException(e)

        self._last_update_id = None
        for update in rep['result']:
            if 'update_id' in update:
                update_id = update['update_id']
                if isinstance(update_id, int):
                    if (self._last_update_id is None or
                        update_id > self._last_update_id):
                        self._last_update_id = update_id

        return rep['result']

    def _send_request(self, request, args):
        try:
            req = urllib.request.Request(self._urlbase + request,
                                         json.dumps(args).encode('utf-8'))
            req.add_header('Content-Type', 'application/json; charset=utf-8')
            rep = json.load(io.TextIOWrapper(urllib.request.urlopen(req),
                                             'utf-8'))
        except (urllib.error.URLError, http.client.HTTPException) as e:
            raise HandleMessageException(e)
        except json.JSONDecodeError as e:
            raise HandleMessageException(e)

        try:
            if rep['ok'] is not True:
                raise HandleMessageException("Unexpected response from "
                                              "{} request".format(request))
        except KeyError as e:
            raise HandleMessageException(e)

        return rep

    def _send_reply(self, message, note, markup = False):
        args = {
            'chat_id' : message['chat']['id'],
            'text' : note,
            'reply_to_message_id' : message['message_id']
        }

        if markup:
            args['parse_mode'] = 'HTML'

        self._send_request('sendMessage', args)

    def _do_start_game(self):
        self._game_running = True
        self._activity()

        random.shuffle(self._players)

        for character in CHARACTERS:
            for _ in range(character.count):
                self._deck.append(character)
        random.shuffle(self._deck)

        # Discard the top card
        self._set_aside_card = self._deck.pop()

        if len(self._players) == 2:
            self._visible_cards = []
            for _ in range(3):
                self._visible_cards.append(self._deck.pop())

        for player in self._players:
            player.card = self._deck.pop()
            self._show_card(player)

        self._take_turn()

    def _has_player(self, id):
        for player in self._players:
            if player.id == id:
                return True
        return False

    def _start_game(self, message):
        id = message['from']['id']
        self._activity()

        if self._in_game:
            if self._game_running:
                self._send_reply(message, "La ludo jam komenciĝis")
            elif not self._has_player(id):
                self._send_reply(message,
                                 "Unue aliĝu al la ludo por povi "
                                 "komenci ĝin")
            elif len(self._players) < 2:
                self._send_reply(message,
                                 "Necesas almenaŭ 2 ludantoj por ludi")
            else:
                self._do_start_game()
        else:
            self._join(message)

    def _can_join(self, message):
        if not self._in_game:
            return True

        id = message['from']['id']

        if self._game_running:
            self._send_reply(message,
                             "La ludo jam komenciĝis kaj ne plu eblas aliĝi")
            return False
        if self._has_player(id):
            self._send_reply(message, "Vi jam estas en la ludo")
            return False
        if len(self._players) >= MAX_PLAYERS:
            self._send_reply(message, "Jam estas tro da ludantoj")
            return False

        return True

    def _join(self, message):
        self._activity()

        try:
            from_id = message['from']['id']
            timestamp, chat_id = self._chat_id_map[from_id]
        except KeyError:
            pass
        else:
            del self._chat_id_map[from_id]
            if time.monotonic() - timestamp < CHAT_ID_MAP_TIMEOUT:
                self._really_join(message, chat_id)
                return

        if not self._can_join(message):
            return

        self._send_reply(message,
                         "Sendu al mi privatan mesaĝon ĉe @{} por ke mi povu "
                         "sendi al vi viajn kartojn sekrete".format(
                             self._botname))
        self._pending_joins.append(message)

    def _handle_private_message(self, message):
        for i, original_message in enumerate(self._pending_joins):
            if original_message['from']['id'] == message['from']['id']:
                del self._pending_joins[i]
                self._really_join(original_message, message['chat']['id'])
                break

    def _announce_game(self):
        if self._announce_channel is None:
            return

        args = {
            'chat_id': self._announce_channel,
            'text': ("Nova ludo atendas ludantojn!\n\n"
                     "Aktualaj ludantoj: {}".format(
                         ", ".join(x.name for x in self._players)))
        }

        try:
            if self._announce_message is not None:
                args['message_id'] = self._announce_message
                rep = self._send_request('editMessageText', args)
            else:
                rep = self._send_request('sendMessage', args)
        except BotException as e:
            print("{}".format(e), file=sys.stderr)

        try:
            self._announce_message = rep['result']['message_id']
        except KeyError:
            pass

    def _really_join(self, message, chat_id):
        self._activity()

        id = message['from']['id']

        self._chat_id_map[id] = (time.monotonic(), chat_id)

        if not self._can_join(message):
            return

        self._in_game = True

        try:
            name = message['from']['first_name']
        except KeyError:
            name = "Sr.{}".format(id)

        self._players.append(Player(id, name, chat_id))
        ludantoj = ", ".join(x.name for x in self._players)
        self._send_reply(message,
                         "Bonvenon. Aliaj ludantoj tajpu "
                         "/aligxi por aliĝi al la ludo aŭ tajpu /komenci "
                         "por komenci la ludon. La aktualaj ludantoj "
                         "estas:\n"
                         "{}".format(ludantoj))

        self._announce_game()

    def _process_command(self, message, command, args):
        chat = message['chat']

        if 'type' in chat and chat['type'] == 'private':
            if command == '/start':
                self._send_reply(message,
                                 "Ĉi tiu roboto estas por ludi la ludon "
                                 "Amletero. Tajpi la komandon /komenci en "
                                 "la taŭga grupo por ludi ĝin")
            elif command == '/helpo':
                self._send_reply(message, REGULOJ, True)
        elif 'id' in chat and chat['id'] == self._game_chat:
            if command == '/komenci':
                self._start_game(message)
            elif command == '/aligxi':
                self._join(message)
            elif command == '/helpo':
                self._send_reply(message, REGULOJ, True)

    def _find_command(self, message):
        if 'entities' not in message or 'text' not in message:
            return None

        for entity in message['entities']:
            if 'type' not in entity or entity['type'] != 'bot_command':
                continue

            start = entity['offset']
            length = entity['length']
            # For some reason the offsets are in UTF-16 code points
            text_utf16 = message['text'].encode('utf-16-le')
            command_utf16 = text_utf16[start * 2 : (start + length) * 2]
            command = command_utf16.decode('utf-16-le')
            remainder_utf16 = text_utf16[(start + length) * 2 :]
            remainder = remainder_utf16.decode('utf-16-le')

            at = command.find('@')
            if at != -1:
                if command[at + 1:] != self._botname:
                    return None
                command = command[0:at]

            return (command, remainder)

        return None

    def _answer_query(self, query):
        args = {
            'callback_query_id': query['id']
        }
        self._send_request('answerCallbackQuery', args)

    def _game_note(self, message):
        args = {
            'chat_id': self._game_chat,
            'text': message
        }

        self._send_request('sendMessage', args)

    def _get_winner(self):
        if len(self._deck) <= 0:
            best_player = None
            best_score = None
            for player in self._players:
                if not player.is_alive:
                    continue
                score = player.get_score()
                if best_player is None or score > best_score:
                    best_player = player
                    best_score = score

            return best_player

        alive_player = None

        for player in self._players:
            if not player.is_alive:
                continue

            # It’s only a winner if there is only one alive player
            if alive_player is not None:
                return None

            alive_player = player

        # It probably shouldn’t happen that there are no alive
        # players, but in order to fail safe we’ll just declare the
        # first player the winner
        if alive_player is None:
            return self._players[0]
        else:
            return alive_player                      

    def _show_stats(self):
        message = []

        if self._visible_cards is not None:
            message.append("Forĵetitaj kartoj: {}\n\n".format(
                "".join(card.symbol for card in self._visible_cards)))

        winner = self._get_winner()

        for i, player in enumerate(self._players):
            if winner is not None:
                if player == winner:
                    message.append("🏆 ")
            elif i == self._current_player:
                message.append("👉 ")

            message.append(html.escape(player.name))

            if not player.is_alive:
                message.append("☠")
            elif player.is_protected:
                message.append("🛡️")

            if len(player.discard_pile) > 0:
                message.append(":\n")

                message.append("".join(card.symbol
                                       for card in player.discard_pile))
                
            message.append("\n\n")

        if winner is not None:
            message.append("{} venkis!".format(winner.name))
        else:
            current_player = self._players[self._current_player]
            message.append("<b>{}</b>, estas via vico".format(
                html.escape(current_player.name)))

        args = {
            'chat_id': self._game_chat,
            'text': "".join(message),
            'parse_mode': 'HTML'
        }

        self._send_request('sendMessage', args)

    def _can_discard(self, card):
        current_player = self._players[self._current_player]

        if card is not current_player.card and card is not self._pending_card:
            return False

        if card is KING or card is PRINCE:
            return (current_player.card is not COMTESSE and
                    self._pending_card is not COMTESSE)

        return True

    def _explain_card(self, message, buttons, card):
        message.append("<b>{}</b>\n{}\n\n".format(
            html.escape(card.long_name()),
            html.escape(card.description)))

        if self._can_discard(card):
            buttons.append([{'text': card.name,
                             'callback_data': card.keyword }])

    def _take_turn(self):
        if self._get_winner() is not None:
            self._show_stats()
            self._reset_game()
            return

        current_player = self._players[self._current_player]
        self._pending_card = self._deck.pop()
        current_player.is_protected = False

        message = ["Kiun karton vi volas forĵeti?\n\n"]
        buttons = []

        self._explain_card(message, buttons, current_player.card)
        self._explain_card(message, buttons, self._pending_card)

        args = {
            'chat_id': current_player.chat_id,
            'text': "".join(message),
            'parse_mode': 'HTML',
            'reply_markup': { 'inline_keyboard': buttons }
        }

        self._send_request('sendMessage', args)
        self._show_stats()

    def _show_card(self, player):
        card = player.card
        message = "Via karto estas: {}".format(card.long_name())

        args = {
            'chat_id': player.chat_id,
            'text': message,
        }

        self._send_request('sendMessage', args)

    def _do_discard(self, card):
        self._start_discard(card)
        self._finish_discard()

    def _start_discard(self, card):
        current_player = self._players[self._current_player]

        if card is not self._pending_card:
            current_player.card = self._pending_card
        current_player.discard_pile.append(card)

    def _finish_discard(self):
        current_player = self._players[self._current_player]

        self._show_card(current_player)

        next_player = self._current_player

        while True:
            next_player = (next_player + 1) % len(self._players)

            if (next_player == self._current_player or
                self._players[next_player].is_alive):
                break

        self._current_player = next_player

        self._take_turn()

    def _get_targets(self):
        return [player for i, player in enumerate(self._players)
                if (i != self._current_player and player.is_alive and
                    not player.is_protected)]

    def _choose_target(self, note, keyword, targets = None):
        current_player = self._players[self._current_player]

        if targets is None:
            targets = self._get_targets()

        buttons = [ [ { 'text': player.name,
                        'callback_data': '{}:{}'.format(keyword, i) } ]
                    for i, player in enumerate(targets) ]

        args = {
            'chat_id': current_player.chat_id,
            'text': note,
            'reply_markup': { 'inline_keyboard': buttons }
        }

        self._send_request('sendMessage', args)

    def _choose_card(self, note, keyword, extra_data):
        current_player = self._players[self._current_player]

        buttons = [ [ { 'text': card.long_name(),
                        'callback_data': '{}:{}'.format(
                            keyword, 0x10000 | (i << 8) | extra_data) } ]
                    for i, card in enumerate(CHARACTERS) ]

        args = {
            'chat_id': current_player.chat_id,
            'text': note,
            'reply_markup': { 'inline_keyboard': buttons }
        }

        self._send_request('sendMessage', args)

    def _discard_guard(self, extra_data):
        current_player = self._players[self._current_player]

        targets = self._get_targets()

        if len(targets) == 0:
            self._game_note("{} forĵetas la {} sed ĉiuj aliaj ludantoj "
                            "estas protektataj kaj ĝi ne havas efikon.".format(
                                current_player.name,
                                GUARD.long_name(n=True)))
            self._do_discard(GUARD)
        elif extra_data is None:
            self._choose_target("Kies karton vi volas diveni?", GUARD.keyword)
        elif extra_data < 0x100:
            self._choose_card("Kiun karton vi divenas?",
                              GUARD.keyword,
                              extra_data)
        else:
            player_num = extra_data & 0xff
            if player_num >= len(targets):
                return
            target = targets[player_num]

            card_num = (extra_data >> 8) & 0xff
            if card_num >= len(CHARACTERS):
                return
            card = CHARACTERS[card_num]

            if card is target.card:
                self._game_note("{} forĵetis la {} kaj ĝuste divenis ke "
                                "{} havis la {}. {} perdas la raŭdon.".format(
                                    current_player.name,
                                    GUARD.long_name(n=True),
                                    target.name,
                                    card.long_name(n=True),
                                    target.name))
                target.is_alive = False
            else:
                self._game_note("{} forĵetis la {} kaj malĝuste divenis "
                                "ke {} havas la {}.".format(
                                    current_player.name,
                                    GUARD.long_name(n=True),
                                    target.name,
                                    card.long_name(n=True)))

            self._do_discard(GUARD)

    def _discard_spy(self, extra_data):
        current_player = self._players[self._current_player]

        targets = self._get_targets()

        if len(targets) == 0:
            self._game_note("{} forĵetas la {} sed ĉiuj aliaj ludantoj "
                            "estas protektataj kaj ĝi ne havas efikon.".format(
                                current_player.name,
                                SPY.long_name(n=True)))
            self._do_discard(SPY)
        elif extra_data is None:
            self._choose_target("Kies karton vi volas vidi?", SPY.keyword)
        else:
            player_num = extra_data
            if player_num >= len(targets):
                return
            target = targets[player_num]

            self._game_note("{} forĵetis la {} kaj devigis {} "
                            "sekrete montri sian karton.".format(
                                current_player.name,
                                SPY.long_name(n=True),
                                target.name))

            args = {
                'chat_id': current_player.chat_id,
                'text': '{} havas la {}'.format(
                    target.name, target.card.long_name(n=True))
            }
            self._send_request('sendMessage', args)

            self._do_discard(SPY)

    def _discard_baron(self, extra_data):
        current_player = self._players[self._current_player]

        targets = self._get_targets()

        if len(targets) == 0:
            self._game_note("{} forĵetas la {} sed ĉiuj aliaj ludantoj "
                            "estas protektataj kaj ĝi ne havas efikon.".format(
                                current_player.name,
                                BARON.long_name(n=True)))
            self._do_discard(BARON)
        elif extra_data is None:
            self._choose_target("Kun kies karto vi volas kompari?",
                                BARON.keyword)
        else:
            player_num = extra_data
            if player_num >= len(targets):
                return
            target = targets[player_num]

            self._start_discard(BARON)

            args = {
                'chat_id': current_player.chat_id,
                'text': 'Vi havas la {} kaj {} havas la {}'.format(
                    current_player.card.long_name(n=True),
                    target.name, target.card.long_name(n=True))
            }
            self._send_request('sendMessage', args)

            if current_player.card.value == target.card.value:
                self._game_note("{} forĵetis la {} kaj komparis sian "
                                "karton kun tiu de {}. La du kartoj estas "
                                "egalaj kaj neniu perdas la raŭdon.".format(
                                    current_player.name,
                                    BARON.long_name(n=True),
                                    target.name))
            else:
                if current_player.card.value > target.card.value:
                    loser = target
                else:
                    loser = current_player

                self._game_note("{} forĵetis la {} kaj komparis sian "
                                "karton kun tiu de {}. La karto de {} estas "
                                "malpli alta kaj ri perdas la raŭdon.".format(
                                    current_player.name,
                                    BARON.long_name(n=True),
                                    target.name,
                                    loser.name))
                loser.is_alive = False

            self._finish_discard()

    def _discard_handmaid(self, extra_data):
        current_player = self._players[self._current_player]

        self._game_note("{} forĵetas la {} kaj estos protektata ĝis "
                        "sia sekva vico".format(current_player.name,
                                                HANDMAID.long_name(n=True)))

        current_player.is_protected = True
        self._do_discard(HANDMAID)

    def _discard_prince(self, extra_data):
        current_player = self._players[self._current_player]

        targets = self._get_targets()
        targets.append(current_player)

        if extra_data is None:
            self._choose_target("Kiun vi volas devigi forĵeti sian manon?",
                                PRINCE.keyword,
                                targets)
        else:
            player_num = extra_data
            if player_num >= len(targets):
                return
            target = targets[player_num]

            self._start_discard(PRINCE)

            if target == current_player:
                target_name = "sin mem"
            else:
                target_name = target.name

            discarded_card = target.card

            if len(self._deck) > 0:
                target.card = self._deck.pop()
            else:
                target.card = self._set_aside_card

            target.discard_pile.append(discarded_card)

            if discarded_card == PRINCESS:
                self._game_note("{} forĵetis la {} kaj devigis {} "
                                "forĵeti sian princinon kaj tial ri perdas "
                                "la raŭdon.".format(current_player.name,
                                                    PRINCE.long_name(n=True),
                                                    target_name))
                target.is_alive = False
            else:
                self._game_note("{} forĵetis la {} kaj devigis {} "
                                "forĵeti sian {} kaj preni novan "
                                "karton."
                                "".format(current_player.name,
                                          PRINCE.long_name(n=True),
                                          target_name,
                                          discarded_card.long_name(n=True)))
                if target != current_player:
                    self._show_card(target)

            self._finish_discard()

    def _exchange_note(self, player_a, player_b):
            args = {
                'chat_id': player_a.chat_id,
                'text': ('Vi fordonas la {} al {} kaj '
                         'ricevas la {}'.format(
                             player_a.card.long_name(n=True),
                             player_b.name,
                             player_b.card.long_name(n=True))),
            }
            self._send_request('sendMessage', args)
        
    def _discard_king(self, extra_data):
        current_player = self._players[self._current_player]

        targets = self._get_targets()

        if len(targets) == 0:
            self._game_note("{} forĵetas la {} sed ĉiuj aliaj ludantoj "
                            "estas protektataj kaj ĝi ne havas efikon.".format(
                                current_player.name,
                                KING.long_name(n=True)))
            self._do_discard(KING)
        elif extra_data is None:
            self._choose_target("Kun kiu vi volas interŝanĝi manojn?",
                                KING.keyword)
        else:
            player_num = extra_data
            if player_num >= len(targets):
                return
            target = targets[player_num]

            self._start_discard(KING)

            self._exchange_note(current_player, target)
            self._exchange_note(target, current_player)

            (current_player.card, target.card) = (target.card,
                                                  current_player.card)

            self._game_note("{} forĵetis la reĝon kaj interŝanĝas la "
                            "manon kun {}".format(
                                current_player.name,
                                target.name))

            self._show_card(target)
            self._finish_discard()

    def _discard(self, card):
        current_player = self._players[self._current_player]
        self._game_note("{} forĵetas la {}".format(
            current_player.name, card.long_name(n=True)))
        self._do_discard(card)

    def _process_callback_query(self, query):
        try:
            from_id = query['from']['id']
            data = query['data']
        except KeyError:
            self._answer_query(query)
            return

        if not self._in_game or not self._game_running:
            self._answer_query(query)
            return

        current_player = self._players[self._current_player]

        colon = data.find(':')
        if colon == -1:
            extra_data = None
        else:
            try:
                extra_data = int(data[colon + 1:])
            except ValueError:
                return
            data = data[0:colon]

        if current_player.id == from_id:
            try:
                card = next(card for card in CHARACTERS
                            if card.keyword == data)
            except StopIteration:
                pass
            else:
                if self._can_discard(card):
                    if card == GUARD:
                        self._discard_guard(extra_data)
                    elif card == SPY:
                        self._discard_spy(extra_data)
                    elif card == BARON:
                        self._discard_baron(extra_data)
                    elif card == HANDMAID:
                        self._discard_handmaid(extra_data)
                    elif card == PRINCE:
                        self._discard_prince(extra_data)
                    elif card == KING:
                        self._discard_king(extra_data)
                    else:
                        self._discard(card)

        self._answer_query(query)

    def run(self):
        while True:
            now = int(time.time())

            try:
                updates = self._get_updates()
            except GetUpdatesException as e:
                print("{}".format(e), file=sys.stderr)
                # Delay for a bit before trying again to avoid DOSing the server
                time.sleep(60)
                continue

            if self._in_game:
                if (time.monotonic() - self._last_activity_time >=
                    INACTIVITY_TIMEOUT):
                    if self._game_running or len(self._players) < 2:
                        self._game_note("La ludo estas senaktiva dum pli ol {} "
                                        "minutoj kaj estos forlasita".format(
                                            INACTIVITY_TIMEOUT // 60))
                        self._reset_game()
                    else:
                        self._game_note("Atendis pli ol 5 minutoj sen novaj "
                                        "aliĝoj. La ludo tuj komenciĝos.")
                        self._do_start_game()

            for update in updates:
                try:
                    if 'message' in update:
                        message = update['message']
                        if 'chat' not in message:
                            continue
                        chat = message['chat']

                        if 'type' in chat and chat['type'] == 'private':
                            self._handle_private_message(message)

                        command = self._find_command(message)
                        if command is not None:
                            self._process_command(message,
                                                  command[0],
                                                  command[1])
                    elif 'callback_query' in update:
                        self._process_callback_query(update['callback_query'])
                except BotException as e:
                    print("{}".format(e), file=sys.stderr)

if __name__ == "__main__":
    bot = Bot()
    bot.run()
