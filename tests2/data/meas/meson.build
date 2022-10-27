testdata_meas = {}

_run = [python3, files('run.py')]
_exe_small = executable('test-small', files('small.c'),
    override_options: ['c_args=', 'debug=true', 'optimization=0'])

_measurements = {
  'small': [_run, '--min-samples=3', '--', '@OUTPUT@', '-e', 'cycles@1000000000', '-e', 'instructions@100000000000', '-t', _exe_small],
  # '1loop-cputime': [_run, '--', '@OUTPUT@', '-e', 'CPUTIME', exe_1loop],
  # '1loop-cputime-t': [_run, '--', '@OUTPUT@', '-e', 'CPUTIME', '-t', exe_1loop],
  '1loop-perf': [_run, '--', '@OUTPUT@', '-e', 'cycles@f40000', '-e', 'instructions@500000', exe_1loop],
  '1loop-perf-t': [_run, '--', '@OUTPUT@', '-e', 'cycles@f40000', '-e', 'instructions@500000', '-t', exe_1loop],
  # 'vecadd-cuda-nvidia':
  #   has_cuda ? [_run, '--', '@OUTPUT@', '-e', 'gpu=nvidia', '-e', 'REALTIME', exe_vecadd_cuda] : [],
  # 'vecadd-cuda-nvidiapc':
  #   has_cuda ? [_run, '--', '@OUTPUT@', '-e', 'gpu=nvidia,pc', '-e', 'REALTIME', exe_vecadd_cuda] : [],
  # 'vecadd-hip-amd':
  #   has_hip ? [_run, '--', '@OUTPUT@', '-e', 'gpu=amd', '-e', 'REALTIME', exe_vecadd_hip] : [],
}

foreach name, cmd : _measurements
  _out = name+'.tar.xz'
  _meas = {
    'cached': files(_out),
  }
  if cmd.length() > 0
    _gen = custom_target(output: _out, command: cmd, env: hpctoolkit_pyenv, console: true,
        build_always_stale: true, build_by_default: false)
    _meas += {'gen_target': _gen}
  endif
  testdata_meas += {name: _meas}
endforeach
