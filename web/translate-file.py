#!/usr/bin/python3

# Puxcobot - A robot to play Coup in Esperanto (Puĉo)
# Copyright (C) 2011, 2020  Neil Roberts
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

import argparse
import re
import sys


def process_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--language",
                        help="Set the translation file",
                        required=True,
                        metavar="translation-file")
    parser.add_argument("-i", "--input",
                        help="Set the input file",
                        required=True,
                        metavar="input-file")
    parser.add_argument("-o", "--output",
                        help="Set the output file",
                        required=True,
                        metavar="output-file")
    return parser.parse_args()


def read_translation_file(filename):
    key_re = re.compile(r'^@([^@]+)@$')
    space_re = re.compile(r'[ \n]+') # don’t use \s to avoid replacing nbsp
    trans = {}
    key = None
    value = []

    def flush_value():
        if key is None:
            return

        trans[key] = space_re.sub(" ", "".join(value)).strip()
        value.clear()

    with open(filename, "rt", encoding="utf-8") as f:
        for line in f:
            md = key_re.match(line)

            if md is not None:
                flush_value()

                key = md.group(1)
            else:
                value.append(line)

    flush_value()

    return trans


def translate_file(trans, filename_in, filename_out):
    key_re = re.compile(r'@(\w+)@')
    line_num = 1

    def get_replacement(md):
        k = md.group(1)
        try:
            return trans[k]
        except KeyError:
            print("warning: {}:{}: untranslated key: {}".
                  format(filename_in,
                         line_num,
                         k),
                  file=sys.stderr)
            return md.group(0)

    with open(filename_in, "rt", encoding="utf-8") as infile:
        with open(filename_out, "wt", encoding="utf-8") as outfile:
            for line in infile:
                print(key_re.sub(get_replacement, line),
                      file=outfile,
                      end='')

                line_num += 1


if __name__ == '__main__':
    args = process_arguments()
    trans = read_translation_file(args.language)
    translate_file(trans, args.input, args.output)
