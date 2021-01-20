# * BeginRiceCopyright *****************************************************
#
# $HeadURL$
# $Id$
#
# --------------------------------------------------------------------------
# Part of HPCToolkit (hpctoolkit.org)
#
# Information about sources of support for research and development of
# HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
# --------------------------------------------------------------------------
#
# Copyright ((c)) 2002-2021, Rice University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Rice University (RICE) nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# This software is provided by RICE and contributors "as is" and any
# express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular
# purpose are disclaimed. In no event shall RICE or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or
# business interruption) however caused and on any theory of liability,
# whether in contract, strict liability, or tort (including negligence
# or otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#
# ******************************************************* EndRiceCopyright *

_srcs = [files(
  'main.cpp',
  'code-ranges.cpp',
  'function-entries.cpp',
  'intervals.cpp',
  'sections.cpp',
  'server.cpp',
  'eh-frames.cpp',
)]
_deps = []

if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
  _srcs += files('x86-process-ranges.cpp', 'amd-xop.c')
  _deps += dependency('xed', required: false, disabler: true)
elif host_machine.cpu_family() == 'aarch64'
  _srcs += files('arm-process-ranges.cpp')
else
  _srcs += files('generic-process-ranges.cpp')
endif

if not get_option('hpcrun').disabled()
  hpcfnbounds_exe = executable('hpcfnbounds', _srcs,
    include_directories: tool_includes,
    link_with: [prof_lean_lib],
    dependencies: [
      dependency('dyninst', required: false, disabler: true),
      dependency('OpenMP', required: get_option('openmp')),
      dependency('libdwarf', required: false, disabler: true)
        .partial_dependency(compile_args: true, includes: true),
      cc.find_library('dl'),
    ] + _deps,
    install: true, install_dir: get_option('libexecdir') / meson.project_name())
  all_apps += hpcfnbounds_exe
endif
summary('hpcfnbounds', is_variable('hpcfnbounds_exe') and not is_disabler(hpcfnbounds_exe), section: 'Applications', bool_yn: true)
