Pucxobot is a Telegram robot and website to play the following games:

* Coup
* Love Letter
* 6 Takes!
* Zombie Dice
* Snitch
* Superfight

If you just want to test it out on Telegram you can join
[this group](https://t.me/bluffing). Otherwise you can add
[@bluffingbot](https://t.me/bluffingbot) to your own group and play.

To try the website, go [here](http://gemelo.org/ludoj/en).

## Building

To build the server, you first need to install libcurl, json-c and
meson. On Fedora you can install them with the following command:

    sudo dnf install json-c-devel libcurl-devel meson ninja-build

On Debian you can install them with the following command:

    sudo apt install libjson-c-dev libcurl4-openssl-dev pkg-config meson ninja-build

Next, to build the project type the following:

    mkdir build && cd build
    meson ..
    ninja

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

The language can be `en`, `fr`, `pt-br` or `eo`. You can create
several bots with one configuration file by repeating the `[bot]`
section. This is useful if you want to have multiple bots in different
languages.

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
