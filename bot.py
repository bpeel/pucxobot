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

class GetUpdatesException(Exception):
    pass

class HandleMessageException(Exception):
    pass

class ProcessCommandException(Exception):
    pass

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
            self._game_room = config["setup"]["game_room"]
        except KeyError:
            print("Missing game_room option in [setup] section of config",
                  file=sys.stderr)
            sys.exit(1)
        try:
            self._game_room = int(self._game_room)
        except ValueError:
            pass

        self._urlbase = "https://api.telegram.org/bot" + self._apikey + "/"
        self._get_updates_url = self._urlbase + "getUpdates"

        self._last_update_id = None

    def _is_valid_update(self, update):
        try:
            if 'message' not in update:
                return False

            message = update['message']

            if 'chat' not in message:
                return False
        except KeyError as e:
            raise GetUpdatesException(e)

        return True

    def _get_updates(self):
        args = {
            'allowed_updates': ['message']
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

        updates = [x for x in rep['result'] if self._is_valid_update(x)]
        return updates

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

    def _process_command(self, message, command, args):
        if command == '/start':
            self._send_reply(message,
                             "Roboto por ludi Puĉo-n")

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

            return (command, remainder)

        return None

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
                message = update['message']
                chat = message['chat']

                if 'type' in chat and chat['type'] == 'private':
                    command = self._find_command(message)
                    if command is not None:
                        try:
                            self._process_command(message,
                                                  command[0],
                                                  command[1])
                        except ProcessCommandException as e:
                            print("{}".format(e), file=sys.stderr)
                    continue
