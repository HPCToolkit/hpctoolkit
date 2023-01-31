exe_1loop = executable('test-1loop', files('1loop.cpp'), dependencies: dependency('openmp'))

_tst = find_program(files('tst-produces-profiles'))
test('Measurement of 1loop produces profiles', _tst, args: ['-t4', exe_1loop],
     env: hpctoolkit_pyenv, suite: 'hpcrun')
