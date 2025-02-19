#!/usr/bin/env python3

import collections
import re
import subprocess
import tempfile

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

Lang = collections.namedtuple('Lang',
                              ['code',
                               'name',
                               'help',
                               'toc',
                               'about',
                               'telegram_bot',
                               'source_code',
                               'skip_games'],
                              defaults=[None, None, None, set()])

LANGUAGES = [
    Lang("en", "english", "Help", "Table of contents", "About",
         "The games on this site are also available via the Telegram bot "
         "<a href=\"https://t.me/bluffingbot\">@bluffingbot</a>.",
         "The source code is available on "
         "<a href=\"https://github.com/bpeel/pucxobot\">Github</a>.",
         skip_games=set(["wordparty"])),
    Lang("eo", "esperanto", "Helpo", "Enhavo", "Pri",
         "La ludoj de ĉi tiu retejo ankaŭ estas ludeblaj ĉe Telegram per "
         "<a href=\"https://t.me/pucxobot\">@pucxobot</a>.",
         "La programkodo estas disponebla ĉe "
         "<a href=\"https://github.com/bpeel/pucxobot\">Github</a>."),
    Lang("fr", "french", "Aide", "Sommaire", "À propos",
         "Les jeux de ce site sont aussi disponibles sur Telegram avec le bot "
         "<a href=\"https://t.me/complotbot\">@complotbot</a>.",
         "Le code source est disponible sur "
         "<a href=\"https://github.com/bpeel/pucxobot\">Github</a>.",
         skip_games=set(["superfight", "fox", "wordparty", "chameleon"])),
]

GAMES = [ "coup", "love", "werewolf", "six", "fox", "wordparty", "chameleon",
          "zombie", "snitch", "superfight" ]

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

def get_game_help_lines(game, lang):
    with tempfile.NamedTemporaryFile(mode='rt', encoding='utf-8') as res:
        with tempfile.NamedTemporaryFile(mode='wt', encoding='utf-8') as cmds:
            print(("start\n"
                   "set $t=pcx_{}_game.get_help_cb(PCX_TEXT_LANGUAGE_{})\n"
                   "set $f=(FILE*)fopen(\"{}\", \"w\")\n"
                   "call (void)fputs($t,$f)\n"
                   "call (void)fclose($f)").
                  format(game, lang.name.upper(), res.name),
                  file=cmds)
            cmds.flush()
            subprocess.check_call(["gdb",
                                   "-q",
                                   "-batch",
                                   "-x", cmds.name,
                                   "../build/src/pucxobot"])

        return res.read().split("\n")

def get_game_help(game, lang):
    lines = get_game_help_lines(game, lang)[2:]

    in_p = False
    parts = []

    for line_num, line in enumerate(lines):
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
        if (len(line) > 0 and
            line_num + 1 < len(lines) and
            len(lines[line_num + 1]) > 0):
            parts[-1] = parts[-1] + "<br>"

    if in_p:
        parts.append("        </p>")

    return "\n".join(parts)

def generate_html(filename, lang, games):
    with open(filename, 'wt', encoding='utf-8') as f:
        games = [game for game in games if game not in lang.skip_games]

        print(HEADER.format(lang.help, lang.toc), file=f, end='')

        print("        <ul>", file=f)

        for game in games:
            print("          <li><a href=\"#{}\">{}</a>".
                  format(game,
                         get_game_name(game, lang)),
                  file=f)

        print("          <li><a href=\"#about\">{}</a>".format(lang.about),
              file=f)

        print("        </ul>", file=f)

        for game in games:
            print("\n        <h2><a id=\"{}\">{}</a></h2>\n\n{}".
                  format(game,
                         get_game_name(game, lang),
                         get_game_help(game, lang)),
                  file=f)

        if lang.about:
            print("\n        <h2><a id=\"about\">{}</a></h2>\n".
                  format(lang.about),
                  file=f)

            if lang.telegram_bot:
                print("          <p>{}</p>\n".format(lang.telegram_bot),
                      file=f)

            if lang.source_code:
                print("          <p>{}</p>\n".format(lang.source_code),
                      file=f)

        print(FOOTER, file=f, end='')


for lang in LANGUAGES:
    generate_html(lang.code + "/help.html", lang, GAMES)

# Wordparty is treated as a standalone game with its own help file
generate_html("vortofesto/help.html",
              next(lang for lang in LANGUAGES if lang.code == 'eo'),
              ['wordparty'])
