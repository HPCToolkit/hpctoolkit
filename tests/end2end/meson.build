# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_tst = find_program(files('tst-default-pipeline'))
foreach exe : [
  executable(f'tstexe-e2e-openmp', files('openmp.cpp'), dependencies: openmp_dep)
]
  test(
    'Default analysis pipeline on @0@ works'.format(exe.name()),
    _tst,
    args: [hpctesttool, hpcrun, hpcstruct, hpcprof, exe],
    suite: 'end2end',
    timeout: 90,
    env: test_env,
    depends: test_depends,
  )
endforeach
