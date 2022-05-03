foreach outfile, infiles : translatable_files
  input_files = [ '../translate-file.py',
                  '..' / 'eo.language-file',
                  'vortofesto.language-file' ]
  args = [ python, '@INPUT0@',
           '-l', '@INPUT1@', '-l', '@INPUT2@',
           '-o', '@OUTPUT0@' ]

  inum = 3

  foreach infile : infiles
    input_files += '..' / infile
    args += ['-i', '@INPUT' + inum.to_string() + '@']
    inum += 1
  endforeach

  custom_target('vortofesto-' + outfile,
                input : input_files,
                output : outfile,
                command : args,                          
                install : true,
                install_dir : web_dir / 'vortofesto')
endforeach
