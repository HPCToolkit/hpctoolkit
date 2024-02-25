test(
  'Perf period sampling measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-perf-period-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Perf period sampling measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-perf-period-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

test(
  'Perf frequency sampling measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-perf-freq-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Perf frequency sampling measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-perf-freq-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
