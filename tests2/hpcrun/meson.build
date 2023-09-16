_args = []
if has_rocm
  _args += ['--rocprofiler-path', rocm_profiler_exdep.get_variable(internal: 'prefix')]
endif
test('Listing metrics does not error', hpcrun, args: ['-L'] + _args, suite: 'hpcrun')

subdir('cpu')
subdir('python')
subdir('gpu/cuda')
subdir('gpu/hip')
