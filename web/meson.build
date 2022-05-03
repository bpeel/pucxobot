translatable_files = {
        'index.html': ['index.html'],
        'pucxobot.js': ['message-reader.js',
                        'wordparty-visualisation.js',
                        'pucxo.js'],
}

languages = [
        'eo',
        'en',
        'fr',
]

data_files = [
        'pucxobot.css',
        'send.png',
        'background.png',
        'help.png',
        'wrong.mp3',
        'correct.mp3',
        'loselife.mp3',
        'gainlife.mp3',
]

web_dir = get_option('datadir') / 'web'

install_data(sources : data_files, install_dir : web_dir)
             
install_data('htaccess', install_dir : web_dir, rename : '.htaccess')

python = find_program('python3')

foreach lang : languages
  install_data(lang / 'help.html', install_dir : web_dir / lang)
  subdir(lang)
endforeach

subdir('vortofesto')
