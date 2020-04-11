#!/usr/bin/env python3

import re
import glob
import os


STRINGS_START_RE = re.compile(r'^\s*pcx_text_[a-z0-9_A-Z]+\s*\[\]\s*=\s*{')
STRINGS_END_RE = re.compile(r'^\s*}')
STRING_RE = re.compile(r'^\s*\[([a-z0-9_A-Z]+)\]\s*=\s*')

COMMENT_LINE_RE = re.compile(r'^(\s*)(\S.*)')


def get_string_names():
    with open("pcx-text.h", 'rt', encoding='utf-8') as f:
        enums = []
        in_enum = False
        enum_start_re = re.compile(r'^\s*enum\s+pcx_text_string\s*{')
        enum_re = re.compile(r'^\s*([\w\d_]+)\s*,?\s*$')
        enum_end_re = re.compile(r'^\s*}')

        for line in f:
            if enum_start_re.match(line):
                in_enum = True
                continue

            if not in_enum:
                continue

            md = enum_re.match(line)
            if md:
                enums.append(md.group(1))
                continue

            if enum_end_re.match(line):
                in_enum = False
                continue

        return enums


def get_string_values(filename):
    values = {}
    buf = []
    current_string = None

    def flush_buf():
        nonlocal current_string, values, buf

        if current_string is None:
            return

        values[current_string] = "".join(buf)
        current_string = None
        buf.clear()

    with open(filename, 'rt', encoding='utf-8') as f:
        in_strings = False
        for line in f:
            if STRINGS_START_RE.match(line):
                in_strings = True
                continue

            if not in_strings:
                continue

            if STRINGS_END_RE.match(line):
                break

            md = STRING_RE.match(line)
            if md:
                flush_buf()
                current_string = md.group(1)
            elif current_string is not None:
                buf.append(line)

        flush_buf()

    return values


def comment_line(line):
    md = COMMENT_LINE_RE.match(line)
    if md is None:
        return ""
    else:
        return md.group(1) + "// " + md.group(2)


class Translations:
    def __init__(self):
        self.filenames = glob.glob('pcx-text-*.c')
        self.filenames.sort()

        self.files = dict((fn, get_string_values(fn)) for fn in self.filenames)

        self.names = get_string_names()

    def get_stub(self, name):
        # Prioritize an English stub
        try:
            english = self.files['pcx-text-english.c']
        except KeyError:
            pass
        else:
            try:
                return english[name]
            except KeyError:
                pass

        # Try any source
        for fn in self.filenames:
            v = self.files[fn]
            try:
                return v[name]
            except KeyError:
                pass

        # Make up a stub
        return "        \"\",\n"

    def get_commented_stub(self, name):
        stub = self.get_stub(name)
        return "\n".join(comment_line(line) for line in stub.split('\n'))


def dump_translations(translations, values, out_f):
    for name in translations.names:
        print('        ', file=out_f, end='')

        if name not in values:
            print('// ', file=out_f, end='')

        print('[{}] ='.format(name), file=out_f)

        try:
            print(values[name], end='', file=out_f)
        except KeyError:
            print(translations.get_commented_stub(name), end='', file=out_f)


translations = Translations()

for in_fn in translations.files:
    out_fn = in_fn + ".tmp"
    values = translations.files[in_fn]

    with open(out_fn, 'wt', encoding='utf-8') as out_f:
        with open(in_fn, 'rt', encoding='utf-8') as in_f:
            in_strings = False

            for line in in_f:
                if STRINGS_START_RE.match(line):
                    print(line, end='', file=out_f)
                    dump_translations(translations, values, out_f)
                    in_strings = True
                    continue

                if STRINGS_END_RE.match(line):
                    in_strings = False

                if not in_strings:
                    print(line, end='', file=out_f)

    os.rename(out_fn, in_fn)
