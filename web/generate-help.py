#!/usr/bin/env python3

import collections
import re

HEADER = """\
<!DOCTYPE html>

<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{0}</title>
    <link href="../pucxobot.css" rel="stylesheet" type="text/css">
  </head>
  <body>
    <div id="content">
      <div id="chatTitle">
        <div id="chatTitleText">
          {0}
        </div>
      </div>
      <div id="help">
        <h2>{1}</h2>
"""

FOOTER = """\
      </div>
    </div>
  </body>
</html>
"""

Lang = collections.namedtuple('Lang', ['code', 'name', 'help', 'toc'])

LANGUAGES = [
    Lang("en", "english", "Help", "Table of contents"),
    Lang("eo", "esperanto", "Helpo", "Enhavo"),
]

GAMES = [ "coup", "love", "zombie", "snitch", "superfight" ]

def get_game_name(game, lang):
    with open("../src/pcx-text-" + lang.name + ".c",
              'rt',
              encoding='utf-8') as f:
        b = f.read()
        m = re.search(r'\[PCX_TEXT_STRING_NAME_' +
                      game.upper() +
                      r'\] *= *\n' +
                      r' *"([^"]+)',
                      b)
        return m.group(1)

def get_game_help(game, lang):
    with open("../src/pcx-{}-help.c".format(game), 'rt', encoding='utf-8') as f:
        b = f.read()
        m = re.search(r'\[PCX_TEXT_LANGUAGE_' +
                      lang.name.upper() +
                      r'\] *= *\n(.+?)\n +\[',
                      b,
                      re.MULTILINE | re.DOTALL)
        lines = m.group(1).split("\n")[2:]

        in_p = False
        parts = []

        for line_num, line in enumerate(lines):
            filter_m = re.match(r' *"(.*?)(\\n)?",? *$', line)
            line = filter_m.group(1)
            nl = filter_m.group(2)

            if len(line) == 0:
                if in_p:
                    parts.append("        </p>")
                    in_p = False
            else:
                if not in_p:
                    m = re.match(" *<b>(.*)</b> *$", line)
                    if m:
                        parts.append("        <h3>{}</h3>".format(m.group(1)))
                        continue
                    else:
                        parts.append("        <p>")
                        in_p = True

            parts.append("        " + line)
            if (nl and
                len(line) > 0 and
                line_num + 1 < len(lines) and
                not re.match(r' *"\\n"', lines[line_num + 1])):
                parts[-1] = parts[-1] + "<br>"

        if in_p:
            parts.append("        </p>")

        return "\n".join(parts)

for lang in LANGUAGES:
    with open(lang.code + "/help.html", 'wt', encoding='utf-8') as f:
        print(HEADER.format(lang.help, lang.toc), file=f, end='')

        print("        <ul>", file=f)

        for game in GAMES:
            print("          <li><a href=\"#{}\">{}</a>".
                  format(game,
                         get_game_name(game, lang)),
                  file=f)

        print("        </ul>", file=f)

        for game in GAMES:
            print("\n        <h2><a id=\"{}\">{}</a></h2>\n\n{}".
                  format(game,
                         get_game_name(game, lang),
                         get_game_help(game, lang)),
                  file=f)

        print(FOOTER, file=f, end='')
