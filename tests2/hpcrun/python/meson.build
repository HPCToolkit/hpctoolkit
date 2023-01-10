test('simple-1thread.py', python3, args: [files('simple-1thread.py')])
test('hpcrun simple-1thread.py', python3, args: [files('run-simple.py'), files('simple-1thread.py')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

test('simple-mthread.py', python3, args: [files('simple-mthread.py')])
test('hpcrun simple-mthread.py', python3, args: [files('run-simple.py'), '-t', '3', files('simple-mthread.py')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails for multiple reasons:
  #  - The test cannot currently detect a side-thread callstack
  #  - The Python support does not work reliably for Python-spawned threads
  should_fail: true)

test('simple-exception.py', python3, args: [files('simple-exception.py')])
test('hpcrun simple-exception.py', python3, args: [files('run-simple.py'), files('simple-exception.py')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])

test('simple-signals.py', python3, args: [files('simple-signals.py')])
test('hpcrun simple-signals.py', python3, args: [files('run-simple.py'), files('simple-signals.py')],
  env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'],
  # FIXME: Fails due to how Python reports signals
  should_fail: true)
