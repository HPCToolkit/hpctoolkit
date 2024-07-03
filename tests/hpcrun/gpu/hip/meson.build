# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

if not rocm_dep.found() or not has_hip
  subdir_done()
endif

_tstexe_vecadd_hip = custom_target(
  output: 'tstexe-vecadd-hip',
  command: hip_ld,
  input: hip.process(files('vecadd.hip.cpp')),
)

_preargs = ['--rocprofiler-path', rocprofiler_path]

_tst = find_program(files('tst-amd-vecadd-produces-profiles'))
test(
  'Measurement of tstexe-vecadd-hip produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, ''] + _preargs,
  suite: ['hpcrun', 'rocm'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-hip with tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, '-t'] + _preargs,
  suite: ['hpcrun', 'rocm'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-hip with boosted tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, '-tt'] + _preargs,
  suite: ['hpcrun', 'rocm'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
