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

_srcs = files(
  'Args.cpp',
  'BaseDataFile.cpp',
  'DataCompressionLayer.cpp',
  'DataOutputFileStream.cpp',
  'DataSocketStream.cpp',
  'DBOpener.cpp',
  'FilteredBaseData.cpp',
  'LargeByteBuffer.cpp',
  'MergeDataFiles.cpp',
  'ProcessTimeline.cpp',
  'ProgressBar.cpp',
  'Server.cpp',
  'SpaceTimeDataController.cpp',
  'TraceDataByRank.cpp',
  'VersatileMemoryPage.cpp',
  'main.cpp',
)

if not get_option('hpcserver').disabled()
  hpcserver_exe = executable('hpcserver', _srcs, files(
      'Communication-SingleThreaded.cpp',
    ),
    include_directories: tool_includes,
    dependencies: [
      analysis_dep,
      dependency('zlib', required: get_option('hpcserver'), disabler: true),
    ],
    install: true)

  # NOTE: Removed due to usage of deprecated MPI C++ interface
  # hpcserver_mpi_exe = executable('hpcserver-mpi', _srcs, files(
  #     'Communication-MPI.cpp',
  #     'Slave.cpp',
  #   ),
  #   cpp_args: '-DMPICH_IGNORE_CXX_SEEK',
  #   include_directories: tool_includes,
  #   dependencies: [
  #     analysis_dep,
      # support_dep, analysis_dep,
  #     dependency('zlib', required: get_option('hpcserver'), disabler: true),
  #     dependency('MPI', language: 'cpp', required: get_option('mpi'), disabler: true),
  #   ],
  #   install: true)
  hpcserver_mpi_exe = disabler()
  all_apps += [hpcserver_exe, hpcserver_mpi_exe]
endif
if get_option('hpcserver').enabled()
  assert(not is_disabler(hpcserver_exe), 'hpcserver required but disabled')
endif
if get_option('hpcserver').enabled() and get_option('mpi').enabled()
  assert(not is_disabler(hpcserver_mpi_exe), 'hpcserver-mpi required but disabled')
endif
summary('hpcserver', is_variable('hpcserver_exe') and not is_disabler(hpcserver_exe), section: 'Applications', bool_yn: true)
summary('hpcserver-mpi', is_variable('hpcserver_mpi_exe') and not is_disabler(hpcserver_mpi_exe), section: 'Applications', bool_yn: true)
