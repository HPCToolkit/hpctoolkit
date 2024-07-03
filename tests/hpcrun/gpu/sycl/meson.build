# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

if not level0_dep.found() or not has_sycl
  subdir_done()
endif

if sycl_is_cpp
  _tstexe_vecadd_sycl = executable('tstexe-vecadd-sycl', 'vecadd.sycl.cpp', dependencies: sycl_dep)
else
  _tstexe_vecadd_sycl = custom_target(
    output: 'tstexe-vecadd-sycl',
    command: sycl_ld,
    input: sycl.process(files('vecadd.sycl.cpp')),
  )
endif

# FIXME: We currently assume SYCL always goes to Level Zero, but there are other SYCL
# implementations available targeting OpenCL, NVIDIA CUDA or AMD ROCm. These tests
# will likely fail for those other implementations, in possibly mysterious ways. Caveat emptor.

_tst = find_program(files('tst-level0-vecadd-produces-profiles'))
test(
  'Measurement of tstexe-vecadd-sycl produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_sycl, 'gpu=level0', ''],
  suite: ['hpcrun', 'level0'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-sycl with tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_sycl, 'gpu=level0', '-t'],
  suite: ['hpcrun', 'level0'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-sycl with boosted tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_sycl, 'gpu=level0', '-tt'],
  suite: ['hpcrun', 'level0'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

if gtpin_dep.found()

  # FIXME: This test is commented out because GTPin has very stringent hardware requirements and
  # for now it's unable to be predicted.
  # test(
  #   'Measurement of tstexe-vecadd-sycl with instruction counting produces profiles',
  #   _tst,
  #   args: [hpctesttool, hpcrun, _tstexe_vecadd_sycl, 'gpu=level0,inst=count', ''],
  #   suite: ['hpcrun', 'level0'],
  #   depends: hpcrun_test_depends,
  #   env: hpcrun_test_env,
  # )

endif
