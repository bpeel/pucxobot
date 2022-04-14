#!/usr/bin/python3

# Puxcobot - A robot to play Coup in Esperanto (Puĉo)
# Copyright (C) 2022  Neil Roberts
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

WORDS = [
    "terpomo",
    "terpomoj"
    "a",
    "zzz",
    "eĥoŝanĝoĉiuĵaŭde",
]

TEST_WORDS = {
    "terpom" : False,
    "terpomojn" : False,
    "zzzz" : False,
    "-" : False,
    "eĥo" : False,
    "eĥoŝanĝoĉiuĵaŭd🤯" : False,
}

for word in WORDS:
    TEST_WORDS[word] = True

if len(sys.argv) != 3:
    print("usage: test-trie.py <test-trie executable> "
          "<make-dictionary executable>",
          file=sys.stderr)
    sys.exit(1)

test_program = sys.argv[1]
trie_program = sys.argv[2]

with tempfile.NamedTemporaryFile() as dictionary:
    trie_proc = subprocess.Popen([trie_program, dictionary.name],
                                 stdin=subprocess.PIPE)
    trie_proc.communicate("\n".join(WORDS).encode("utf-8"))
    if trie_proc.wait() != 0:
        print("make-dictionary failed", file=sys.stderr)
        sys.exit(1)

    input = []
    expected_output = []

    for word, result in TEST_WORDS.items():
        input.append(word)
        expected_output.append("{}: {}".format(word, "yes" if result else "no"))

    test_proc = subprocess.Popen([test_program, dictionary.name, *input],
                                 encoding="utf-8",
                                 stdout=subprocess.PIPE)

    expected_iter = iter(expected_output)

    result = True

    for line in test_proc.stdout:
        try:
            expected_line = next(expected_iter)
        except StopIteration:
            print("Extra line in output: {}".format(line), file=sys.stderr)
            sys.exit(1)

        if line.rstrip() != expected_line:
            print("Mismatched line:\n"
                  "< {}\n"
                  "> {}".format(expected_line, line), end='')
            result = False

    try:
        next(expected_iter)
    except StopIteration:
        pass
    else:
        print("Not enough lines from test output", file=sys.stderr)
        result = False

    if not result:
        sys.exit(1)

    with tempfile.NamedTemporaryFile(mode="wt", encoding="utf-8") as word_list:
        for word in sorted(WORDS):
            print(word, file=word_list)

        word_list.flush()

        with tempfile.NamedTemporaryFile() as tried_list:
            subprocess.check_call([test_program, dictionary.name],
                                  stdout=tried_list)
            tried_list.flush()

            subprocess.check_call(["diff", word_list.name, tried_list.name])
