# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_srcs = files(
  'accumulators.cpp',
  'args.cpp',
  'attributes.cpp',
  'context.cpp',
  'dataclass.cpp',
  'expression.cpp',
  'finalizer.cpp',
  'finalizers/denseids.cpp',
  'finalizers/directclassification.cpp',
  'finalizers/kernelsyms.cpp',
  'finalizers/logical.cpp',
  'finalizers/struct.cpp',
  'lexical.cpp',
  'metric.cpp',
  'module.cpp',
  'mpi/accumulate-thread.cpp',
  'packedids.cpp',
  'pipeline.cpp',
  'scope.cpp',
  'sink.cpp',
  'sinks/hpctracedb2.cpp',
  'sinks/metadb.cpp',
  'sinks/metricsyaml.cpp',
  'sinks/packed.cpp',
  'sinks/sparsedb.cpp',
  'source.cpp',
  'sources/hpcrun4.cpp',
  'sources/packed.cpp',
  'stdshim/atomic.cpp',
  'stdshim/futex-detail.c',
  'stdshim/shared_mutex.cpp',
  'util/file-posix.cpp',
  'util/log.cpp',
  'util/lzmastream.cpp',
  'util/once.cpp',
  'util/ragged_vector.cpp',
  'util/stable_hash.cpp',
  'util/xml.cpp',
)
_srcs += configure_file(
  output: 'static.data.cpp',
  input: 'static.data.cpp.in',
  configuration: {
    'FORMATS_MD': fs.read(meson.project_source_root() / 'doc/FORMATS.md'),
    'METRICS_YAML': fs.read(meson.project_source_root() / 'doc/METRICS.yaml'),
  },
)

_deps = [libdw_dep, libelf_dep, lzma_dep, openmp_dep, xerces_dep, xxhash_dep, yaml_cpp_dep]

_srcs += common_lean_srcs
_deps += common_lean_deps

hpcprof = executable(
  'hpcprof',
  version_cpp,
  _srcs,
  'main.cpp',
  'mpi/standalone.cpp',
  implicit_include_directories: false,
  dependencies: [_deps],
  install: true,
)

if get_option('hpcprof_mpi').enable_auto_if(mpi_dep.found()).enabled()
  hpcprof_mpi = executable(
    'hpcprof-mpi',
    version_cpp,
    _srcs,
    'main-mpi.cpp',
    'mpi.cpp',
    'tree.cpp',
    implicit_include_directories: false,
    dependencies: [_deps, mpi_dep],
    install: true,
  )
endif
