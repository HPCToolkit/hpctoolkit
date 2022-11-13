foreach name, meas : testdata_meas
  test(f'hpcprof flags work on @name@', python3, args: [files('flags-work.py'), meas['cached']],
      env: hpctoolkit_pyenv, suite: 'hpcprof',
      should_fail: meas['xfail'])
endforeach
foreach name, dbase : testdata_dbase_current
  foreach threadcnt : ['1', '4', '16', '64']
    test(f'hpcprof -j@threadcnt@ @name@ / accuracy', python3,
        args: [files('accuracy.py'), threadcnt, dbase['measurements']['cached'], dbase['cached_db']],
        env: hpctoolkit_pyenv, suite: 'hpcprof',
        should_fail: dbase['xfail'])
  endforeach

  if mpic.found() or mpicpp.found()
    foreach rankcnt : ['1'] # ['1', '2', '4', '8']
      foreach threadcnt : ['1', '4']
        test(f'hpcprof-mpi ranks=@rankcnt@ -j@threadcnt@ @name@ / accuracy', python3,
            args: [files('accuracy-mpi.py'), rankcnt, threadcnt, dbase['measurements']['cached'],
                   dbase['cached_db']],
            env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'],
            should_fail: dbase['xfail'] or rankcnt != '1' \
                         or name in ['loops-cuda-nvidiapc', 'loops-cuda-nvidiapc-t'])
      endforeach
    endforeach
  endif
endforeach
