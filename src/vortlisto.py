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

from lxml import etree
from glob import glob
import re

class WordList:
    ALPHA_RE = re.compile(r'[^a-pr-vzĥŝĝĉĵŭ]')

    def __init__(self):
        self.words = set()

    def add_word(self, word):
        self.words.add(word)

    def add_noun(self, root):
        for ending in ["o", "oj", "on", "ojn"]:
            self.add_word(root + ending)

    def add_simple_adjective(self, root):
        for ending in ["a", "aj", "an", "ajn", "e"]:
            self.add_word(root + ending)

    def add_adjective(self, root):
        self.add_simple_adjective(root)
        self.add_verb(root)
        self.add_verb(root + "ig")
        self.add_verb(root + "iĝ")

    def add_verb(self, root):
        self.add_word(root + "i")

        for tense in "iao":
            participle = root + tense + "nt"
            self.add_noun(participle)
            self.add_simple_adjective(participle)

            self.add_word(root + tense + "s")

        self.add_noun(root + "ad")

    def _parse_kap(self, root, kap):
        before = []
        after = []
        current_list = before

        if kap.text is not None:
            before.append(kap.text)

        for element in kap:
            if element.tag == "tld":
                current_list.append(root)
                current_list = after

            if element.tail is not None:
                current_list.append(element.tail)

        return ("".join(before).strip(), "".join(after).strip())

    def _contains_nonalpha(self, word):
        return bool(self.ALPHA_RE.search(word))

    def add_from_xml(self, filename):
        parser = etree.XMLParser(load_dtd=True,
                                 no_network=False)
        tree = etree.parse(filename, parser=parser)

        article = tree.xpath("/vortaro/art")[0]
        root_node = article.xpath("./kap/rad")
        root = "".join(root_node[0].itertext()).strip()

        for derivation in article.xpath("./drv/kap"):
            (before, after) = self._parse_kap(root, derivation)

            if (self._contains_nonalpha(before) or
                self._contains_nonalpha(after)):
                continue

            if after == "i":
                self.add_verb(before)
            elif after == "o":
                self.add_noun(before)
            elif after == "a":
                self.add_adjective(before)
            else:
                self.add_word(before + after)

def add_correlatives(word_list):
    # The correlatives are listed as root words so they don’t get the
    # declinations automatically

    for prefix in ["i", "ki", "ti", "ĉi", "neni"]:
        for suffix in ["aj", "an", "ajn",
                       "on",
                       "uj", "un", "ujn"]:
            word_list.add_word(prefix + suffix)

word_list = WordList()

add_correlatives(word_list)

for xml_file in glob("*.xml"):
    word_list.add_from_xml(xml_file)

for word in sorted(word_list.words):
    print(word)
