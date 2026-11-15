# Copyright (C) 2026 Interpretica Unipessoal Lda
tests = [ 'prologue', 'epilogue',
          'render', 'cursor', 'attrs', 'expect', 'resize', 'wait',
          'conformance',
          'erase', 'scroll', 'edit', 'tabs', 'wrap', 'altscreen', 'unicode',
          'savecursor', 'color', 'cursorvis', 'keys', 'signals', 'scrollback',
          'rowattrs', 'regex', 'multisession' ]
foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach
install_data(['package.xml'], install_dir: package_dir)
