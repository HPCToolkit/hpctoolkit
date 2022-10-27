foreach name, meas : testdata_meas
  test(f'hpcprof flags work on @name@', python3, args: [files('flags-work.py'), meas['cached']],
      env: hpctoolkit_pyenv, suite: 'hpcprof')
endforeach
foreach name, dbase : testdata_dbase_current
  test(f'hpcprof consistent on @name@', python3,
      args: [files('consistent.py'), dbase['measurements']['cached'], dbase['cached_db']],
      env: hpctoolkit_pyenv, suite: 'hpcprof', timeout: 30 * 4)
  if mpic.found() or mpicpp.found()
    test(f'hpcprof-mpi consistent on @name@', python3,
        args: [files('consistent-mpi.py'), dbase['measurements']['cached'], dbase['cached_db']],
        env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'], timeout: 30 * 8)
  endif
endforeach
