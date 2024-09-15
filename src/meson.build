
translations = [
        'pcx-text-esperanto.c',
        'pcx-text-french.c',
        'pcx-text-english.c',
        'pcx-text-pt-br.c',
        'pcx-text-chinese-traditional.c',
]

server_src = [
        'pcx-log.c',
        'pcx-game.c',
        'pcx-util.c',
        'pcx-error.c',
        'pcx-slice.c',
        'pcx-slab.c',
        'pcx-buffer.c',
        'pcx-list.c',
        'pcx-coup-character.c',
        'pcx-server.c',
        'pcx-player.c',
        'pcx-playerbase.c',
        'pcx-conversation.c',
        'pcx-proto.c',
        'pcx-base64.c',
        'pcx-utf8.c',
        'pcx-class-store.c',
        'pcx-ws-parser.c',
        'pcx-connection.c',
        'pcx-netaddress.c',
        'pcx-generate-id.c',
        'pcx-socket.c',
        'pcx-listen-socket.c',
        'pcx-file-error.c',
        'pcx-key-value.c',
        'pcx-coup-help.c',
        'pcx-config.c',
        'pcx-text.c',
        'pcx-ssl-error.c',
        'pcx-coup.c',
        'pcx-snitch.c',
        'pcx-snitch-help.c',
        'pcx-html.c',
        'pcx-hat.c',
        'pcx-dictionary.c',
        'pcx-syllabary.c',
        'pcx-trie.c',
        'pcx-love.c',
        'pcx-love-help.c',
        'pcx-six.c',
        'pcx-six-help.c',
        'pcx-fox.c',
        'pcx-fox-help.c',
        'pcx-superfight.c',
        'pcx-superfight-deck.c',
        'pcx-superfight-help.c',
        'pcx-wordparty.c',
        'pcx-wordparty-help.c',
        'pcx-chameleon.c',
        'pcx-chameleon-help.c',
        'pcx-chameleon-list.c',
        'pcx-zombie.c',
        'pcx-zombie-help.c',
        'sha1.c',
]

server_src += translations

src = [
        'pcx-main.c',
        'pcx-main-context.c',
        'pcx-bot.c',
        'pcx-message-queue.c',
        'pcx-curl-multi.c',
] + server_src

curl = dependency('libcurl', version: '>=7.16')
json = dependency('json-c')
openssl = dependency('openssl')
thread_dep = dependency('threads')

pucxobot = executable('pucxobot', src,
                      dependencies: [curl, json, thread_dep, openssl],
                      include_directories: configinc,
                      install: true)

test_coup_src = [
        'pcx-util.c',
        'pcx-main-context.c',
        'pcx-class-store.c',
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
        'pcx-class-store.c',
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

test_utf8_src = [
        'pcx-utf8.c',
        'test-utf8.c',
]

test_utf8 = executable('test-utf8', test_utf8_src,
                       include_directories: configinc)
test('utf8', test_utf8)

make_dictionary_src = [
        'pcx-buffer.c',
        'pcx-slab.c',
        'pcx-util.c',
        'pcx-utf8.c',
        'make-dictionary.c',
]
make_dictionary = executable('make-dictionary', make_dictionary_src,
                             include_directories: configinc)

make_syllables_src = [
        'pcx-buffer.c',
        'pcx-error.c',
        'pcx-file-error.c',
        'pcx-dictionary.c',
        'pcx-utf8.c',
        'pcx-util.c',
        'make-syllables.c',
]
make_syllables = executable('make-syllables', make_syllables_src,
                            include_directories: configinc)

dump_syllabary_src = [
        'dump-syllabary.c',
]
dump_syllabary = executable('dump-syllabary', dump_syllabary_src,
                            include_directories: configinc)

test_dictionary_src = [
        'pcx-buffer.c',
        'pcx-error.c',
        'pcx-file-error.c',
        'pcx-dictionary.c',
        'pcx-utf8.c',
        'pcx-util.c',
        'test-dictionary.c',
]
test_dictionary = executable('test-dictionary', test_dictionary_src,
                             include_directories: configinc)

test('dictionary', files('test-dictionary.py'),
     args : [ test_dictionary, make_dictionary ])

test_trie_src = [
        'pcx-buffer.c',
        'pcx-slab.c',
        'pcx-trie.c',
        'pcx-utf8.c',
        'pcx-util.c',
        'test-trie.c',
]
test_trie = executable('test-trie', test_trie_src,
                       include_directories: configinc)
test('trie', test_trie)

test_wordparty_src = [
        'test-wordparty.c',
        'test-time-hack.c',
] + server_src
test_wordparty = executable('test-wordparty', test_wordparty_src,
                            dependencies: [thread_dep, openssl],
                            include_directories: configinc)
test('wordparty', test_wordparty)

test_chameleon_list_src = [
        'test-chameleon-list.c',
        'pcx-buffer.c',
        'pcx-chameleon-list.c',
        'pcx-error.c',
        'pcx-file-error.c',
        'pcx-list.c',
        'pcx-slab.c',
        'pcx-slice.c',
        'pcx-utf8.c',
        'pcx-util.c',
]
test_chameleon_list = executable('test-chameleon-list', test_chameleon_list_src,
                                 include_directories: configinc)
test('chameleon-list', test_chameleon_list)

test_chameleon_src = [
        'pcx-buffer.c',
        'pcx-chameleon.c',
        'pcx-chameleon-help.c',
        'pcx-chameleon-list.c',
        'pcx-class-store.c',
        'pcx-config.c',
        'pcx-error.c',
        'pcx-file-error.c',
        'pcx-html.c',
        'pcx-key-value.c',
        'pcx-list.c',
        'pcx-log.c',
        'pcx-slab.c',
        'pcx-slice.c',
        'pcx-text.c',
        'pcx-utf8.c',
        'pcx-util.c',
        'test-message.c',
        'test-time-hack.c',
        'test-chameleon.c',
]

test_chameleon_src += translations

test_chameleon = executable('test-chameleon', test_chameleon_src,
                            include_directories: configinc,
                            dependencies: [thread_dep])
test('chameleon', test_chameleon)

fake_telegram_src = [
        'fake-telegram.c',
        'pcx-main-context.c',
        'pcx-util.c',
        'pcx-list.c',
        'pcx-slice.c',
        'pcx-buffer.c',
        'pcx-slab.c',
        'pcx-listen-socket.c',
        'pcx-netaddress.c',
        'pcx-error.c',
        'pcx-file-error.c',
        'pcx-socket.c',
]

fake_telegram = executable('fake-telegram', fake_telegram_src,
                           dependencies: [json],
                           include_directories: configinc)

test('bot', files('test-bot.py'),
     args : [ fake_telegram, pucxobot, files('test-bot.script') ])
