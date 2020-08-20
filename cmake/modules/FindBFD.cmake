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

# As a shorthand, you can use Binutils_ROOT instead of BFD_ROOT
if(NOT DEFINED BFD_ROOT)
  if(DEFINED Binutils_ROOT)
    set(BFD_ROOT "${Binutils_ROOT}")
  elseif(ENV{Binutils_ROOT})
    set(BFD_ROOT "$ENV{Binutils_ROOT}")
  endif()
endif()

find_library(BFD_LIBRARY NAMES bfd
             DOC "Location of the libbfd library")
find_path(BFD_INCLUDE_DIR NAMES bfd.h
          DOC "Location of the include directory for libbfd")

if(BFD_INCLUDE_DIR)
  # We decipher the version from the header, and just test lines that we care
  # about. If anyone needs another line, add a stanza here.

  # Test for >= 2.34
  file(WRITE "${CMAKE_BINARY_DIR}/CMakeFiles/FindBFD/bfd_234.c" [[
    #include <bfd.h>
    int main() {
      return bfd_section_vma((asection*)0);
    }
  ]])
  try_compile(BFD_IS_GE_234 "${CMAKE_BINARY_DIR}/CMakeFiles/FindBFD/bfd_234"
              SOURCES "${CMAKE_BINARY_DIR}/CMakeFiles/FindBFD/bfd_234.c"
              CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${BFD_INCLUDE_DIR}")

  if(BFD_IS_GE_234)
    set(BFD_VERSION 2.34)
  else()
    set(BFD_VERSION 2.33)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BFD
                                  REQUIRED_VARS BFD_LIBRARY BFD_INCLUDE_DIR
                                  VERSION_VAR BFD_VERSION)

if(BFD_FOUND)
  add_library(BFD::BFD UNKNOWN IMPORTED)
  set_target_properties(BFD::BFD PROPERTIES
                        IMPORTED_LOCATION "${BFD_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${BFD_INCLUDE_DIR}")
endif()
