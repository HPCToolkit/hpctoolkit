tstexe_1loop = executable('tstexe-1loop', files('1loop.cpp'), dependencies: dependency('openmp'))

_tst = configure_file(input: files('tst-produces-profiles'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
test('Measurement of tstexe-1loop produces profiles',
     _tst, args: ['-t4', tstexe_1loop],
     env: hpctoolkit_pyenv, suite: 'hpcrun')
