foreach outfile, infiles : translatable_files
  input_files = [ '../translate-file.py', '..' / lang + '.language-file' ]
  args = [ python, '@INPUT0@', '-l', '@INPUT1@', '-o', '@OUTPUT0@' ]

  inum = 2

  foreach infile : infiles
    input_files += '..' / infile
    args += ['-i', '@INPUT' + inum.to_string() + '@']
    inum += 1
  endforeach

  custom_target(lang + '-' + outfile,
                input : input_files,
                output : outfile,
                command : args,                          
                install : true,
                install_dir : web_dir / lang)
endforeach
