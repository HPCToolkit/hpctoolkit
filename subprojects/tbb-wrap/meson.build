# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

project('oneTBB', version: '2021.13.0', meson_version: '>=1.1.0')

cmake = import('cmake')
tbb_proj = cmake.subproject('tbb')

tbb_dep = tbb_proj.dependency('tbb')
tbbmalloc_dep = tbb_proj.dependency('tbbmalloc')
tbbmalloc_proxy_dep = tbb_proj.dependency('tbbmalloc_proxy')

# Before Meson 1.6.x Meson propagates a --version-script linker flag, which causes undesirable link
# failures when trying to use this subproject. Avoid these issues by stripping the link arguments
# before passing the dependency up to the user(s).
# See also https://github.com/mesonbuild/meson/pull/13422.
if not meson.version().version_compare('>=1.6.0')
  tbb_dep = tbb_dep.partial_dependency(
    compile_args: true,
    includes: true,
    links: true,
    sources: true,
  )
  tbbmalloc_dep = tbbmalloc_dep.partial_dependency(
    compile_args: true,
    includes: true,
    links: true,
    sources: true,
  )
  tbbmalloc_proxy_dep = tbbmalloc_proxy_dep.partial_dependency(
    compile_args: true,
    includes: true,
    links: true,
    sources: true,
  )
endif

meson.override_dependency('tbb', tbb_dep)
meson.override_dependency('tbbmalloc', tbbmalloc_dep)
meson.override_dependency('tbbmalloc_proxy', tbbmalloc_proxy_dep)
