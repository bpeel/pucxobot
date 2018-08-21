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

class Player:
    def __init__(self, id, name, chat_id):
        self.id = id
        self.name = name
        self.chat_id = chat_id

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
        self._show_stats()
        self._activity()
        for player in self._players:
            self._show_card(player)

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

    def _show_card(self, player):
        message = "Via karto estas: {} {}".format("?", "?")

        args = {
            'chat_id': player.chat_id,
            'text': "".message,
        }

        self._send_request('sendMessage', args)

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
