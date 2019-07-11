project('pucxobot', 'c')

cc = meson.get_compiler('c')

foreach a : ['-Wall', '-Wuninitialized', '-Wempty-body', '-Wformat',
      '-Wformat-security', '-Winit-self', '-Wundef',
      '-Wvla', '-Wpointer-arith', '-Wmissing-declarations']
  if cc.has_argument(a)
    add_project_arguments(a, language : ['c'])
  endif
endforeach

src = [
        'src/pcx-main.c',
        'src/pcx-game.c',
        'src/pcx-util.c',
        'src/pcx-main-context.c',
        'src/pcx-error.c',
        'src/pcx-slice.c',
        'src/pcx-slab.c',
        'src/pcx-buffer.c',
        'src/pcx-list.c',
        'src/pcx-tty-game.c',
        'src/pcx-coup-character.c',
        'src/pcx-bot.c',
        'src/pcx-key-value.c',
        'src/pcx-coup-help.c',
        'src/pcx-curl-multi.c',
        'src/pcx-config.c',
        'src/pcx-text-esperanto.c',
        'src/pcx-text-french.c',
        'src/pcx-text-english.c',
        'src/pcx-text.c',
        'src/pcx-coup.c',
        'src/pcx-snitch.c',
        'src/pcx-snitch-help.c',
        'src/pcx-html.c',
        'src/pcx-love.c',
        'src/pcx-love-help.c',
]

curl = dependency('libcurl', version: '>=7.16')
json = dependency('json-c')

executable('pucxobot', src, dependencies: [curl, json])
