testdata_files = {}

subdir('meas')
subdir('dbase')
subdir('bin')
subdir('struct')

_cp_r = find_program(files('cp-r'))

_regen = {
  'none': {'cmd': [_cp_r], 'depends': []},
  'cpu': {'cmd': [_cp_r], 'depends': []},
  'nvidia': {'cmd': [_cp_r], 'depends': []},
  'sw-cuda': {'cmd': [_cp_r], 'depends': []},
  'amd': {'cmd': [_cp_r], 'depends': []},
}
foreach dst, src : testdata_files
  _orig = _regen[src['tag']]
  _new = {
    'cmd': _orig['cmd'] + [src['src'], dst],
    'depends': _orig['depends'] + src.get('depends', []),
  }
  _regen += {src['tag']: _new}
endforeach
foreach tag, data : _regen
  run_target(f'regen-testdata-@tag@', command: data['cmd'], depends: data['depends'])
endforeach
