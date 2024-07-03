# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_srcs = files('Args.cpp', 'main.cpp')

hpcproftt = executable(
  'hpcproftt',
  version_cpp,
  _srcs,
  common_srcs,
  implicit_include_directories: false,
  dependencies: [common_deps, iga_dep, igc_dep, libdw_dep, libelf_dep, lzma_dep, xerces_dep],
)

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
