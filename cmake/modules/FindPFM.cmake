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

# As a shorthand, you can use PAPI_ROOT instead of PFM_ROOT
if(NOT DEFINED PFM_ROOT)
  if(DEFINED PAPI_ROOT)
    set(PFM_ROOT "${PAPI_ROOT}")
  elseif(ENV{PAPI_ROOT})
    set(PFM_ROOT "$ENV{PAPI_ROOT}")
  endif()
endif()

find_library(PFM_LIBRARY NAMES pfm
             DOC "Location of the libpfm library")
find_path(PFM_INCLUDE_DIR NAMES pfmlib.h
          DOC "Location of the include directory for libPFM")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PFM
                                  REQUIRED_VARS PFM_LIBRARY PFM_INCLUDE_DIR)

if(PFM_FOUND)
  add_library(PFM::PFM UNKNOWN IMPORTED)
  set_target_properties(PFM::PFM PROPERTIES
                        IMPORTED_LOCATION "${PFM_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${PFM_INCLUDE_DIR}")
endif()
