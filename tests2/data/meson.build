testdata_root = 'tests2/data/'
testdata_srcroot = meson.project_source_root() / '..'

subdir('meas')
subdir('dbase')

_make_tar = find_program(files('make-tar'))
_tar_cmds = {
  'none': [_make_tar, '@OUTPUT@'],
  'cpu': [_make_tar, '@OUTPUT@'],
  'nvidia': [_make_tar, '@OUTPUT@'],
  'amd': [_make_tar, '@OUTPUT@'],
}

foreach testdata : [testdata_meas, testdata_dbase]
  foreach _, spec : testdata
    _suite = spec['gen']['suite']
    foreach dstpath, srcgen : spec['gen']['files']
      _tar_cmds += {
        _suite: _tar_cmds[_suite] + ['--add', srcgen, dstpath],
      }
    endforeach
  endforeach
endforeach

foreach suite, cmd : _tar_cmds
  custom_target(output: f'fresh-@suite@.tar.xz', command: cmd, build_by_default: false)
endforeach
