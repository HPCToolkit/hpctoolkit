
_threadmodels = {
  'pthread': executable('tstexe-pthread', files('pthread.c'), dependencies: [threads_dep, math_dep]),
  'openmp': executable('tstexe-openmp', files('openmp.cpp'), dependencies: [openmp_dep, math_dep]),
  'cpp-thread': executable('tstexe-cpp-thread', files('cpp-thread.cpp'), dependencies: [threads_dep, math_dep]),
  'c11-thread': executable('tstexe-c11-thread', files('c11-thread.c'), dependencies: [threads_dep, math_dep]),
}
foreach _threadmodel, _tstexe : _threadmodels
  # FIXME: C11 thread functions aren't intercepted in current versions, which means the tests will
  # fail when using the C11 threading model.
  _should_fail = _threadmodel == 'c11-thread'

  test(
    f'Measurement of tstexe-@_threadmodel@ produces profiles',
    find_program(files('tst-threads-produces-profiles')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
  test(
    f'Measurement of tstexe-@_threadmodel@ produces profiles w/o auditor',
    find_program(files('tst-threads-produces-profiles-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )

  test(
    f'CPUTIME event measures on tstexe-@_threadmodel@',
    find_program(files('tst-threads-cputime')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
  test(
    f'CPUTIME event measures on tstexe-@_threadmodel@ w/o auditor',
    find_program(files('tst-threads-cputime-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )

  test(
    f'REALTIME event measures on tstexe-@_threadmodel@',
    find_program(files('tst-threads-realtime')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
  test(
    f'REALTIME event measures on tstexe-@_threadmodel@ w/o auditor',
    find_program(files('tst-threads-realtime-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )

  test(
    f'Perf events measure on tstexe-@_threadmodel@',
    find_program(files('tst-threads-perf')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
  test(
    f'Perf events measure on tstexe-@_threadmodel@ w/o auditor',
    find_program(files('tst-threads-perf-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
endforeach
