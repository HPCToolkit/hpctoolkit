_run_simple = find_program(files('run-simple'))

_simple_1thread = find_program(files('simple-1thread'))
test('simple-1thread', _simple_1thread)
test('hpcrun simple-1thread', _run_simple, args: [files('simple-1thread')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

_simple_mthread = find_program(files('simple-mthread'))
test('simple-mthread', _simple_mthread)
test('hpcrun simple-mthread', _run_simple, args: ['-t', '3', files('simple-mthread')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails for multiple reasons:
  #  - The test cannot currently detect a side-thread callstack
  #  - The Python support does not work reliably for Python-spawned threads
  should_fail: true)

_simple_exception = find_program(files('simple-exception'))
test('simple-exception', _simple_exception)
test('hpcrun simple-exception', _run_simple, args: [files('simple-exception')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

_simple_signals = find_program(files('simple-signals'))
test('simple-signals', _simple_signals)
test('hpcrun simple-signals', _run_simple, args: [files('simple-signals')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails due to how Python reports signals
  should_fail: true)
