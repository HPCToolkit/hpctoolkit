tstexe_1loop = executable('tstexe-1loop', files('1loop.cpp'), dependencies: dependency('openmp'))

test('Measurement of tstexe-1loop produces profiles',
    find_program(files('tst-1loop-produces-profiles')), args: [hpctesttool, hpcrun, tstexe_1loop],
    suite: 'hpcrun',)
