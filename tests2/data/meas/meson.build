testdata_meas = {}

_exe_small = executable('testmeas-small', files('small.c'),
    override_options: ['debug=true', 'optimization=0'])
_exe_loops = executable('testmeas-loops', files('loops.c'),
    override_options: ['debug=true', 'optimization=0'],
    dependencies: dependency('openmp'))
_exe_loops_cuda = executable('testmeas-loops-cuda', files('loops.cu'),
    dependencies: cuda_dep)
_exe_loops_hip = custom_target(output: 'testmeas-loops-hip', input: files('loops.hip.cpp'),
    command: hipcc_cmd)

_run = configure_file(input: files('run'), output: '@PLAINNAME@.venv', command: venv_shebang)
_measurements = {
  # `small` is designed to contain 3-4 samples scattered through a tiny, easy-to-understand
  # calling context tree. Good for debugging very bad problems.
  'small': {
    'cmd': [_run, '--min-samples=3', '--', '@OUTPUT@', '-e', 'cycles@1000000000', '-e', 'instructions@100000000000', '-t', _exe_small],
    'suite': 'cpu',
  },

  # `loops-*` are designed to exercise more interesting cases with multiple nested loops and
  # simple multithreading. Various events/trace settings are tested to exercise minor differences
  # in the different metrics. Tuned to contain <300 samples total.
  'loops-cputime-t': {
    'cmd': [_run, '--', '@OUTPUT@', '-e', 'CPUTIME', '-t', _exe_loops],
    'suite': 'none',
  },
  'loops-perf-t': {
    'cmd': [_run, '--', '@OUTPUT@', '-e', 'cycles@f100', '-e', 'instructions@120000000', '-t', _exe_loops],
    'suite': 'cpu',
  },
  'loops-cuda-nvidia-t': {
    'cmd': [_run, '--cuda', '--', '@OUTPUT@', '-e', 'gpu=nvidia', '-t', _exe_loops_cuda],
    'suite': 'nvidia', 'if': cuda_dep.found(),
  },
  'loops-cuda-nvidiapc-t': {
    'cmd': [_run, '--cuda-pc', '--', '@OUTPUT@', '-e', 'gpu=nvidia,pc', '-t', _exe_loops_cuda],
    'suite': 'nvidia', 'if': cuda_dep.found(),
  },
  'loops-hip-amd-t': {
    'cmd': [_run, '--', '@OUTPUT@', '-e', 'gpu=amd', '-t', _exe_loops_hip],
    'suite': 'amd', 'if': hipcc.found(),
  },
}
_xfails = []

_root = testdata_root + 'meas/'
foreach name, spec : _measurements
  _out = name+'.tar.xz'
  assert(fs.is_samepath(testdata_srcroot / _root+_out, files(_out)))
  if spec.get('if', true)
    _gen = custom_target(output: _out, command: spec['cmd'], env: hpctoolkit_pyenv,
                         console: true, build_always_stale: true, build_by_default: false)
  else
    _gen = disabler()
  endif

  # Extract the tarball at configure-time to make it easier to manage
  _extract_path = meson.current_build_dir() / f'@name@.m'
  if run_command(venv_python, files('extract-ms'), files(_out), _extract_path,
                 check: false).returncode() != 0
    warning(f'Failed to extract @_root@@_out@, some tests may fail!')
  endif

  testdata_meas += {name: {
    'dir': _extract_path,
    'xfail': name in _xfails,
    'fresh': _gen,
    'gen': {
      'suite': spec['suite'],
      'files': {_root+_out: _gen},
    },
  }}
endforeach
