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

# TBB ships both a CMake config file and a pkg-config file, so we use
# whatever we find first.

find_package(TBB QUIET CONFIG)
if(TBB_FOUND)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(TBB CONFIG_MODE)
  return()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_TBB QUIET tbb)
  if(PC_TBB_FOUND)
    set(TBB_VERSION ${PC_TBB_VERSION})
  endif()
endif()

find_library(TBB_LIBRARY NAMES tbb
             DOC "Location of the TBB main library")
find_library(TBB_MALLOC_LIBRARY NAMES tbbmalloc
             DOC "Location of the TBB malloc library")
find_library(TBB_MPROXY_LIBRARY NAMES tbbmalloc_proxy
             DOC "Location of the TBB malloc proxy library")
find_path(TBB_INCLUDE_DIR NAMES tbb/concurrent_hash_map.h
          DOC "Location of the include directory for TBB")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TBB
  REQUIRED_VARS TBB_LIBRARY TBB_MALLOC_LIBRARY TBB_MPROXY_LIBRARY
                TBB_INCLUDE_DIR)

if(TBB_FOUND)
  add_library(TBB::tbb UNKNOWN IMPORTED)
  set_target_properties(TBB::tbb PROPERTIES
                        IMPORTED_LOCATION "${TBB_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIR}")
  add_library(TBB::tbbmalloc UNKNOWN IMPORTED)
  set_target_properties(TBB::tbbmalloc PROPERTIES
                        IMPORTED_LOCATION "${TBB_MALLOC_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIR}")
  add_library(TBB::tbbmalloc_proxy UNKNOWN IMPORTED)
  set_target_properties(TBB::tbbmalloc_proxy PROPERTIES
                        IMPORTED_LOCATION "${TBB_MPROXY_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIR}"
                        INTERFACE_LINK_LIBRARYS TBB::tbbmalloc)
endif()
