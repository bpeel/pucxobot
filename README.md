Ĉi tiu roboto estas por ludi kelkajn kartludojn ĉe Telegram.

Por kompili la programon, tajpu la jenon:

    mkdir build && cd build
    meson ..
    ninja

Nun vi devas krei agordodosieron por informi la roboton de ĝia ŝlosilo
de la API. Vi povas krei tian ŝlosilon per la roboto
[@BotFather](https://t.me/BotFather) ĉe telegram. Kreu dosieron ĉe
`~/.pucxobot/config.txt` kaj enmetu ion kiel la jenon:

    [bot]
    apikey = 123456789:AAABBEUAEUIEAIUE_auieauieauie
    botname = mojosabot
    language = eo

La lingvo povas esti `fr` aŭ `eo`. Vi povas krei plurajn robotojn per
nur unu agordodosiero se vi ripetas la sekcion `[bot]`. Tio estas
utila se vi volas robotojn en malsamaj lingvoj.

Sekve vi povas aldoni la roboton al iu ajn grupo (aŭ al pluraj grupoj
samtempe). Por krei partion de ludo, tajpu unu el la jenaj komandoj en
la grupo:

    /pucxo
    /amletero
    /perfidulo

Aŭ vi povas tajpi `/helpo` por vidi resumon de la reguloj.

Por agordi la roboton ĉe [@BotFather](https://t.me/BotFather), vi
povas uzi la jenajn priskribojn de komandoj:

    pucxo - Krei ludon de Puĉo
    perfidulo - Krei ludon de Perfidulo
    amletero - Krei ludon de Amletero
    aligxi - Aliĝi al jam kreita ludo
    komenci - Komenci jam kreitan ludon
    helpo - Montri resumon de la reguloj

Alikaze, se vi simple volas ludi kun jam ekzistanta roboto, vizitu
[ludoj.telegramo.org](https://ludoj.telegramo.org) aŭ invitu la
roboton [@pucxobot](https://t.me/pucxobot) al via propra grupo.
