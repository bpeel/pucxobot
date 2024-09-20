Pucxobot is a Telegram robot and website to play the following games:

* Coup
* Love Letter
* One Night Werewolf
* 6 Takes!
* Fox in the Forest
* Vortofesto
* Chameleon
* Zombie Dice
* Snitch
* Superfight

If you just want to test it out on Telegram you can join
[this group](https://t.me/bluffing). Otherwise you can add
[@bluffingbot](https://t.me/bluffingbot) to your own group and play.

To try the website, go [here](https://gemelo.org/ludoj/en).

## Building

To build the server, you first need to install libcurl, json-c,
openssl and meson. On Fedora you can install them with the following
command:

    sudo dnf install json-c-devel libcurl-devel openssl-devel meson ninja-build

On Debian you can install them with the following command:

    sudo apt install libjson-c-dev libcurl4-openssl-dev libssl-dev \
             pkg-config meson ninja-build

Next, to build the project type the following:

    meson build
    ninja -C build

Before running it, you need to create a configuration file in
`~/.pucxobot/conf.txt`. You can either run the server as the Telegram
bot, the website or both at the same time.

## Telegram Bot

To run the program as a Telegram bot you need to get an API key by
talking to [@BotFather](https://t.me/BotFather) on Telegram. You can
enter it into the `conf.txt` with something like this:

    [bot]
    apikey = 123456789:AAABBEUAEUIEAIUE_auieauieauie
    botname = cardgamebot
    language = en

The language can be `en`, `fr`, `pt-br`, `zh-tw` or `eo`. You can
create several bots with one configuration file by repeating the
`[bot]` section. This is useful if you want to have multiple bots in
different languages.

To play a game, you can add the bot to a group and type `/join`. It
will present you with a choice of the available games to play.

You can also type `/help` to get a summary of the rules of the game.

## Website

To run the program as a website add something like this to the `conf.txt`:

    [server]
    address = 3648

You can change the port number or the listen address or leave the
`address` line out entirely to use the default port.

The server is just to run the WebSocket back-end and you will still
need an actual web server to serve the HTML and JavaScript files. The
files for the site are in the `web` directory. They are first filtered
through a script to generate the different translations. If you run
`ninja install` you can find all the web files ready in
`<prefix>/share/web`. The server can handle multiple languages
simultaneously so there’s no need to configure the language.

## HTTPS

If the webserver that serves the web pages is using HTTPS then
Pucxobot also needs to use TLS to handle the WebSockets or the browser
will refuse to connect to a mix of secure and insecure resources and
the site will not work. You can tell Pucxobot to use TLS by specifying
files for the certificate and private keys with the `certificate` and
`private_key` options in the `[server]` section. The default port for
the server with TLS is different from the one without. Therefore you
can make Pucxobot accept both secure and insecure connections with a
configuration like this:

    [server]

    [server]

    certificate = /path/to/cert.pem
    private_key = /path/to/key.pem

The first server section specifies the default unencrypted server on
port 3648 and the second one makes it additionally listen for
encrypted websockets on port 3649.

## Daemonize

If you pass `-d` to the program it will detach from the terminal and
run as a daemon. By default, Pucxobot prints log messages to the
terminal. However if it is run as a daemon it will instead log to
`~/.pucxobot/log.txt`. You can override this data file directory and
the log file by adding an extra section to the config like this:

    [general]
    data_dir = /var/run/puxcobot-data
    log_file = /var/log/my-super-log-file

The program needs write access to the data directory in order to store
its state.

## Vortofesto data

In order for the Vortofesto game to work, the program needs to have access to word and syllable lists in the data directory. These are stored in a binary format so that they can be accessed more efficiently. The git repo includes a script to extract a word list from the XML files of the [Reta Vortaro](https://reta-vortaro.de/). Here are the commands needed to create the dictionary. If you want to update the dictionary later, you can rerun the commands without having to restart the server. It will pick up the newer version of the dictionary as soon as a new game is started when no other games are already running.

### Clone the ReVo data

```bash
git clone --depth 1 https://github.com/revuloj/voko-grundo.git
git clone --depth 1 https://github.com/revuloj/revo-fonto.git
```

The XML files in `revo-fonto` refer to the DTD files in `voko-grundo` as if they were in the same repo so we need to set up a symbolic link to get that to work:

```bash
ln -s ../voko-grundo/dtd revo-fonto/dtd
```

### Create the dictionary

```bash
./src/vortlisto.py revo-fonto/revo | \
 ./build/src/make-dictionary dictionary-eo.bin
# Separate step to replace the file atomically in case the server
# is running
mv dictionary-eo.bin ~/.pucxobot
```

### Create the syllabary

```bash
./build/src/make-syllables ~/.pucxobot/dictionary-eo.bin syllabary-eo.bin
mv syllabary-eo.bin ~/.pucxobot
```

### Test the dictionary

If you want to you can test that the dictionary data structure is working for the current word list with the following commands:

```bash
./src/vortlisto.py revo-fonto/revo > wordlist.txt
./src/test-dictionary.py \
 ./build/src/test-dictionary \
 ./build/src/make-dictionary \
 wordlist.txt
```

### Look at the syllables

If you are curious about what syllables are in the data, you can run the following command to see thim:

```bash
./build/src/dump-syllabary ~/.pucxobot/syllabary-eo.bin | less
```

The first number in each line is the cumulative number of times that syllable was found in the word list. The second number is just the difference with the previous line. If you divide the second number by the first number on the last line you will get the probability that this syllable will be chosen.

## Chameleon data

The Chameleon game needs to have a list of cards to work and this is not provided in the git repo so you’ll have to create it yourself. There is a different file for each language. For example, the file for playing in English is stored at:

```
~/.pucxobot/chameleon-word-list-en.txt
```

The list is just a simple text file with a list of words for each card. The cards are separated by a blank line. The first line of each card is the topic. You can add an extra blank line between the topic and the words if you want. A card can have as many or as few cards as you want but if you add more than 16 the website won’t display it properly. You can also add comments preceded with a `#` character. Here is an example:

```
Colours

Green
Violet
Red
Blue
Orange
Cyan
Magenta
Yellow
Ingigo
Brown
Purple
Black
Grey
White
Pink
Mauve

Books

Twenty Thousand Leagues Under the Seas
The bible
Harry Potter and the Philosopher’s Stone
Flatland
The Stainless Steel Rat
It
The Girl with the Dragon Tattoo
```

## Superfight data

The Superfight game needs two lists to work, one for the roles and one for the attributes. These aren’t in the git repo so you need to write them yourself. There is a separate set of lists for each language. For example the files for a game in English are expected to be at:

```
~/.pucxobot/superfight-roles-en.txt
~/.pucxobot/superfight-attributes-en.txt
```

Each file is just a list with one line per entry. Blank lines and lines beginning with `#` are ignored.
