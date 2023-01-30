test('simple-1thread', python3, args: [files('simple-1thread')])
test('hpcrun simple-1thread', python3, args: [files('run-simple'), files('simple-1thread')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

test('simple-mthread', python3, args: [files('simple-mthread')])
test('hpcrun simple-mthread', python3, args: [files('run-simple'), '-t', '3', files('simple-mthread')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails for multiple reasons:
  #  - The test cannot currently detect a side-thread callstack
  #  - The Python support does not work reliably for Python-spawned threads
  should_fail: true)

test('simple-exception', python3, args: [files('simple-exception')])
test('hpcrun simple-exception', python3, args: [files('run-simple'), files('simple-exception')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

test('simple-signals', python3, args: [files('simple-signals')])
test('hpcrun simple-signals', python3, args: [files('run-simple'), files('simple-signals')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails due to how Python reports signals
  should_fail: true)
