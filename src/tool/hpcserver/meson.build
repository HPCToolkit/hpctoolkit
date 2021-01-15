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
# Copyright ((c)) 2002-2020, Rice University
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

hpcserver_exe = executable('hpcserver', _srcs, files(
    'Communication-SingleThreaded.cpp',
  ),
  include_directories: tool_includes,
  link_with: [support_lib],
  dependencies: [zlib_d_dep, hpcserver_opt_dep],
  install: true)
summary('hpcserver', not is_disabler(hpcserver_exe), section: 'Applications', bool_yn: true)
all_apps += hpcserver_exe

if get_option('hpcserver').enabled()
  assert(not is_disabler(hpcserver_exe), 'hpcserver required but disabled')
endif

hpcserver_mpi_exe = executable('hpcserver-mpi', _srcs, files(
    'Communication-MPI.cpp',
    'Slave.cpp',
  ),
  cpp_args: '-DMPICH_IGNORE_CXX_SEEK',
  include_directories: tool_includes,
  link_with: [support_lib],
  dependencies: [zlib_d_dep, mpi_cpp_d_dep, hpcserver_opt_dep],
  install: true)
summary('hpcserver-mpi', not is_disabler(hpcserver_mpi_exe), section: 'Applications', bool_yn: true)
all_apps += hpcserver_mpi_exe

if get_option('hpcserver').enabled() and mpi_cpp_d_dep.found()
  assert(not is_disabler(hpcserver_mpi_exe), 'hpcserver-mpi required but disabled')
endif
