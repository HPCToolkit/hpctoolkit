# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

test(
  'Perf frequency on group of events',
  find_program(files('tst-perf-freq-group')),
  args: [hpcproftt, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

test(
  'Perf period on group of events',
  find_program(files('tst-perf-period-group')),
  args: [hpcproftt, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

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
