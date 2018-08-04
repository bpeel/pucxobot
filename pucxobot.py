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
import game
import html

REGULOJ = """\
<b>RESUMO DE LA REGULOJ:</b>

Puĉo estas kartludo de trompado kaj blufado.

Ĉiu komence havas 2 kartojn (fackaŝitajn) kaj 2 monerojn. Se oni perdas vivon oni devas malkaŝi karton kaj oni ne plu povas uzi ĝin.

Dum sia vico oni povas fari unu el la sevkaj agoj:

<b>Enspezi</b>: gajni unu moneron kaj neniu povas malhelpi ĝin.

<b>Eksterlanda helpo</b>: gajni du monerojn, sed se iu pretendas havi la dukon ri povas bloki ĝin.

<b>Puĉo</b>: pagi 7 monerojn por mortigi iun. Neniu povas malhelpi tion. Se iu havas 10 monerojn ri nepre devas fari puĉon.

Se oni havas unu el la sekvaj kartoj, aŭ pretendas havi ĝin, oni povas:

<b>Imposto (Duko)</b>: Preni 3 monerojn.

<b>Murdi (Murdisto)</b>: Pagi 3 monerojn kaj murdi iun. Se la viktimo pretendas havi la grafinon ri povas bloki ĝin.

<b>Interŝanĝi (Ambasadoro)</b>: Interŝanĝi siajn kartojn por du novaj kartoj.

<b>Ŝteli (Kapitano)</b>: Ŝteli 2 monerojn de alia ludanto. Se la viktimo pretendas havi la ambasadoron aŭ la kapitanon ri povas bloki ĝin.

Ekzistas nur po 3 kartoj de ĉiu rolulo.

Ĉiu pretendo de karto povas esti defiita de iu ajn alia ludanto. Se la defio estis prava, la defiito perdas karton, alikaze la defianto perdas karton.

Tiu kiu restas vivanta venkas."""

class BotException(Exception):
    pass

class GetUpdatesException(BotException):
    pass

class HandleMessageException(BotException):
    pass

class ProcessCommandException(BotException):
    pass

CHARACTER_BUTTONS = [
    { 'text': 'Imposto (Duko)', 'callback_data': 'tax' },
    { 'text': 'Murdi (Murdisto)', 'callback_data': 'assassinate' },
    { 'text': 'Interŝanĝi (Ambasadoro)', 'callback_data': 'exchange' },
    { 'text': 'Ŝteli (Kapitano)', 'callback_data': 'steal' }
]

BLOCK_BUTTON = {
    'text': 'Bloki', 'callback_data': 'block'
}

CHALLENGE_BUTTON = {
    'text': 'Defii', 'callback_data': 'challenge'
}

WAIT_TIME = 30
INACTIVITY_TIMEOUT = 5 * 60

