
translations = [
        'pcx-text-esperanto.c',
        'pcx-text-french.c',
        'pcx-text-english.c',
        'pcx-text-pt-br.c',
        'pcx-text-chinese-traditional.c',
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
        'pcx-server.c',
        'pcx-player.c',
        'pcx-playerbase.c',
        'pcx-conversation.c',
        'pcx-proto.c',
        'pcx-base64.c',
        'pcx-ws-parser.c',
        'pcx-connection.c',
        'pcx-netaddress.c',
        'pcx-generate-id.c',
        'pcx-socket.c',
        'pcx-file-error.c',
        'pcx-message-queue.c',
        'pcx-key-value.c',
        'pcx-coup-help.c',
        'pcx-curl-multi.c',
        'pcx-config.c',
        'pcx-text.c',
        'pcx-ssl-error.c',
        'pcx-coup.c',
        'pcx-snitch.c',
        'pcx-snitch-help.c',
        'pcx-html.c',
        'pcx-love.c',
        'pcx-love-help.c',
        'pcx-six.c',
        'pcx-six-help.c',
        'pcx-fox.c',
        'pcx-fox-help.c',
        'pcx-superfight.c',
        'pcx-superfight-deck.c',
        'pcx-superfight-help.c',
        'pcx-zombie.c',
        'pcx-zombie-help.c',
        'sha1.c',
]

src += translations

curl = dependency('libcurl', version: '>=7.16')
json = dependency('json-c')
openssl = dependency('openssl')
thread_dep = dependency('threads')

executable('pucxobot', src,
           dependencies: [curl, json, thread_dep, openssl],
           include_directories: configinc,
           install: true)

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
        'test-message.c',
        'test-coup.c',
]

test_coup_src += translations

test_coup = executable('test-coup', test_coup_src,
                       include_directories: configinc)
test('coup', test_coup)

test_fox_src = [
        'pcx-util.c',
        'pcx-main-context.c',
        'pcx-error.c',
        'pcx-slice.c',
        'pcx-slab.c',
        'pcx-buffer.c',
        'pcx-list.c',
        'pcx-text.c',
        'pcx-fox.c',
        'pcx-fox-help.c',
        'test-message.c',
        'test-fox.c',
]

test_fox_src += translations

test_fox = executable('test-fox', test_fox_src,
                       include_directories: configinc)
test('fox', test_fox)
