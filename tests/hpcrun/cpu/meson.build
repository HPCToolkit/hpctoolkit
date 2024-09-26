# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

simple_tstexe = executable(
  'tstexe-simple-spin',
  'simple-spin.c',
  dependencies: [math_dep],
)

subdir('itimer')
subdir('papi')
subdir('perf')
subdir('pthread-blame')
subdir('memleak')
subdir('io')
