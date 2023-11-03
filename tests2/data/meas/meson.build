testdata_meas = {}

_exe_small = executable('testmeas-small', files('small.c'),
    override_options: ['debug=true', 'optimization=0'])
_exe_loops = executable('testmeas-loops', files('loops.c'),
    override_options: ['debug=true', 'optimization=0'],
    dependencies: dependency('openmp'))
if has_cuda
  _exe_loops_cuda = executable('testmeas-loops-cuda', files('loops.cu'))
else
  _exe_loops_cuda = disabler()
endif
if has_rocm
  _exe_loops_hip = custom_target(output: 'testmeas-loops-hip', input: files('loops.hip.cpp'),
      command: hipcc_cmd)
  _rocprofiler_prefix = rocm_profiler_exdep.get_variable(internal: 'prefix')
else
  _exe_loops_hip = disabler()
  _rocprofiler_prefix = disabler()
endif

_run = [find_program(files('run')), hpcrun, hpcstruct]

_measurements = {
  # `small` is designed to contain 3-4 samples scattered through a tiny, easy-to-understand
  # calling context tree. Good for debugging very bad problems.
  'small': { 'tag': 'cpu', 'minsamples': 3,
    'tail': ['-e', 'cycles@1000000000', '-e', 'instructions@100000000000', '-t', _exe_small],
  },

  # `loops-*` are designed to exercise more interesting cases with multiple nested loops and
  # simple multithreading. Various events/trace settings are tested to exercise minor differences
  # in the different metrics. Tuned to contain <300 samples total.
  'loops-cputime-t': { 'tag': 'none',
    'tail': ['-e', 'CPUTIME', '-t', _exe_loops],
  },
  'loops-perf-t': { 'tag': 'cpu',
    'tail': ['-e', 'cycles@f1000', '-e', 'instructions@12000000', '-t', _exe_loops],
  },
  'loops-cuda-nvidia-t': { 'tag': 'nvidia',
    'tail': ['-e', 'gpu=nvidia', '-t', _exe_loops_cuda],
  },
  'loops-cuda-nvidiapc-t': { 'tag': 'nvidia',
    'tail': ['-e', 'gpu=nvidia,pc', '-t', _exe_loops_cuda],
  },
  'loops-hip-amd-t': { 'tag': 'amd',
    'tail': ['--rocprofiler-path', _rocprofiler_prefix, '-e', 'gpu=amd', '-t', _exe_loops_hip],
  },
}

foreach name, spec : _measurements
  _stamp = custom_target(output: f'@name@.m.fresh', build_by_default: false,
      build_always_stale: true,  # This can be removed once we can actually track when hpcrun changes
      console: true, command:
          [_run, spec.get('minsamples', 100).to_string(), '@OUTPUT@']
          + spec['tail'],
  )
  testdata_meas += {name: {
    'dir': meson.current_source_dir() / f'@name@.m',
    'tag': spec['tag'],
    'freshstamp': _stamp,
  }}
  testdata_files += {testdata_meas[name]['dir']: {
    'tag': spec['tag'],
    'src': meson.current_build_dir() / f'@name@.m',
    'depends': [_stamp],
  }}
endforeach
