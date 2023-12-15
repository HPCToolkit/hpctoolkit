tstexe_1loop = executable('tstexe-1loop', files('1loop.cpp'), dependencies: dependency('openmp'))

test(
  'Measurement of tstexe-1loop produces profiles',
  find_program(files('tst-1loop-produces-profiles')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)
test(
  'Measurement of tstexe-1loop produces profiles w/o auditor',
  find_program(files('tst-1loop-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)

test(
  'CPUTIME event measures on tstexe-1loop',
  find_program(files('tst-1loop-cputime')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)
test(
  'CPUTIME event measures on tstexe-1loop w/o auditor',
  find_program(files('tst-1loop-cputime-noaudit')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)

test(
  'REALTIME event measures on tstexe-1loop',
  find_program(files('tst-1loop-realtime')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)
test(
  'REALTIME event measures on tstexe-1loop w/o auditor',
  find_program(files('tst-1loop-realtime-noaudit')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)

test(
  'Perf events measure on tstexe-1loop',
  find_program(files('tst-1loop-perf')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)
test(
  'Perf events measure on tstexe-1loop w/o auditor',
  find_program(files('tst-1loop-perf-noaudit')),
  args: [hpctesttool, hpcrun, tstexe_1loop],
  suite: 'hpcrun',
)
