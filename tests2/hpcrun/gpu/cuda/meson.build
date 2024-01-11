if not has_cuda
  subdir_done()
endif

_tstexe_vecadd_cuda = executable('tstexe-vecadd-cuda', files('vecadd.cu'))

_tst = find_program(files('tst-cuda-vecadd-produces-profiles'))
test(
  'Measurement of tstexe-vecadd-cuda produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-cuda w/ tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia', '-t'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-cuda w/ boosted tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia', '-tt'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of tstexe-vecadd-cuda w/ PC sampling produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia,pc'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
  is_parallel: false,  # CUPTI does not allow multiple processes to sample the GPU
  priority: 100,  # High priority to optimize for lack of parallelism
)
test(
  'Measurement of tstexe-vecadd-cuda w/ PC + tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia,pc', '-t'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
  is_parallel: false,  # CUPTI does not allow multiple processes to sample the GPU
  priority: 100,  # High priority to optimize for lack of parallelism
)
test(
  'Measurement of tstexe-vecadd-cuda w/ PC + boosted tracing produces profiles',
  _tst,
  args: [hpctesttool, hpcrun, _tstexe_vecadd_cuda, 'gpu=nvidia,pc', '-tt'],
  suite: ['hpcrun', 'cuda'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
  is_parallel: false,  # CUPTI does not allow multiple processes to sample the GPU
  priority: 100,  # High priority to optimize for lack of parallelism
)
