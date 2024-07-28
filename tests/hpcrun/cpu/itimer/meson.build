# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

test(
  'CPUTIME event measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-cputime-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
test(
  'CPUTIME event measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-cputime-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)

test(
  'REALTIME event measures on @0@'.format(simple_tstexe.name()),
  find_program(files('tst-realtime-produces-profiles')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
test(
  'REALTIME event measures on @0@ w/o auditor'.format(simple_tstexe.name()),
  find_program(files('tst-realtime-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
