testdata_meas = {}

_run = [python3, files('run.py')]
_exe_small = executable('testmeas-small', files('small.c'),
    override_options: ['debug=true', 'optimization=0'])
_exe_loops = executable('testmeas-loops', files('loops.c'),
    override_options: ['debug=true', 'optimization=0'],
    dependencies: dependency('openmp'))
_exe_loops_cuda = executable('testmeas-loops-cuda', files('loops.cu'),
    dependencies: cuda_dep)
_exe_loops_hip = custom_target(output: 'testmeas-loops-hip', input: files('loops.hip.cpp'),
    command: hipcc_cmd)

_cuda_pc = [cuda_filter, '--capability=>6.0', '--random=1']

_measurements = {
  # `small` is designed to contain 3-4 samples scattered through a tiny, easy-to-understand
  # calling context tree. Good for debugging very bad problems.
  'small': [_run, '--min-samples=3', '--', '@OUTPUT@', '-e', 'cycles@1000000000', '-e', 'instructions@100000000000', '-t', _exe_small],

  # `loops-*` are designed to exercise more interesting cases with multiple nested loops and
  # simple multithreading. Various events/trace settings are tested to exercise minor differences
  # in the different metrics. Tuned to contain <300 samples total.
  # CPUTIME is currently disabled due to https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/646
  # 'loops-cputime': [_run, '--', '@OUTPUT@', '-e', 'CPUTIME', _exe_loops],
  # 'loops-cputime-t': [_run, '--', '@OUTPUT@', '-e', 'CPUTIME', '-t', _exe_loops],
  'loops-perf': [_run, '--', '@OUTPUT@', '-e', 'cycles@f30', '-e', 'instructions@120000000', _exe_loops],
  'loops-perf-t': [_run, '--', '@OUTPUT@', '-e', 'cycles@f30', '-e', 'instructions@120000000', '-t', _exe_loops],
  'loops-cuda-nvidia': [_run, '--', '@OUTPUT@', '-e', 'gpu=nvidia', _exe_loops_cuda],
  'loops-cuda-nvidia-t': [_run, '--', '@OUTPUT@', '-e', 'gpu=nvidia', _exe_loops_cuda],
  'loops-cuda-nvidiapc': [_cuda_pc, _run, '--', '@OUTPUT@', '-e', 'gpu=nvidia,pc', _exe_loops_cuda],
  'loops-cuda-nvidiapc-t': [_cuda_pc, _run, '--', '@OUTPUT@', '-e', 'gpu=nvidia,pc', '-t', _exe_loops_cuda],
  'loops-hip-amd': [_run, '--', '@OUTPUT@', '-e', 'gpu=amd', _exe_loops_hip],
  'loops-hip-amd-t': [_run, '--', '@OUTPUT@', '-e', 'gpu=amd', '-t', _exe_loops_hip],
}
_xfails = []

foreach name, cmd : _measurements
  _out = name+'.tar.xz'
  _meas = {
    'cached': files(_out),
    'xfail': name in _xfails,
  }

  _gen = custom_target(output: _out, command: cmd, env: hpctoolkit_pyenv, console: true,
      build_always_stale: true, build_by_default: false)
  if not is_disabler(_gen)
    _meas += {'gen_target': _gen}
  endif

  testdata_meas += {name: _meas}
endforeach
