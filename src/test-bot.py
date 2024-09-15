#!/usr/bin/python3

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

import sys
import os.path
import tempfile
import subprocess
import re


def run_script(fake_telegram, script):
    for line_num, line in enumerate(script):
        if line.startswith("<<"):
            fake_telegram.stdin.flush()

            terminator = line[2:]

            while True:
                actual = fake_telegram.stdout.readline()

                if len(actual) == 0:
                    print(f"unexpected EOF on line {line_num + 1}")
                    return False
                elif actual == terminator:
                    break
        elif line.startswith("<"):
            fake_telegram.stdin.flush()

            expected = line[1:]
            actual = fake_telegram.stdout.readline()

            if expected != actual:
                print(f"output does not match on line {line_num + 1}\n"
                      f"expected: {expected.rstrip()}\n"
                      f"actual:   {actual.rstrip()}")
                return False
        else:
            print(line, end='', file=fake_telegram.stdin)

    return True


if len(sys.argv) != 4:
    print("usage: test-dictionary.py <fake-telegram executable> "
          "<pucxobot executable> "
          "<test script>",
          file=sys.stderr)
    sys.exit(1)

fake_telegram_file = sys.argv[1]
pucxobot_file = sys.argv[2]
script_file = sys.argv[3]

fake_telegram = subprocess.Popen(fake_telegram_file,
                                 stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 text="utf-8")

port_line = fake_telegram.stdout.readline()

md = re.match(r'^Listening on .*:([0-9]+)$', port_line)

fake_telegram_port = int(md.group(1))

returncode = 0

with tempfile.TemporaryDirectory() as data_dir:
    conf_file = os.path.join(data_dir, "conf.txt")

    with open(conf_file, "w", encoding="utf-8") as f:
        print(f"""\
[general]
telegram_url = http://localhost:{fake_telegram_port}
data_dir = {data_dir}

[bot]
apikey = 123:1252_llutbt
botname = testbot
language = eo
""",
              file=f)

    pucxobot = subprocess.Popen([pucxobot_file, "-c", conf_file])

    with open(script_file, "r", encoding="utf-8") as f:
        if not run_script(fake_telegram, f):
            returncode = 1

    pucxobot.terminate()

    if pucxobot.wait() != 0:
        print("pucxobot had non-zero exit status", file=sys.stderr)
        returncode = 1

fake_telegram.stdin.close()

if fake_telegram.wait() != 0:
   print("fake-telegram had non-zero exit status", file=sys.stderr)
   returncode = 1

sys.exit(returncode)
