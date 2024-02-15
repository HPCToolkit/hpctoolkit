testdata_meas = {}

_exe_small = executable(
  'testmeas-small',
  files('small.c'),
  override_options: ['debug=true', 'optimization=0'],
)
_exe_loops = executable(
  'testmeas-loops',
  files('loops.c'),
  override_options: ['debug=true', 'optimization=0'],
  dependencies: dependency('openmp'),
)
if cupti_dep.found()
  _exe_loops_cuda = executable('testmeas-loops-cuda', files('loops.cu'))
  _exe_recursion_cuda = executable('testmeas-recursion-cuda', files('recursion.cu'))
else
  _exe_loops_cuda = disabler()
  _exe_recursion_cuda = disabler()
endif
_exe_loops_hip = custom_target(
  output: 'testmeas-loops-hip',
  input: files('loops.hip.cpp'),
  command: hipcc_cmd,
)

_run = [find_program(files('run')), hpcrun, hpcstruct]

_measurements = {
  # `small` is designed to contain a few samples scattered through a tiny, easy-to-understand
  # calling context tree. Good for debugging very bad problems.
  'small': {
    'tag': 'cpu',
    'minsamples': 3,
    'tail': ['-e', 'CPUTIME@100000', '-t', _exe_small],
  },

  # `loops-*` are designed to exercise more interesting cases with multiple nested loops and
  # simple multithreading. Various events/trace settings are tested to exercise minor differences
  # in the different metrics.
  'loops-cputime-t': {
    'tag': 'none',
    'tail': ['-e', 'CPUTIME', '-t', _exe_loops],
  },
  'loops-perf': {
    'tag': 'cpu',
    'tail': ['-e', 'perf::cpu-clock@f80', '-e', 'perf::task-clock@100000000', _exe_loops],
  },
  'loops-cuda-nvidia-t': {
    'tag': 'nvidia',
    'tail': ['-e', 'gpu=nvidia', '-t', _exe_loops_cuda],
  },
  'loops-cuda-nvidiapc-t': {
    'tag': 'nvidia',
    'tail': ['-e', 'gpu=nvidia,pc', '-t', _exe_loops_cuda],
  },
  'loops-hip-amd-t': {
    'tag': 'amd',
    'tail': ['--rocprofiler-path', rocprofiler_path, '-e', 'gpu=amd', '-t', _exe_loops_hip],
  },

  # `recursion` is a small example that has recursive properties in the GPU kernel code.
  'recursion-cuda-nvidiapc-t': {
    'tag': 'nvidia',
    'tail': ['-e', 'gpu=nvidia,pc', '-t', _exe_recursion_cuda],
  },
}

foreach name, spec : _measurements
  _stamp = custom_target(
    output: f'@name@.m.fresh',
    build_by_default: false,
    build_always_stale: true, # This can be removed once we can actually track when hpcrun changes
    console: true,
    command: [_run, spec.get('minsamples', 100).to_string(), '@OUTPUT@'] + spec['tail'],
    env: test_env,
    depends: test_depends,
  )
  testdata_meas += {
    name: {
      'dir': meson.current_source_dir() / f'@name@.m',
      'tag': spec['tag'],
      'freshstamp': _stamp,
    },
  }
  testdata_files += {
    testdata_meas[name]['dir']: {
      'tag': spec['tag'],
      'src': meson.current_build_dir() / f'@name@.m',
      'depends': [_stamp],
    },
  }
endforeach
