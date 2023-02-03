_testdata_dbase = {}

_prof = find_program(files('prof'))

_databases = {
  'small': {'yaml': true},
  'loops-cputime-t': {},
  'loops-perf-t': {},
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidiapc-t': {},
  'loops-hip-amd-t': {},
}

_extract_db = find_program(files('extract-db'))
_root = testdata_root + 'dbase/v4.0/'
foreach name, features : _databases
  _out_db = name+'.tar.xz'
  _out_yaml = name+'.yaml'

  _cmd = [_prof, '@INPUT@', '@OUTPUT0@']
  _output = [_out_db]
  assert(fs.is_samepath(testdata_srcroot / _root+_out_db, files(_out_db)))
  if features.get('yaml', false)
    _cmd += ['-y', '@OUTPUT1@']
    _output += [_out_yaml]
    assert(fs.is_samepath(testdata_srcroot / _root+_out_yaml, files(_out_yaml)))
  endif
  _gen = custom_target(output: _output, command: _cmd, input: testdata_meas[name]['fresh'],
      env: hpctoolkit_pyenv, build_by_default: false)
  _gen_files = {_root+_out_db: _gen[0]}
  if features.get('yaml', false)
    _gen_files += {_root+_out_yaml: _gen[1]}
  endif

  # Extract the tarball at configure-time to make it easier to manage
  _extract_path = meson.current_build_dir() / f'@name@.d'
  if run_command(_extract_db, files(_out_db), _extract_path, check: false).returncode() != 0
    warning(f'Failed to extract @_root@@_out_db@, some tests may fail!')
  endif

  _dbase = {
    'measurements': testdata_meas[name],
    'dir': _extract_path,
    'xfail': features.get('xfail', false) or testdata_meas[name]['xfail'],
    'gen': {
      'suite': testdata_meas[name]['gen']['suite'],
      'files': _gen_files,
    },
  }
  if features.get('yaml', false)
    _dbase += {'yaml': files(_out_yaml)}
  endif

  _testdata_dbase += {name: _dbase}
endforeach

testdata_dbase_4_0 = _testdata_dbase
