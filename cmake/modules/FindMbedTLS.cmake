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

find_library(MbedTLS_CRYPTO_LIBRARY NAMES mbedcrypto
             DOC "Location of the libmbedcrypto library")
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
find_library(MbedTLS_CRYPTO_LIBRARY_STATIC NAMES mbedcrypto
             DOC "Location of the libmbedcrypto static library")
find_path(MbedTLS_INCLUDE_DIR NAMES mbedtls/md5.h
          DOC "Location of the include directory for libmbedtls")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MbedTLS
  REQUIRED_VARS MbedTLS_CRYPTO_LIBRARY MbedTLS_INCLUDE_DIR)

if(MbedTLS_FOUND)
  add_library(MbedTLS::Crypto UNKNOWN IMPORTED)
  set_target_properties(MbedTLS::Crypto PROPERTIES
                        IMPORTED_LOCATION "${MbedTLS_CRYPTO_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}")
  add_library(MbedTLS::Crypto_static UNKNOWN IMPORTED)
  set_target_properties(MbedTLS::Crypto_static PROPERTIES
                        IMPORTED_LOCATION "${MbedTLS_CRYPTO_LIBRARY_STATIC}"
                        INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}")
endif()
