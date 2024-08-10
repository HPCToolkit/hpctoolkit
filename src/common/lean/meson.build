# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

common_lean_srcs = files(
  'BalancedTree.c',
  'OSUtil.c',
  'bichannel.c',
  'binarytree.c',
  'bistack.c',
  'compress_lzma.c',
  'cpuset_hwthreads.c',
  'crypto-hash.c',
  'cskiplist.c',
  'demangle.c',
  'elf-extract.c',
  'elf-hash.c',
  'elf-helper.c',
  'formats/cctdb.c',
  'formats/metadb.c',
  'formats/primitive.c',
  'formats/profiledb.c',
  'formats/tracedb.c',
  'generic_pair.c',
  'hpcfmt.c',
  'hpcio-buffer.c',
  'hpcio.c',
  'hpcrun-fmt.c',
  'id-tuple.c',
  'lush/lush-support.c',
  'mcs-lock.c',
  'md5.c',
  'pfq-rwlock.c',
  'placeholders.c',
  'procmaps.c',
  'producer_wfq.c',
  'queues.c',
  'randomizer.c',
  'spinlock.c',
  'splay-uint64.c',
  'stacks.c',
  'timer.c',
  'urand.c',
  'usec_time.c',
  'vdso.c',
)
common_lean_deps = [libelf_dep, libiberty_dep, lzma_dep, threads_dep]

test_srcs = files(
  'compress_lzma_test.cpp',
  'crypto-hash-test.cpp',
  'elf-hash-test.cpp',
  'randomizer-test.cpp',
)

test(
  'common-lean',
  executable(
    'common-lean-test',
    test_srcs,
    common_lean_srcs,
    dependencies: [threads_dep, gmock_dep, common_lean_deps],
  ),
  args: [files('elf-hash-example')],
  protocol: 'gtest',
)
