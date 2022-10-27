exe_1loop = executable('test-1loop', files('1loop.cpp'),
  dependencies: dependency('openmp'))
test('1loop', exe_1loop)
test('hpcrun 1loop', python3, args: [files('1loop.py'), exe_1loop],
  env: hpctoolkit_pyenv, suite: 'hpcrun')
