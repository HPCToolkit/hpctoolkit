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
  'Args.cpp',
)]

_gen = generator(find_program('stringify.sh', native: true),
                 arguments: ['@INPUT@'],
                 output: '@BASENAME@.h', capture: true)
_srcs += _gen.process(files('gpubin-analysis.txt'))

if not get_option('hpcstruct').disabled()
  hpcstruct_exe = executable('hpcstruct', _srcs,
    include_directories: tool_includes,
    link_with: [support_lib, banal_lib, analysis_lib],
    dependencies: [
      dependency('libbfd', required: false).partial_dependency(compile_args: true, includes: true),
      dependency('OpenMP', required: get_option('openmp')),
    ],
    install: true)
  all_apps += hpcstruct_exe
endif
if get_option('hpcstruct').enabled()
  assert(not is_disabler(hpcstruct_exe), 'hpcstruct required but disabled')
endif
summary('hpcstruct', is_variable('hpcstruct_exe') and not is_disabler(hpcstruct_exe), section: 'Applications', bool_yn: true)

if not get_option('devtools').disabled()
  dotgraph_exe = executable('dotgraph', files('DotGraph.cpp'),
    include_directories: tool_includes,
    dependencies: [
      dependency('dyninst', required: get_option('devtools'), disabler: true),
      dependency('OpenMP', required: get_option('openmp')),
    ],
    install: true, install_dir: get_option('libexecdir') / meson.project_name())
  all_apps += dotgraph_exe
endif
if get_option('devtools').enabled()
  assert(not is_disabler(dotgraph_exe), 'dotgraph required but disabled')
endif
summary('dotgraph', is_variable('dotgraph_exe') and not is_disabler(dotgraph_exe), section: 'Applications', bool_yn: true)
