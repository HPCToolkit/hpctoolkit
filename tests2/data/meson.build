subdir('meas')
subdir('dbase')

_cp = find_program('cp', native: true)

_gen_targets = []
_cp_targets = []
foreach name, spec : testdata_meas
  if spec.has_key('gen_target')
    _gen_targets += spec['gen_target']
    _cp_targets += [run_target(f'install-testdata-meas-@name@',
        command: [_cp, spec['gen_target'], spec['cached']])]
  endif
endforeach
foreach name, spec : testdata_dbase
  if spec.has_key('gen_target')
    _gen_targets += spec['gen_target']
    _cp_targets += [run_target(f'install-testdata-dbase-@name@-db',
        command: [_cp, spec['gen_target_db'], spec['cached_db']])]
    if spec.has_key('cached_yaml')
      _cp_targets += [run_target(f'install-testdata-dbase-@name@-yaml',
          command: [_cp, spec['gen_target_yaml'], spec['cached_yaml']])]
    endif
  endif
endforeach
alias_target('gen-testdata', _gen_targets)
alias_target('install-testdata', _cp_targets)
