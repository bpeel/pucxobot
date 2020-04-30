foreach file : translatable_files
  custom_target(lang + '-' + file,
                input : [ '../translate-file.pl',
                          '..' / lang + '.language-file',
                          '..' / file ],
                output : file,
                command : [ perl, '@INPUT0@',
                            '-l', '@INPUT1@',
                            '-i', '@INPUT2@',
                            '-o', '@OUTPUT0@' ],
                install : true,
                install_dir : web_dir / lang)
endforeach
