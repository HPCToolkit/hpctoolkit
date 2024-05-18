# SPDX-FileCopyrightText: 2023-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

if not python_inst.found()
  subdir_done()
endif

_tst = find_program(files('tst-python-simple'))
test(
  'Measurement of simple-1thread unwound Python',
  _tst,
  args: [hpctesttool, hpcrun, hpcprof, python.full_path(), files('simple-1thread'), '1'],
  suite: ['hpcrun', 'python'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of simple-mthread unwound Python',
  _tst,
  args: [hpctesttool, hpcrun, hpcprof, python.full_path(), files('simple-mthread'), '3'],
  suite: ['hpcrun', 'python'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
  # FIXME: The Python support currently can't detect Python-spawned threads
  should_fail: true,
)
test(
  'Measurement of simple-exception unwound Python',
  _tst,
  args: [hpctesttool, hpcrun, hpcprof, python.full_path(), files('simple-exception'), '1'],
  suite: ['hpcrun', 'python'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
test(
  'Measurement of simple-signals unwound Python',
  _tst,
  args: [hpctesttool, hpcrun, hpcprof, python.full_path(), files('simple-signals'), '1'],
  suite: ['hpcrun', 'python'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

_tst = find_program(files('tst-python-simple-noaudit'))
test(
  'Measurement of simple-1thread unwound Python w/o auditor',
  _tst,
  args: [hpctesttool, hpcrun, hpcprof, python.full_path(), files('simple-1thread'), '1'],
  suite: ['hpcrun', 'python'],
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)
