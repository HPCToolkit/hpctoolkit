testdata_dbase = {}

_databases = {
  'small': {'yaml': true},
  'small.nostruct': {'yaml': true, 'args': ['--ignore-structs'], 'remap_measdir': true},
  'loops-cputime-t': {},
  'loops-cputime-t.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
  'loops-perf': {},
  'loops-perf.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidia-t.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
  'loops-cuda-nvidiapc-t': {},
  'loops-cuda-nvidiapc-t.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
  'recursion-cuda-nvidiapc-t': {},
  'recursion-cuda-nvidiapc-t.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
  'loops-hip-amd-t': {},
  'loops-hip-amd-t.nostruct': {'args': ['--ignore-structs'], 'remap_measdir': true},
}

_prof = [find_program(files('prof')), hpcprof]

foreach _name, _features : _databases
  _meas = testdata_meas[_name.split('.')[0]]
  _args = ['--foreign'] + _features.get('args', [])
  if _features.get('remap_measdir', false) and 'orig_measdir' in _meas and _meas['orig_measdir'] != _meas['dir']
    # FIXME: This should really be handled automatically by hpcrun/hpcprof, but until the support
    # for renaming measurement directories becomes a reality we do a remapping ourselves.
    # See also https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/411
    # and https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/269
    _args += ['-R@0@=@1@'.format(_meas['orig_measdir'].replace('\\', '\\\\').replace('=', '\\='), _meas['dir'].replace('\\', '\\\\').replace('=', '\\='))]
  endif

  _stamp = custom_target(
    output: f'@_name@.d.fresh',
    build_by_default: false,
    build_always_stale: true, # This can be removed once we can actually track when hpcprof changes
    console: true,
    command: [_prof, _meas['freshstamp'], '@OUTPUT@', _args],
  )
  _dbase = {
    'measurements': _meas,
    'dir': meson.current_source_dir() / f'@_name@.d',
    'freshstamp': _stamp,
    'args': _args,
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
