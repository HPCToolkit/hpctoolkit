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
  foreach threads : [1, 3]
    test(
      f'Database from @name@ is accurate (-j@threads@)',
      _tst,
      args: [hpctesttool, dbase['dir'], hpcprof, f'-j@threads@', dbase['measurements']['dir']],
      suite: 'hpcprof',
    )
  endforeach

  if mpi_dep.found()
    foreach x : [[1, 1], [3, 1], [2, 2]]
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
        is_parallel: false,  # Needed to deal with touchy MPI implementations
        priority: 100,  # High priority to optimize for lack of parallelism
        timeout: 90,
      )
    endforeach
  endif
endforeach
