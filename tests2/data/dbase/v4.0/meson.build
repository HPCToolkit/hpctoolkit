_testdata_dbase = {}

_prof = [python3, files('prof')]
_pickle = [python3, files('pickle-db'), '@INPUT@', '@OUTPUT@']

_databases = {
  'small': {'yaml': true},
  'loops-perf-t': {},
  'loops-cuda-nvidia-t': {},
  'loops-cuda-nvidiapc-t': {},
  'loops-hip-amd-t': {},
}

foreach name, features : _databases
  _out_db = name+'.tar.xz'
  _dbase = {
    'measurements': testdata_meas[name],
    'tarball': files(_out_db),
    'pickled': custom_target(output: name+'.pickle', input: files(_out_db),
        command: _pickle, env: hpctoolkit_pyenv, build_always_stale: true,
        build_by_default: false),
    'xfail': features.get('xfail', false) or testdata_meas[name]['xfail'],
  }
  _idxs = {'_db': 0}
  _cmd = [_prof, '@INPUT@', '@OUTPUT@0@@'.format(_idxs['_db'])]
  _output = [_out_db]

  if features.get('yaml', false)
    _out_yaml = name+'.yaml'
    _dbase += {'yaml': files(_out_yaml)}
    _idxs += {'_yaml': _output.length()}
    _cmd += ['-y', '@OUTPUT@0@@'.format(_output.length())]
    _output += [_out_yaml]
  endif

  if testdata_meas[name].has_key('gen_target')
    _gen = custom_target(output: _output, command: _cmd,
        input: testdata_meas[name]['gen_target'], env: hpctoolkit_pyenv,
        build_always_stale: true, build_by_default: false)
    _dbase += {'gen_target': _gen}
    foreach suffix, idx : _idxs
      _dbase += {'gen_target'+suffix: _gen[idx]}
    endforeach
  endif
  _testdata_dbase += {name: _dbase}
endforeach

testdata_dbase_4_0 = _testdata_dbase
