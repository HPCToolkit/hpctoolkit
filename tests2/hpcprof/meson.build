_tst = find_program(files('tst-flags-effective'))
foreach name, meas : testdata_meas
  test(f'Flags on @name@ are effective', _tst, args: [meas['extracted']],
      env: hpctoolkit_pyenv, suite: 'hpcprof',
      should_fail: meas['xfail'])
endforeach

_tst = find_program(files('tst-accuracy'))
foreach name, dbase : testdata_dbase_current
  foreach threads : [1, 64]
    test(f'Database from @name@ is accurate (-j@threads@)', _tst,
        args: [f'-j@threads@', dbase['measurements']['extracted'], dbase['pickled']],
        env: hpctoolkit_pyenv, suite: 'hpcprof',
        should_fail: dbase['xfail'], is_parallel: threads == 1)
  endforeach

  if mpic.found() or mpicpp.found()
    foreach x : [[1, 1], [1, 64], [2, 1], [4, 2]]
      ranks = x[0]
      threads = x[1]
      test(f'Database from @name@ is accurate (ranks=@ranks@ -j@threads@)', _tst,
          args: [f'-n@ranks@', f'-j@threads@', dbase['measurements']['extracted'], dbase['pickled']],
          env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'],
          should_fail: dbase['xfail'], priority: 10, is_parallel: false)
    endforeach
  endif
endforeach
