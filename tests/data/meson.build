# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

testdata_files = {}

subdir('meas')
subdir('dbase')
subdir('bin')
subdir('struct')

_cp_r = find_program(files('cp-r'))
_git = find_program('git', required: false, disabler: true)

_regen = {
  'none': {'cmd': [_cp_r], 'depends': [], 'patches': []},
  'cpu': {'cmd': [_cp_r], 'depends': [], 'patches': []},
  'nvidia': {'cmd': [_cp_r], 'depends': [], 'patches': []},
  'sw-cuda': {'cmd': [_cp_r], 'depends': [], 'patches': []},
  'amd': {'cmd': [_cp_r], 'depends': [], 'patches': []},
}
foreach dst, src : testdata_files
  _orig = _regen[src['tag']]
  _patch = custom_target(
    output: fs.name(dst) + '.patch',
    capture: true,
    command: [
      _git,
      '-C',
      '@SOURCE_ROOT@',
      'diff',
      '--full-index',
      '--binary',
      'HEAD',
      '--',
      dst,
    ],
  )
  _new = {
    'cmd': _orig['cmd'] + [src['src'], dst],
    'depends': _orig['depends'] + src.get('depends', []),
    'patches': _orig['patches'] + [_patch],
  }
  _regen += {src['tag']: _new}
endforeach
foreach tag, data : _regen
  run_target(f'regen-testdata-@tag@', command: data['cmd'], depends: data['depends'])
  alias_target(f'patches-testdata-@tag@', data['patches'])
endforeach
