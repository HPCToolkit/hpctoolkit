testdata_dbase = {}

_databases = {
  'small': {'yaml': true},
  'loops-cputime-t': {},
  'loops-perf': {},
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidiapc-t': {},
  'recursion-cuda-nvidiapc-t': {},
}

_prof = [find_program(files('prof')), hpcprof]

foreach name, features : _databases
  _stamp = custom_target(output: f'@name@.d.fresh', build_by_default: false,
    build_always_stale: true,  # This can be removed once we can actually track when hpcprof changes
    console: true, command: [_prof, testdata_meas[name]['freshstamp'], '@OUTPUT@'],
  )
  _dbase = {
    'measurements': testdata_meas[name],
    'dir': meson.current_source_dir() / f'@name@.d',
    'freshstamp': _stamp,
  }
  testdata_files += {_dbase['dir']: {
    'tag': testdata_meas[name]['tag'],
    'src': meson.current_build_dir() / f'@name@.d',
    'depends': [_stamp],
  }}

  if features.get('yaml', false)
    _yaml = custom_target(output: f'@name@.yaml', build_by_default: false,
      depends: [_stamp],
      command: [hpctesttool, 'test', 'yaml',
                meson.current_build_dir() / f'@name@.d', '@OUTPUT@'])
    _dbase += {
      'yaml': meson.current_source_dir() / f'@name@.yaml',
      'freshyaml': _yaml,
    }
    testdata_files += {_dbase['yaml']: {'src': _yaml, 'tag': testdata_meas[name]['tag']}}
  endif

  testdata_dbase += {name: _dbase}
endforeach
