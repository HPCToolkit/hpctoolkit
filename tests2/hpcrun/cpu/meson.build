exe_1loop = executable('test-1loop', files('1loop.cpp'),
  dependencies: dependency('openmp'))
test('1loop', exe_1loop)
test('hpcrun 1loop', python3, args: [files('run-check.py'), '-t4', exe_1loop],
  env: hpctoolkit_pyenv, suite: 'hpcrun')

if get_option('python')
  test('simple.py', python3, args: [files('simple.py')])
  test('hpcrun simple.py', python3, args: [files('run-simple.py'), files('simple.py')],
    env: hpctoolkit_pyenv, suite: ['hpcrun', 'python'])
endif
