# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_srcs = files('fnbounds.c', 'scan.c', 'server.c')

hpcfnbounds = executable(
  'hpcfnbounds',
  version_cpp,
  _srcs,
  implicit_include_directories: false,
  dependencies: [libdw_dep, libelf_dep],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name(),
)
