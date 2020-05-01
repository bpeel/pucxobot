translatable_files = [
        'index.html',
        'pucxobot.js',
]

languages = [
        'eo',
        'en',
]

data_files = [
        'pucxobot.css',
        'send.png',
        'background.png',
        'help.png',
]

web_dir = get_option('datadir') / 'web'

install_data(sources : data_files, install_dir : web_dir)
             
install_data('htaccess', install_dir : web_dir, rename : '.htaccess')

perl = find_program('perl')

foreach lang : languages
  install_data(lang / 'help.html', install_dir : web_dir / lang)
  subdir(lang)
endforeach
