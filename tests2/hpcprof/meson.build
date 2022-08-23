foreach meas : testdata_meas
  _fn = fs.name(meas)
  test(f'hpcprof flags work on @_fn@', python3, args: [files('flags-work.py'), meas],
    env: hpctoolkit_pyenv, suite: 'hpcprof')
endforeach
