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

        self._game = None

    def _get_updates(self):
        args = {
            'allowed_updates': ['message', 'callback_query']
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

    def _send_reply(self, message, note):
        args = {
            'chat_id' : message['chat']['id'],
            'text' : note,
            'reply_to_message_id' : message['message_id']
        }

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

        return buttons

    def _get_keyboard(self):
        return list([x] for x in self._get_buttons())

    def _show_stats(self):
        message = []

        for i, player in enumerate(self._game.players):
            if i == self._game.current_player:
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

        message.append("{}, estas via vico, kion vi volas fari?".format(
            html.escape(self._game.players[self._game.current_player].name)))

        args = {
            'chat_id': self._game_chat,
            'text': "".join(message),
            'parse_mode': 'HTML',
            'reply_markup': { 'inline_keyboard': self._get_keyboard() }
        }

        self._send_request('sendMessage', args)

    def _start_game(self, message):
        id = message['from']['id']

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
                self._game.start()
                self._show_stats()
        else:
            self._join(message)

    def _join(self, message):
        if self._game is None:
            self._game = game.Game()

        id = message['from']['id']
        try:
            name = message['from']['first_name']
        except KeyError:
            name = "Sr.{}".format(id)

        if self._game.is_running:
            self._send_reply(message,
                             "La ludo jam komenciĝis kaj ne plu eblas aliĝi")
        elif self._game.has_player(id):
            self._send_reply(message, "Vi jam estas en la ludo")
        elif len(self._game.players) >= game.MAX_PLAYERS:
            self._send_reply(message, "Jam estas tro da ludantoj")
        else:
            self._game.add_player(id, name)
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
                send_reply(message,
                           "Ĉi tiu roboto estas por ludi la ludon "
                           "Puĉo. Tajpi la komandon /komenci en "
                           "la taŭga grupo por ludi ĝin")
        elif 'id' in chat and chat['id'] == self._game_chat:
            if command == '/komenci':
                self._start_game(message)
            elif command == '/aligxi':
                self._join(message)

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
        if self._game.is_finished():
            try:
                winner = next(x for x in self._game.players
                              if x.is_alive()).name
            except StopIteration:
                winner = "Neniu"

            self._game_note("{} venkis!".format(winner))
            self._game = None

        else:
            self._game.next_player()
            self._show_stats()

    def _income(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} enspezas 1 moneron".format(player.name))
        player.coins += 1

        self._turn_over()

    def _foreign_aid(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} prenas 2 monerojn per eksterlanda helpo".format(
            player.name))
        player.coins += 2

        self._turn_over()

    def _coup(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} faras puĉon".format(
            player.name))

        self._turn_over()

    def _tax(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} prenas 3 monerojn pro imposto".format(
            player.name))
        player.coins += 3

        self._turn_over()

    def _assassinate(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} murdas".format(
            player.name))

        self._turn_over()

    def _exchange(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} interŝanĝas".format(
            player.name))

        self._turn_over()

    def _steal(self):
        player = self._game.players[self._game.current_player]

        self._game_note("{} ŝtelas".format(
            player.name))

        self._turn_over()

    def _process_callback_query(self, query):
        try:
            from_id = query['from']['id']
            data = query['data']
        except KeyError:
            self._answer_bad_query(query)
            return

        if (self._game is None or
            not self._game.is_running or
            self._game.players[self._game.current_player].id != from_id):
            self._answer_bad_query(query)
            return

        if data == 'income':
            self._income()
        elif data == 'foreign_aid':
            self._foreign_aid()
        elif data == 'coup':
            self._coup()
        elif data == 'tax':
            self._tax()
        elif data == 'assassinate':
            self._assassinate()
        elif data == 'exchange':
            self._exchange()
        elif data == 'steal':
            self._steal()
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

            for update in updates:
                try:
                    if 'message' in update:
                        message = update['message']
                        if 'chat' not in message:
                            continue
                        command = self._find_command(message)

                        if command is None:
                            continue

                        self._process_command(message,
                                              command[0],
                                              command[1])
                    elif 'callback_query' in update:
                        self._process_callback_query(update['callback_query'])
                except BotException as e:
                    print("{}".format(e), file=sys.stderr)
