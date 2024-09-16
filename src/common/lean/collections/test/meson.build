# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

project('collection-tests', 'c')

incdir = include_directories('..', '../../../../')

e = executable(
  'freelist-test-inplace',
  'freelist-test-inplace.c',
  include_directories: incdir,
)
test('freelist-test-inplace', e)

e = executable('stack-test-inplace', 'stack-test-inplace.c', include_directories: incdir)
test('stack-test-inplace', e)

e = executable(
  'splay-tree-test-inplace',
  'splay-tree-test-inplace.c',
  include_directories: incdir,
)
test('splay-tree-test-inplace', e)


thread_dep = dependency('threads')

e = executable(
  'mpsc-queue-test-inplace',
  'mpsc-queue-test-inplace.c',
  include_directories: incdir,
  dependencies: thread_dep,
)
test('mpsc-queue-test-inplace', e)

e = executable(
  'concurrent-stack-test-inplace',
  'concurrent-stack-test-inplace.c',
  include_directories: incdir,
  dependencies: thread_dep,
)
test('concurrent-stack-test-inplace', e)

e = executable(
  'concurrent-id-map-test-inplace',
  'concurrent-id-map-test-inplace.c',
  include_directories: incdir,
  dependencies: thread_dep,
)
test('concurrent-id-map-test-inplace', e)
