_flags_work = find_program(files('flags-work'))
_accuracy = find_program(files('accuracy'))
_accuracy_mpi = find_program(files('accuracy-mpi'))

foreach name, meas : testdata_meas
  test(f'hpcprof flags work on @name@', _flags_work, args: [meas['extracted']],
      env: hpctoolkit_pyenv, suite: 'hpcprof',
      should_fail: meas['xfail'])
endforeach
foreach name, dbase : testdata_dbase_current
  foreach threads : [1, 64]
    threadcnt = threads.to_string()
    test(f'hpcprof -j@threadcnt@ @name@ / accuracy', _accuracy,
        args: [threadcnt, dbase['measurements']['extracted'], dbase['pickled']],
        env: hpctoolkit_pyenv, suite: 'hpcprof',
        should_fail: dbase['xfail'], is_parallel: threads == 1)
  endforeach

  if mpic.found() or mpicpp.found()
    foreach x : [[1, 1], [1, 64], [2, 1], [4, 2]]
      ranks = x[0]
      threads = x[1]
      rankcnt = ranks.to_string()
      threadcnt = threads.to_string()
      test(f'hpcprof-mpi ranks=@rankcnt@ -j@threadcnt@ @name@ / accuracy', _accuracy_mpi,
          args: [rankcnt, threadcnt, dbase['measurements']['extracted'], dbase['pickled']],
          env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'],
          should_fail: dbase['xfail'], priority: 10, is_parallel: false)
    endforeach
  endif
endforeach
