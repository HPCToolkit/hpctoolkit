# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproflm = executable('hpcproflm', _srcs,
  common_srcs,
  implicit_include_directories: false,
  dependencies: [
    common_deps,
    libiberty_dep,
    lzma_dep,
    openmp_dep,
  ],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
