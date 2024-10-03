# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_tstexe_vecadd_opencl = executable(
  'tstexe-vecadd-opencl',
  'vecadd.opencl.c',
  dependencies: [opencl_full_dep, math_dep],
)

_tst = find_program(files('tst-opencl-vecadd-produces-profiles'))
test(
  'Measurement of tstexe-vecadd-opencl produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_opencl, 'gpu=opencl', ''],
  suite: ['hpcrun', 'opencl'],
  depends: hpcrun_test_depends,
)
test(
  'Measurement of tstexe-vecadd-opencl with tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_opencl, 'gpu=opencl', '-t'],
  suite: ['hpcrun', 'opencl'],
  depends: hpcrun_test_depends,
)
test(
  'Measurement of tstexe-vecadd-opencl with boosted tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_opencl, 'gpu=opencl', '-tt'],
  suite: ['hpcrun', 'opencl'],
  depends: hpcrun_test_depends,
)
