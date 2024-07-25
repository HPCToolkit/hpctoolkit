# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_example_tstexe = executable('tstexe-pthread-blame-example', 'example.c', dependencies: threads_dep)

test(
  'Pthread-blame event measures on @0@'.format(_example_tstexe.name()),
  find_program(files('tst-pthread-blame-produces-profiles')),
  args: [hpctesttool, hpcrun, _example_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
test(
  'Pthread-blame event measures on @0@ w/o auditor'.format(_example_tstexe.name()),
  find_program(files('tst-pthread-blame-produces-profiles-noaudit')),
  args: [hpctesttool, hpcrun, _example_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
