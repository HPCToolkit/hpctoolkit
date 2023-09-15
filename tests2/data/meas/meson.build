testdata_meas = {}

_exe_small = executable('testmeas-small', files('small.c'),
    override_options: ['debug=true', 'optimization=0'])
_exe_loops = executable('testmeas-loops', files('loops.c'),
    override_options: ['debug=true', 'optimization=0'],
    dependencies: dependency('openmp'))

_run = [find_program(files('run')), hpctesttool, hpcrun, hpcstruct]

_measurements = {
  # `small` is designed to contain 3-4 samples scattered through a tiny, easy-to-understand
  # calling context tree. Good for debugging very bad problems.
  'small': {
    'cmd': [_run, '3', '@OUTPUT@', '-e', 'cycles@1000000000', '-e', 'instructions@100000000000', '-t', _exe_small],
  },

  # `loops-*` are designed to exercise more interesting cases with multiple nested loops and
  # simple multithreading. Various events/trace settings are tested to exercise minor differences
  # in the different metrics. Tuned to contain <300 samples total.
  'loops-cputime-t': {
    'cmd': [_run, '100', '@OUTPUT@', '-e', 'CPUTIME', '-t', _exe_loops],
  },
  'loops-perf-t': {
    'cmd': [_run, '100', '@OUTPUT@', '-e', 'cycles@f1000', '-e', 'instructions@12000000', '-t', _exe_loops],
  },
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidiapc-t': {},
  'loops-hip-amd-t': {},
}
if has_cuda
  _exe_loops_cuda = executable('testmeas-loops-cuda', files('loops.cu'))
  _measurements += {
    'loops-cuda-nvidia-t': {
      'cmd': [_run, '100', '@OUTPUT@', '-e', 'gpu=nvidia', '-t', _exe_loops_cuda],
    },
    'loops-cuda-nvidiapc-t': {
      'cmd': [_run, '100', '@OUTPUT@', '-e', 'gpu=nvidia,pc', '-t', _exe_loops_cuda],
    },
  }
endif
if has_rocm
  _exe_loops_hip = custom_target(output: 'testmeas-loops-hip', input: files('loops.hip.cpp'),
      command: hipcc_cmd)
  _measurements += {
    'loops-hip-amd-t': {
      'cmd': [_run, '100', '@OUTPUT@',
              '--rocprofiler-path', rocm_profiler_exdep.get_variable(internal: 'prefix'),
              '-e', 'gpu=amd', '-t', _exe_loops_hip],
    },
  }
endif
_xfails = []

_root = testdata_root + 'meas/'
foreach name, spec : _measurements
  _out = name+'.tar.xz'
  assert(fs.is_samepath(testdata_srcroot / _root+_out, files(_out)))
  _gen_attrs = {}
  if 'cmd' in spec
    _gen = custom_target(output: _out, command: spec['cmd'],
                         console: true, build_always_stale: true, build_by_default: false)
    _gen_attrs = {
      'fresh': _gen,
      'gen': {
        'files': {_root+_out: _gen},
      },
    }
  endif

  # Extract the tarball at configure-time to make it easier to manage
  _extract_path = meson.current_build_dir() / f'@name@.m'
  run_command('rm', '-rf', _extract_path, check: true)
  run_command(python, '-m', 'tarfile', '-e', files(_out), _extract_path, check: true)

  testdata_meas += {name: {
    'dir': _extract_path,
    'xfail': name in _xfails,
  } + _gen_attrs}
endforeach
