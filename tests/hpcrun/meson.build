_args = []
if rocm_dep.found()
  assert(not is_disabler(rocprofiler_path))
  _args += ['--rocprofiler-path', rocprofiler_path]
endif
test(
  'Listing metrics does not error',
  hpcrun,
  args: ['-L'] + _args,
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

subdir('cpu')
subdir('python')
subdir('gpu/cuda')
subdir('gpu/hip')
