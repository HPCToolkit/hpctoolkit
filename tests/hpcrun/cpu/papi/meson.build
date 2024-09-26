# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

if not papi_dep.found()
  subdir_done()
endif

papi_check = executable('papi_check', 'papi_check.c', dependencies: [papi_dep])

test(
  'PAPI profile exist',
  find_program(files('tst-papi-exist')),
  args: [papi_check, hpcrun, simple_tstexe],
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
)
