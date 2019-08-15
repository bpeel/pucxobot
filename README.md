Pucxobot is a Telegram robot to play the card games Coup, Snitch and
Love Letter.

To build the bot, type the following:

    mkdir build && cd build
    meson ..
    ninja

Before running it, you need to create a configuration file in
`~/.pucxobot/config.txt`. This will contain the API key for the bot
which you can create by talking to
[@BotFather](https://t.me/BotFather) on Telegram. It should look
something like this:

    [bot]
    apikey = 123456789:AAABBEUAEUIEAIUE_auieauieauie
    botname = cardgamebot
    language = en

The language can be `en`, `fr`, `pt-br` or `eo`. You can create
several bots with one configuration file by repeating the `[bot]`
section. That is useful if you want to have bots in different
languages.

To play a game, you can add the bot to a group and type `/join`. It
will present you with a choice of the available games to play.

You can also type `/help` to get a summary of the rules of the game.

When you configure the bot with [@BotFather](https://t.me/BotFather),
you can tell it what commands the bot can handle. Here is a suggestion
of what to type there:

    coup - Create a game of Coup
    snitch - Create a game of Snitch
    letter - Create a game of Love Letter
    join - Join an existing game or create a new one
    start - Start an existing game
    help - Show a summary of the rules

If you just want to try the bot, you can join
[this channel](https://t.me/bluffing). Otherwise you can add
[@bluffingbot](https://t.me/bluffingbot) to your own group and play.
