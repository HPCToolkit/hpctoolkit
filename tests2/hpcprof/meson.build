_tst = find_program(files('tst-flags-effective'))
foreach name, meas : testdata_meas
  test(
    f'Flags on @name@ are effective',
    _tst,
    args: [hpctesttool, hpcprof, meas['dir']],
    suite: 'hpcprof',
  )
endforeach

_tst = find_program(files('tst-accuracy'))
foreach name, dbase : testdata_dbase
  foreach threads : [1, 64]
    test(
      f'Database from @name@ is accurate (-j@threads@)',
      _tst,
      args: [hpctesttool, dbase['dir'], hpcprof, f'-j@threads@', dbase['measurements']['dir']],
      suite: 'hpcprof',
      is_parallel: threads == 1,
      priority: threads == 1 ? 0 : 100,  # High priority to optimize for lack of parallelism
    )
  endforeach

  if mpi_dep.found()
    foreach x : [[1, 1], [1, 64], [2, 1], [4, 2]]
      ranks = x[0]
      threads = x[1]
      test(
        f'Database from @name@ is accurate (ranks=@ranks@ -j@threads@)',
        _tst,
        args: [
          hpctesttool,
          dbase['dir'],
          mpiexec,
          f'@ranks@',
          hpcprof_mpi,
          f'-j@threads@',
          dbase['measurements']['dir'],
        ],
        suite: ['hpcprof', 'mpi'],
        priority: 100,  # High priority to optimize for lack of parallelism
        is_parallel: false,
        timeout: 90,
      )
    endforeach
  endif
endforeach
