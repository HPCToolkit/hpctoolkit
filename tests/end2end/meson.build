# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_tst = find_program(files('tst-default-workflow'))
_tst_ss = find_program(files('tst-single-structfile-workflow'))
foreach exe : [executable('tstexe-e2e-openmp', files('openmp.cpp'), dependencies: openmp_dep)]
  test(
    'Default analysis workflow on @0@ works'.format(exe.name()),
    _tst,
    args: [hpctesttool, hpcrun, hpcstruct, hpcprof, exe],
    suite: 'end2end',
    timeout: 90,
    env: test_env,
    depends: test_depends,
  )
  test(
    'Single-structfile analysis workflow on @0@ works'.format(exe.name()),
    _tst_ss,
    args: [hpctesttool, hpcrun, hpcstruct, hpcprof, exe],
    suite: 'end2end',
    timeout: 90,
    env: test_env,
    depends: test_depends,
  )
endforeach
