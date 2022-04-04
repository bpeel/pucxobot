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

# The trie on disk is stored as a single trie node. The trie node is a
# recursive data structure defined as follows:
#
# • A byte offset to the next sibling node, not including this length.
#   The number is stored as a variable-length integer. Each byte
#   contains the next most-significant 7 bits. The topmost bit of the
#   byte determines whether there are more bits to follow.
# • 1-6 bytes of UTF-8 encoded data to represent the character of this node.
#
# This is followed by the child nodes of this node. The children are
# counted within the length of the parent node.
#
# The character of the root node should be ignored.
#
# If the character is "\0" then it means the letters in the chain of
# parents leading up to this node are a valid word.

def n_bytes_for_size(size):
    return max((size.bit_length() + 6) // 7, 1)

def encode_size(size):
    prefix_size = n_bytes_for_size(size)

    def child_byte(index):
        byte = (size >> (index * 7)) & 127
        if index < prefix_size - 1:
            byte |= 0x80

        return byte

    return bytes(child_byte(i) for i in range(prefix_size))

class Node:
    def __init__(self, letter):
        self.letter = letter
        self.children = {}
        self._size = None

    def get_size(self):
        if self._size is not None:
            return self._size

        def child_size(child):
            size = child.get_size()
            return size + n_bytes_for_size(size)

        size = (sum(child_size(child) for child in self.children.values()) +
                len(self.letter.encode("utf-8")))

        self._size = size

        return size

class Trie:
    def __init__(self):
        self.root = Node('*')

    def add_word(self, word):
        node = self.root

        for letter in word:
            try:
                node = node.children[letter]
            except KeyError:
                child_node = Node(letter)
                node.children[letter] = child_node
                node = child_node

        node.children["\0"] = Node("\0")

    def words(self):
        stack = [(trie.root, iter(trie.root.children.items()))]

        while len(stack) > 0:
            try:
                letter, value = next(stack[-1][1])
            except StopIteration:
                stack.pop()
                continue

            if letter == "\0":
                yield "".join(entry[0].letter for entry in stack[1:])

            stack.append((value, iter(value.children.items())))

    def _start_node(self, stack, node, f):
        stack.append(iter(node.children.values()))

        f.write(encode_size(node.get_size()))
        f.write(node.letter.encode("utf-8"))

    def write(self, f):
        stack = []

        self._start_node(stack, self.root, f)

        while len(stack) > 0:
            try:
                child = next(stack[-1])
            except StopIteration:
                stack.pop()
                continue

            self._start_node(stack, child, f)

if len(sys.argv) != 2:
    print("usage: trie.py <output_file>", file=sys.stderr)
    sys.exit(1)

trie = Trie()

for line in sys.stdin:
    trie.add_word(line.rstrip())

with open(sys.argv[1], "wb") as f:
    trie.write(f)
