# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_srcs = files('main.cpp')

hpctracedump = executable(
  'hpctracedump',
  _srcs,
  common_srcs,
  dependencies: [libiberty_dep, lzma_dep, common_deps],
)

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
