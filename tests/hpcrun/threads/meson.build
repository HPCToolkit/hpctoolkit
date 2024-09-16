# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_threadmodels = {
  'pthread': executable(
    'tstexe-pthread',
    files('pthread.c'),
    dependencies: [threads_dep, math_dep],
  ),
  'openmp': executable(
    'tstexe-openmp',
    files('openmp.c'),
    dependencies: [openmp_dep, math_dep],
  ),
  'cpp-thread': executable(
    'tstexe-cpp-thread',
    files('cpp-thread.cpp'),
    dependencies: [threads_dep, math_dep],
  ),
  'c11-thread': executable(
    'tstexe-c11-thread',
    files('c11-thread.c'),
    dependencies: [threads_dep, math_dep],
  ),
}
foreach _threadmodel, _tstexe : _threadmodels
  _should_fail = _threadmodel == 'c11-thread'  # FIXME: C11 threads are not yet intercepted

  test(
    f'Threads are tracked for @_threadmodel@',
    find_program(files('tst-threads-tracked')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    should_fail: _should_fail,
  )
  test(
    f'Threads are tracked for @_threadmodel@ w/o auditor',
    find_program(files('tst-threads-tracked-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    should_fail: _should_fail,
  )
endforeach
