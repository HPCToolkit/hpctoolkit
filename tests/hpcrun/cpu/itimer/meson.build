# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

test(
  'CPUTIME event measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-cputime-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'CPUTIME event measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-cputime-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

test(
  'REALTIME event measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-realtime-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'REALTIME event measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-realtime-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
