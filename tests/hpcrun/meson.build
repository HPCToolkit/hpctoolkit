# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_args = []
if rocm_dep.found()
  assert(not is_disabler(rocprofiler_path))
  _args += ['--rocprofiler-path', rocprofiler_path]
endif
test(
  'Listing metrics does not error',
  hpcrun,
  args: ['-L'] + _args,
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: hpcrun_test_env,
)

_env = hpcrun_test_env
_env.prepend('PATH', fs.parent(hpcrun.full_path()))
test(
  'Listing metrics as "hpcrun" does not error',
  find_program('env'),
  args: ['hpcrun', '-L'] + _args,
  suite: 'hpcrun',
  depends: hpcrun_test_depends,
  env: _env,
)

subdir('cpu')
subdir('threads')
subdir('python')
subdir('gpu/opencl')
subdir('gpu/cuda')
subdir('gpu/hip')
subdir('gpu/sycl')