class Bot:
    def __init__(self):
        conf_dir = os.path.expanduser("~/.pucxobot")

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
            self._botname = config["setup"]["botname"]
        except KeyError:
            print("Missing botname option in [setup] section of config",
                  file=sys.stderr)
            sys.exit(1)

        self._urlbase = "https://api.telegram.org/bot" + self._apikey + "/"
        self._get_updates_url = self._urlbase + "getUpdates"

        self._last_update_id = None
        self._reset_game()
        self._activity()

    def _cancel_block(self):
        self._pending_action = self._blocked_action
        self._blocking_player = None
        self._blocked_action = None

    def _reset_turn(self):
        self._blocking_player = None
        self._blocked_action = None
        self._pending_action = None
        self._pending_action_time = None
        self._pending_target = None
        # Pending cards to pick
        self._pending_exchange = None
        self._pending_exchange_message = None

    def _reset_game(self):
        self._game = None
        self._pending_joins = []
        self._reset_turn()

    def _activity(self):
        self._last_activity_time = time.monotonic()

    def _get_updates(self):
        if self._pending_action_time is not None:
            timeout = WAIT_TIME + self._pending_action_time - time.monotonic()
        elif self._game is not None:
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

    def _get_buttons(self):
        coup = {
            'text': 'Puĉo',
            'callback_data': 'coup'
        }

        player = self._game.players[self._game.current_player]

        if player.coins >= 10:
            return [coup]

        buttons = [
            { 'text': 'Enspezi', 'callback_data': 'income' },
            { 'text': 'Eksterlanda helpo', 'callback_data': 'foreign_aid' }
        ]

        if player.coins >= 7:
            buttons.append(coup)

        buttons.extend(CHARACTER_BUTTONS)

        if player.coins < 3:
            for i, button in enumerate(buttons):
                if button['callback_data'] == 'assassinate':
                    del buttons[i]
                    break

        return buttons

    def _get_keyboard(self):
        return list([x] for x in self._get_buttons())

    def _show_stats(self):
        message = []

        is_finished = self._game.is_finished()

        for i, player in enumerate(self._game.players):
            if is_finished:
                if player.is_alive():
                    message.append("🏆 ")
            elif i == self._game.current_player:
                message.append("👉 ")

            message.append(html.escape(player.name))
            message.append(":\n")

            for card in player.cards:
                if card.visible:
                    message.append(" ☠{}☠".format(card.character.value))
                else:
                    message.append("🂠")

            if player.is_alive():
                message.append(", ")

                if player.coins == 1:
                    message.append("1 monero")
                else:
                    message.append("{} moneroj".format(player.coins))

            message.append("\n\n")

        if is_finished:
            try:
                winner = next(x for x in self._game.players
                              if x.is_alive()).name
            except StopIteration:
                winner = "Neniu"

            message.append("{} venkis!".format(winner))
        else:
            current_player = self._game.players[self._game.current_player]
            message.append("{}, estas via vico, kion vi volas fari?".format(
                html.escape(current_player.name)))

        args = {
            'chat_id': self._game_chat,
            'text': "".join(message),
            'parse_mode': 'HTML'
        }

        if not is_finished:
            args['reply_markup'] = { 'inline_keyboard': self._get_keyboard() }

        self._send_request('sendMessage', args)

    def _do_start_game(self):
        self._game.start()
        self._show_stats()
        self._activity()
        for player in self._game.players:
            self._show_cards(player)

    def _start_game(self, message):
        id = message['from']['id']
        self._activity()

        if self._game is not None:
            if self._game.is_running:
                self._send_reply(message, "La ludo jam komenciĝis")
            elif not self._game.has_player(id):
                self._send_reply(message,
                                 "Unue aliĝu al la ludo por povi "
                                 "komenci ĝin")
            elif len(self._game.players) < 2:
                self._send_reply(message,
                                 "Necesas almenaŭ 2 ludantoj por ludi")
            else:
                self._do_start_game()
        else:
            self._join(message)

    def _can_join(self, message):
        if self._game is None:
            return True

        id = message['from']['id']

        if self._game.is_running:
            self._send_reply(message,
                             "La ludo jam komenciĝis kaj ne plu eblas aliĝi")
            return False
        if self._game.has_player(id):
            self._send_reply(message, "Vi jam estas en la ludo")
            return False
        if len(self._game.players) >= game.MAX_PLAYERS:
            self._send_reply(message, "Jam estas tro da ludantoj")
            return False

        return True

    def _join(self, message):
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

    def _really_join(self, message, chat_id):
        if not self._can_join(message):
            return

        if self._game is None:
            self._game = game.Game()

        id = message['from']['id']
        try:
            name = message['from']['first_name']
        except KeyError:
            name = "Sr.{}".format(id)

        self._activity()
        self._game.add_player(id, name, chat_id)
        ludantoj = ", ".join(x.name for x in self._game.players)
        self._send_reply(message,
                         "Bonvenon. Aliaj ludantoj tajpu "
                         "/aligxi por aliĝi al la ludo aŭ tajpu /komenci "
                         "por komenci la ludon. La aktualaj ludantoj "
                         "estas:\n"
                         "{}".format(ludantoj))

    def _process_command(self, message, command, args):
        chat = message['chat']

        if 'type' in chat and chat['type'] == 'private':
            if command == '/start':
                self._send_reply(message,
                                 "Ĉi tiu roboto estas por ludi la ludon "
                                 "Puĉo. Tajpi la komandon /komenci en "
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

    def _answer_bad_query(self, query):
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

    def _turn_over(self):
        self._reset_turn()

        if self._game.is_finished():
            self._show_stats()
            self._reset_game()
        else:
            self._game.next_player()
            self._show_stats()

    def _set_pending_action(self, action):
        self._pending_action = action
        self._pending_action_time = time.monotonic()

    def _show_cards(self, player):
        message = ["Viaj kartoj estas:\n"]

        for card in player.cards:
            if card.visible:
                message.append("☠{}☠\n".format(card.character.value))
            else:
                message.append("{}\n".format(card.character.value))

        args = {
            'chat_id': player.chat_id,
            'text': "".join(message),
        }

        self._send_request('sendMessage', args)

    def _lose_card(self, player):
        self._game.lose_card(player)
        self._show_cards(player)

    def _income(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} enspezas 1 moneron".format(player.name))
        player.coins += 1

        self._activity()
        self._turn_over()

    def _do_foreign_aid(self):
        player = self._game.players[self._game.current_player]

        self._game_note("Neniu blokis, {} prenas la 2 monerojn".format(
            player.name))
        player.coins += 2

        self._activity()
        self._turn_over()

    def _foreign_aid(self):
        player = self._game.players[self._game.current_player]

        args = {
            'chat_id': self._game_chat,
            'text': ('{} prenas 2 monerojn per eksterlanda helpo.\n'
                     'Ĉu iu volas pretendi havi la dukon kaj bloki rin?'.format(
                         player.name)),
            'reply_markup': { 'inline_keyboard': [[ BLOCK_BUTTON ]] }
        }

        self._activity()
        self._send_request('sendMessage', args)
        self._set_pending_action(self._do_foreign_aid)

    def _get_targets(self, action):
        buttons = []

        for i, target in enumerate(self._game.players):
            if (i == self._game.current_player or
                not target.is_alive()):
                continue
            buttons.append([{'text': target.name,
                             'callback_data': '{}:{}'.format(action, i)}])

        return buttons

    def _coup(self, target_num):
        player = self._game.players[self._game.current_player]

        if player.coins < 7:
            return

        if target_num is None:
            buttons = self._get_targets('coup')

            args = {
                'chat_id': self._game_chat,
                'text': "{}, kiun vi volas mortigi dum la puĉo?".format(
                    player.name),
                'reply_markup': { 'inline_keyboard': buttons }
            }

            self._send_request('sendMessage', args)
            return

        if (target_num < 0 or
            target_num >= len(self._game.players) or
            target_num == self._game.current_player):
            return

        target = self._game.players[target_num]

        if not target.is_alive():
            return

        self._game_note("{} faras puĉon kontraŭ {}".format(
            player.name, target.name))

        self._lose_card(target)
        player.coins -= 7

        self._activity()
        self._turn_over()

    def _do_tax(self, message=True):
        player = self._game.players[self._game.current_player]

        if message:
            self._game_note("Neniu blokis, {} prenas la 3 monerojn".format(
                player.name))
        player.coins += 3

        self._activity()
        self._turn_over()

    def _tax(self):
        player = self._game.players[self._game.current_player]

        args = {
            'chat_id': self._game_chat,
            'text': ('{} pretendas havi la dukon kaj prenas 3 monerojn per '
                     'imposto.\n'
                     'Ĉu iu volas defii rin?'.format(
                         player.name)),
            'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ]] }
        }

        self._activity()
        self._send_request('sendMessage', args)
        self._set_pending_action(self._do_tax)

    def _do_assassinate(self):
        player = self._game.players[self._game.current_player]

        self._game_note("Neniu blokis aŭ defiis, {} murdas {}".format(
            player.name, self._pending_target.name))
        player.coins -= 3

        self._activity()
        self._lose_card(self._pending_target)
        self._turn_over()

    def _assassinate(self, target_num):
        player = self._game.players[self._game.current_player]

        if player.coins < 3:
            return

        if target_num is None:
            buttons = self._get_targets('assassinate')

            args = {
                'chat_id': self._game_chat,
                'text': "{}, kiun vi volas murdi?".format(
                    player.name),
                'reply_markup': { 'inline_keyboard': buttons }
            }

            self._send_request('sendMessage', args)
            return

        if (target_num < 0 or
            target_num >= len(self._game.players) or
            target_num == self._game.current_player):
            return

        target = self._game.players[target_num]

        if not target.is_alive():
            return

        args = {
            'chat_id': self._game_chat,
            'text': ("{} volas murdi {}\n"
                     "{}, ĉu vi volas bloki ĝin per grafino?\n"
                     "Aŭ ĉu iu volas defii?".format(
                         player.name, target.name, target.name)),
            'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ],
                                                  [ BLOCK_BUTTON ]] }
        }

        self._activity()
        self._send_request('sendMessage', args)
        self._pending_target = target
        self._set_pending_action(self._do_assassinate)

    def _keep(self, extra_data):
        player = self._game.players[self._game.current_player]

        if (extra_data is None or
            extra_data < 0 or
            extra_data >= len(self._pending_exchange)):
            return

        character = self._pending_exchange[extra_data]
        del self._pending_exchange[extra_data]
        player.cards.append(game.Card(character))
        self._activity()

        if len(player.cards) >= 2:
            self._show_cards(player)
            self._turn_over()
        else:
            self._send_exchange_message()

    def _send_exchange_message(self):
        player = self._game.players[self._game.current_player]

        buttons = [[ { 'text': x.value, 'callback_data': "keep:{}".format(i) } ]
                   for i, x in enumerate(self._pending_exchange)]

        args = {
            'chat_id': player.chat_id,
            'text': ("Kiujn kartojn vi volas konservi?".format(
                player.name)),
            'reply_markup': { 'inline_keyboard': buttons }
        }

        if self._pending_exchange_message is not None:
            args['message_id'] = self._pending_exchange_message
            rep = self._send_request('editMessageText', args)
        else:
            rep = self._send_request('sendMessage', args)

        return rep['result']['message_id']

    def _do_exchange(self):
        player = self._game.players[self._game.current_player]

        self._reset_turn()
        self._pending_exchange = []
        dead_cards = []

        while (len(self._pending_exchange) < 2 and
               len(self._game.deck) >= 1):
            character = self._game.deck.pop()
            self._pending_exchange.append(character)

        for card in player.cards:
            if card.visible:
                dead_cards.append(card)
            else:
                self._pending_exchange.append(card.character)

        player.cards = dead_cards

        self._activity()
        self._pending_exchange_message = self._send_exchange_message()
        self._game_note('Neniu blokis, {} interŝanĝas kartojn'.format(
            player.name))

    def _exchange(self):
        player = self._game.players[self._game.current_player]

        args = {
            'chat_id': self._game_chat,
            'text': ("{} pretendas havi la ambasadoron kaj volas interŝanĝi "
                     "kartojn, ĉu iu volas defii rin?".format(
                player.name)),
            'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ]] }
        }

        self._activity()
        self._send_request('sendMessage', args)
        self._set_pending_action(self._do_exchange)

    def _do_steal(self):
        player = self._game.players[self._game.current_player]

        self._game_note("Neniu blokis aŭ defiis, {} ŝtelas de {}".format(
            player.name, self._pending_target.name))
        amount = min(2, self._pending_target.coins)
        player.coins += amount
        self._pending_target.coins -= amount

        self._activity()
        self._turn_over()

    def _steal(self, target_num):
        player = self._game.players[self._game.current_player]

        if target_num is None:
            buttons = self._get_targets('steal')

            args = {
                'chat_id': self._game_chat,
                'text': "{}, de kiu vi volas ŝteli?".format(
                    player.name),
                'reply_markup': { 'inline_keyboard': buttons }
            }

            self._send_request('sendMessage', args)
            return

        if (target_num < 0 or
            target_num >= len(self._game.players) or
            target_num == self._game.current_player):
            return

        target = self._game.players[target_num]

        if not target.is_alive():
            return

        args = {
            'chat_id': self._game_chat,
            'text': ("{} volas ŝteli de {}\n"
                     "{}, ĉu vi volas bloki ĝin per ambasadoro aŭ kapitano?\n"
                     "Aŭ ĉu iu volas defii?".format(
                         player.name, target.name, target.name)),
            'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ],
                                                  [ BLOCK_BUTTON ]] }
        }

        self._activity()
        self._send_request('sendMessage', args)
        self._pending_target = target
        self._set_pending_action(self._do_steal)

    def _do_block(self):
        self._game_note("Neniu defiis kaj la ago estis blokita")
        self._turn_over()
        self._activity()

    def _do_block_assassinate(self):
        self._game.players[self._game.current_player].coins -= 3
        self._do_block()
        self._activity()

    def _challenge(self, from_id):
        try:
            (player_num, player) = next(x for x in enumerate(self._game.players)
                                        if x[1].id == from_id)
        except StopIteration:
            return

        if not player.is_alive():
            return

        self._activity()

        if self._blocked_action == self._do_foreign_aid:
            if self._game.show_card(self._blocking_player, game.Character.DUKE):
                self._game_note("{} defiis sed {} ja havis la dukon kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    self._blocking_player.name,
                                    player.name))
                self._lose_card(player)
                self._show_cards(self._blocking_player)
                self._turn_over()
            else:
                self._game_note("{} defiis kaj {} ne havis la dukon "
                                "kaj perdas karton".format(
                                    player.name,
                                    self._blocking_player.name))
                self._lose_card(self._blocking_player)
                self._cancel_block()
        elif self._pending_action == self._do_tax:
            current_player = self._game.players[self._game.current_player]

            if self._game.show_card(current_player, game.Character.DUKE):
                self._game_note("{} defiis sed {} ja havis la dukon kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    current_player.name,
                                    player.name))
                self._lose_card(player)
                self._show_cards(current_player)
                self._do_tax(message=False)
            else:
                self._game_note("{} defiis kaj {} ne havis la dukon "
                                "kaj perdas karton".format(
                                    player.name,
                                    current_player.name))
                self._lose_card(current_player)
                self._turn_over()
        elif self._blocked_action == self._do_assassinate:
            if self._game.show_card(self._pending_target,
                                    game.Character.CONTESSA):
                self._game_note("{} defiis sed {} ja havis la grafinon kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    self._pending_target.name,
                                    player.name))
                self._game.players[self._game.current_player].coins -= 3
                self._lose_card(player)
                self._show_cards(self._pending_target)
                self._turn_over()
            else:
                self._game_note("{} defiis kaj {} ne havis la grafinon "
                                "kaj perdas karton".format(
                                    player.name,
                                    self._pending_target.name))
                self._lose_card(self._pending_target)
                self._do_assassinate()
        elif self._pending_action == self._do_assassinate:
            current_player = self._game.players[self._game.current_player]
            if self._game.show_card(current_player,
                                    game.Character.ASSASSIN):
                self._game_note("{} defiis sed {} ja havis la murdiston kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    current_player.name,
                                    player.name))
                self._lose_card(player)
                self._show_cards(current_player)
                self._do_assassinate()
            else:
                self._game_note("{} defiis kaj {} ne havis la murdiston "
                                "kaj perdas karton".format(
                                    player.name,
                                    current_player.name))
                self._lose_card(current_player)
                self._turn_over()
        elif self._blocked_action == self._do_steal:
            card = self._game.show_card(self._pending_target,
                                        game.Character.AMBASSADOR,
                                        game.Character.CAPTAIN)
            if card is not None:
                self._game_note("{} defiis sed {} ja havis la {}n kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    self._pending_target.name,
                                    card.value,
                                    player.name))
                self._lose_card(player)
                self._show_cards(self._pending_target)
                self._turn_over()
            else:
                self._game_note("{} defiis kaj {} ne havis la ambasadoron "
                                "nek la kapitanon kaj perdas karton".format(
                                    player.name,
                                    self._pending_target.name))
                self._lose_card(self._pending_target)
                self._do_steal()
        elif self._pending_action == self._do_steal:
            current_player = self._game.players[self._game.current_player]
            if self._game.show_card(current_player, game.Character.CAPTAIN):
                self._game_note("{} defiis sed {} ja havis la kapitanon kaj {} "
                                "perdas karton".format(
                                    player.name,
                                    current_player.name,
                                    player.name))
                self._lose_card(player)
                self._show_cards(current_player)
                self._do_steal()
            else:
                self._game_note("{} defiis kaj {} ne havis la kapitanon "
                                "kaj perdas karton".format(
                                    player.name,
                                    current_player.name))
                self._lose_card(current_player)
                self._turn_over()
        elif self._pending_action == self._do_exchange:
            current_player = self._game.players[self._game.current_player]
            if self._game.show_card(current_player,
                                    game.Character.AMBASSADOR):
                self._game_note("{} defiis sed {} ja havis la ambasadoron kaj "
                                "{} perdas karton".format(
                                    player.name,
                                    current_player.name,
                                    player.name))
                self._lose_card(player)
                self._show_cards(current_player)
                self._do_exchange()
            else:
                self._game_note("{} defiis kaj {} ne havis la ambasadoron "
                                "kaj perdas karton".format(
                                    player.name,
                                    current_player.name))
                self._lose_card(current_player)
                self._turn_over()

    def _block(self, from_id):
        try:
            (player_num, player) = next(x for x in enumerate(self._game.players)
                                        if x[1].id == from_id)
        except StopIteration:
            return

        if not player.is_alive():
            return

        self._activity()

        if self._pending_action == self._do_foreign_aid:
            if player_num == self._game.current_player:
                return

            args = {
                'chat_id': self._game_chat,
                'text': ('{} pretendas havi la dukon kaj blokas '
                         'la eksterlandan helpon. Ĉu iu volis defii '
                         'rin?'.format(player.name)),
                'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ]] }
            }

            self._send_request('sendMessage', args)

            self._blocking_player = player
            self._blocked_action = self._pending_action
            self._set_pending_action(self._do_block)
        elif self._pending_action == self._do_assassinate:
            if player != self._pending_target:
                return

            args = {
                'chat_id': self._game_chat,
                'text': ('{} pretendas havi la grafinon kaj blokas '
                         'la murdon. Ĉu iu volis defii '
                         'rin?'.format(player.name)),
                'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ]] }
            }

            self._send_request('sendMessage', args)

            self._blocking_player = player
            self._blocked_action = self._pending_action
            self._set_pending_action(self._do_block_assassinate)
        elif self._pending_action == self._do_steal:
            if player != self._pending_target:
                return

            args = {
                'chat_id': self._game_chat,
                'text': ('{} pretendas havi la kapitanon aŭ la ambasadoron '
                         'kaj blokas la ŝtelon. Ĉu iu volis defii '
                         'rin?'.format(player.name)),
                'reply_markup': { 'inline_keyboard': [[ CHALLENGE_BUTTON ]] }
            }

            self._send_request('sendMessage', args)

            self._blocking_player = player
            self._blocked_action = self._pending_action
            self._set_pending_action(self._do_block)

    def _process_callback_query(self, query):
        try:
            from_id = query['from']['id']
            data = query['data']
        except KeyError:
            self._answer_bad_query(query)
            return

        if (self._game is None or
            not self._game.is_running):
            self._answer_bad_query(query)
            return

        current_player = self._game.players[self._game.current_player]

        colon = data.find(':')
        if colon == -1:
            extra_data = None
        else:
            try:
                extra_data = int(data[colon + 1:])
            except ValueError:
                return
            data = data[0:colon]

        if data == 'challenge':
            self._challenge(from_id)
        elif data == 'block':
            self._block(from_id)
        elif current_player.id == from_id:
            if self._pending_exchange:
                if data == 'keep':
                    self._keep(extra_data)
            elif not self._blocked_action and not self._pending_action:
                if data == 'coup':
                    self._coup(extra_data)
                elif current_player.coins < 10:
                    if data == 'income':
                        self._income()
                    elif data == 'foreign_aid':
                        self._foreign_aid()
                    elif data == 'tax':
                        self._tax()
                    elif data == 'assassinate':
                        self._assassinate(extra_data)
                    elif data == 'exchange':
                        self._exchange()
                    elif data == 'steal':
                        self._steal(extra_data)

        self._answer_bad_query(query)

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

            if self._pending_action_time is not None:
                if time.monotonic() - self._pending_action_time >= WAIT_TIME:
                    self._pending_action()
            elif self._game is not None:
                if (time.monotonic() - self._last_activity_time >=
                    INACTIVITY_TIMEOUT):
                    if self._game.is_running or len(self._game.players) < 2:
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
