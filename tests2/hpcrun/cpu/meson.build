_run_check = find_program(files('run-check'))

exe_1loop = executable('test-1loop', files('1loop.cpp'),
  dependencies: dependency('openmp'))
test('1loop', exe_1loop)
test('hpcrun 1loop', _run_check, args: ['-t4', exe_1loop],
  env: hpctoolkit_pyenv, suite: 'hpcrun')
