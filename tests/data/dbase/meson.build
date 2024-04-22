testdata_dbase = {}

_databases = {
  'small': {'yaml': true},
  'small.nostruct': {'yaml': true, 'args': ['--ignore-structs']},
  'loops-cputime-t': {},
  'loops-cputime-t.nostruct': {'args': ['--ignore-structs']},
  'loops-perf': {},
  'loops-perf.nostruct': {'args': ['--ignore-structs']},
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidia-t.nostruct': {'args': ['--ignore-structs']},
  'loops-cuda-nvidiapc-t': {},
  'loops-cuda-nvidiapc-t.nostruct': {'args': ['--ignore-structs']},
  'recursion-cuda-nvidiapc-t': {},
  'recursion-cuda-nvidiapc-t.nostruct': {'args': ['--ignore-structs']},
  'loops-hip-amd-t': {},
  'loops-hip-amd-t.nostruct': {'args': ['--ignore-structs']},
}

_prof = [find_program(files('prof')), hpcprof]

foreach _name, _features : _databases
  _meas = testdata_meas[_name.split('.')[0]]
  _stamp = custom_target(
    output: f'@_name@.d.fresh',
    build_by_default: false,
    build_always_stale: true, # This can be removed once we can actually track when hpcprof changes
    console: true,
    command: [_prof, _meas['freshstamp'], '@OUTPUT@', '--foreign', _features.get('args', [])],
  )
  _dbase = {
    'measurements': _meas,
    'dir': meson.current_source_dir() / f'@_name@.d',
    'freshstamp': _stamp,
    'args': ['--foreign'] + _features.get('args', []),
  }
  testdata_files += {
    _dbase['dir']: {
      'tag': _meas['tag'],
      'src': meson.current_build_dir() / f'@_name@.d',
      'depends': [_stamp],
    },
  }

  if _features.get('yaml', false)
    _yaml = custom_target(
      output: f'@_name@.yaml',
      build_by_default: false,
      depends: [_stamp],
      command: [hpctesttool, 'test', 'yaml', meson.current_build_dir() / f'@_name@.d', '@OUTPUT@'],
    )
    _dbase += {
      'yaml': meson.current_source_dir() / f'@_name@.yaml',
      'freshyaml': _yaml,
    }
    testdata_files += {_dbase['yaml']: {'src': _yaml, 'tag': _meas['tag']}}
  endif

  testdata_dbase += {_name: _dbase}
endforeach
