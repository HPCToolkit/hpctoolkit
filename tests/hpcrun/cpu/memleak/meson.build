# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_example_tstexe = executable('tstexe-memleak-example', 'example.c', dependencies: threads_dep)

test(
  'Memleak event measures on @0@'.format(_example_tstexe.name()),
  find_program(files('tst-memleak-produces-profiles')),
  args: [hpctesttool, hpcrun, _example_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Memleak event measures on @0@ w/o auditor'.format(_example_tstexe.name()),
  find_program(files('tst-memleak-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, _example_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
