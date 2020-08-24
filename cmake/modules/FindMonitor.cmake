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

find_library(Monitor_LIBRARY NAMES monitor
             DOC "Location of the libmonitor library")
set(_all_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
find_library(Monitor_LIBRARY_STATIC NAMES monitor
             DOC "Location of the libmonitor static library")
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_all_library_suffixes})
unset(_all_library_suffixes)
find_library(Monitor_WRAP_LIBRARY NAMES monitor_wrap
             DOC "Location of the libmonitor wrap library")
find_path(Monitor_INCLUDE_DIR NAMES monitor.h
          DOC "Location of the include directory for libmonitor")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Monitor
  REQUIRED_VARS Monitor_LIBRARY Monitor_WRAP_LIBRARY Monitor_INCLUDE_DIR)

if(Monitor_FOUND)
  add_library(Monitor::Monitor UNKNOWN IMPORTED)
  set_target_properties(Monitor::Monitor PROPERTIES
                        IMPORTED_LOCATION "${Monitor_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${Monitor_INCLUDE_DIR}")
  if(Monitor_LIBRARY_STATIC)
    add_library(Monitor::Monitor_static UNKNOWN IMPORTED)
    set_target_properties(Monitor::Monitor_static PROPERTIES
                          IMPORTED_LOCATION "${Monitor_LIBRARY_STATIC}"
                          INTERFACE_INCLUDE_DIRECTORIES "${Monitor_INCLUDE_DIR}")
  endif()
  add_library(Monitor::wrap UNKNOWN IMPORTED)
  set_target_properties(Monitor::wrap PROPERTIES
                        IMPORTED_LOCATION "${Monitor_WRAP_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${Monitor_INCLUDE_DIR}")
endif()
