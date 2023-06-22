_tst = configure_file(input: files('tst-flags-effective'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
foreach name, meas : testdata_meas
  test(f'Flags on @name@ are effective',
       _tst, args: [meas['dir']],
       env: hpctoolkit_pyenv, suite: 'hpcprof',
       should_fail: meas['xfail'])
endforeach

_tst = configure_file(input: files('tst-accuracy'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
foreach name, dbase : testdata_dbase_current
  foreach threads : [1, 64]
    test(f'Database from @name@ is accurate (-j@threads@)',
         _tst, args: [f'-j@threads@', dbase['measurements']['dir'], dbase['dir']],
         env: hpctoolkit_pyenv, suite: 'hpcprof',
         should_fail: dbase['xfail'], is_parallel: threads == 1)
  endforeach

  if mpicxx.found()
    foreach x : [[1, 1], [1, 64], [2, 1], [4, 2]]
      ranks = x[0]
      threads = x[1]
      test(f'Database from @name@ is accurate (ranks=@ranks@ -j@threads@)',
           _tst, args: [f'-n@ranks@', f'-j@threads@', dbase['measurements']['dir'], dbase['dir']],
           env: hpctoolkit_pyenv, suite: ['hpcprof', 'mpi'],
           should_fail: dbase['xfail'], priority: 10, is_parallel: false, timeout: 90)
    endforeach
  endif
endforeach
