foreach name, meas : testdata_meas
  test(f'hpcprof flags work on @name@', python3, args: [files('flags-work.py'), meas['cached']],
      env: hpctoolkit_pyenv, suite: 'hpcprof',
      should_fail: meas['xfail'])
endforeach
foreach name, dbase : testdata_dbase_current
  foreach threads : [1, 4, 16, 64]
    threadcnt = threads.to_string()
    test(f'hpcprof -j@threadcnt@ @name@ / accuracy', python3,
        args: [files('accuracy.py'), threadcnt, dbase['measurements']['cached'], dbase['cached_db']],
        env: hpctoolkit_pyenv, suite: 'hpcprof',
        should_fail: dbase['xfail'])
  endforeach

  if mpic.found() or mpicpp.found()
    foreach ranks : [1, 2, 4, 8]
      rankcnt = ranks.to_string()
      foreach threads : [1, 4]
        threadcnt = threads.to_string()
        test(f'hpcprof-mpi ranks=@rankcnt@ -j@threadcnt@ @name@ / accuracy', python3,
            args: [files('accuracy-mpi.py'), rankcnt, threadcnt, dbase['measurements']['cached'],
                   dbase['cached_db']],
            env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'],
            should_fail: dbase['xfail'], timeout: ranks * 20)
      endforeach
    endforeach
  endif
endforeach
