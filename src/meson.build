
translations = [
        'pcx-text-esperanto.c',
        'pcx-text-french.c',
        'pcx-text-english.c',
        'pcx-text-pt-br.c',
]

src = [
        'pcx-main.c',
        'pcx-log.c',
        'pcx-game.c',
        'pcx-util.c',
        'pcx-main-context.c',
        'pcx-error.c',
        'pcx-slice.c',
        'pcx-slab.c',
        'pcx-buffer.c',
        'pcx-list.c',
        'pcx-tty-game.c',
        'pcx-coup-character.c',
        'pcx-bot.c',
        'pcx-message-queue.c',
        'pcx-key-value.c',
        'pcx-coup-help.c',
        'pcx-curl-multi.c',
        'pcx-config.c',
        'pcx-text.c',
        'pcx-coup.c',
        'pcx-snitch.c',
        'pcx-snitch-help.c',
        'pcx-html.c',
        'pcx-love.c',
        'pcx-love-help.c',
        'pcx-superfight.c',
        'pcx-superfight-deck.c',
        'pcx-superfight-help.c',
        'pcx-zombie.c',
        'pcx-zombie-help.c',
]

src += translations

curl = dependency('libcurl', version: '>=7.16')
json = dependency('json-c')
thread_dep = dependency('threads')

executable('pucxobot', src, dependencies: [curl, json, thread_dep])

test_coup_src = [
        'pcx-util.c',
        'pcx-main-context.c',
        'pcx-error.c',
        'pcx-slice.c',
        'pcx-slab.c',
        'pcx-buffer.c',
        'pcx-list.c',
        'pcx-coup-character.c',
        'pcx-coup-help.c',
        'pcx-text.c',
        'pcx-coup.c',
        'test-coup.c',
]

test_coup_src += translations

test_coup = executable('test-coup', test_coup_src)
test('coup', test_coup)
