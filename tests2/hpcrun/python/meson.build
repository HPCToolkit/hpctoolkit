_tst = find_program(files('tst-unwound-python'))
test('Measurement of simple-1thread unwound Python', _tst, args: ['simple', files('simple-1thread')],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])
test('Measurement of simple-mthread unwound Python', _tst, args: ['simple', '-t3', files('simple-mthread')],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
     # FIXME: Fails for multiple reasons:
     #  - The test cannot currently detect a side-thread callstack
     #  - The Python support does not work reliably for Python-spawned threads
     should_fail: true)
test('Measurement of simple-exception unwound Python', _tst, args: ['simple', files('simple-exception')],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])
test('Measurement of simple-signals unwound Python', _tst, args: ['simple', files('simple-signals')],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
     # FIXME: Fails due to how Python reports signals
     should_fail: true)
