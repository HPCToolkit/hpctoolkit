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

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(hpctoolkit
        VERSION 2020.08
        HOMEPAGE_URL "http://hpctoolkit.org"
        LANGUAGES C CXX)

# TODO: Hunt down the Git version and Spack spec. Generally not needed.

# Define some system-wide options up here.
option(ENABLE_ALL_STATIC "Output static binaries for MPI-supporting applications." OFF)
option(ENABLE_DEV_TOOLS "Enable extra tools useful for debugging." OFF)
option(EXTERNAL_STRIP "Strip extrnal libraries as part of installation." ON)

# We might need C11 nowadays. Just force it for now.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# TODO: Autotools issues a whole bunch of sanity checks for headers and types. In particular:
#  - stdlib.h, stdarg.h, string.h, float.h
#  - unistd.h, inttypes.h (but only for C++?)
#  - void* (for C)
#  - cxxabi.h, cstdlib, cstddef, cstdio, cassert, cerrno, cstdarg, cstring, cmath, csetjmp, csignal,
#    cctype, climits, cfloat, clocale (for C++)
#  - sizeof void* (but only for C++?)
#  - ushort, uint, ulong
#  - pthread.h (more than preprocessor)
# None of these are probably nessesary on a modern system.

# TODO: Autotools has a long switch statement configuring the unwinder usage and host flags.
# Some of it is handled by CMake (CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR), but the rest is tricky.

# If we're running on a little-endian machine, we'll have to swap bytes. Try to use byteswap.h if we can.
include(TestBigEndian)
test_big_endian(big_endian)
if(NOT big_endian)
  try_compile(HAS_bswap_64 ${CMAKE_BINARY_DIR}/cmaketest_byteswap_64 ${CMAKE_SOURCE_DIR}/cmake/byteswap-test.c)
endif()

# Warn if we're not running a GNU compiler with a sufficient version
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  message(WARNING "Compilers other than GCC are not supported!")
elseif(CMAKE_C_COMPILER_VERSION VERSION_LESS 5 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5)
  message(WARNING "GCC before 5.0 is not supported!")
endif()

# TODO: Fix the compiler wrapper thingy for Spack builds

# TODO: Find libstdc++, for hpcfnbounds apparently. Not used for hpcfnbounds2.

# Enable extended C++ alignment if possible, for dyninst
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-faligned-new HAS_faligned_new)
if(HAS_faligned_new)
  # TODO: Check that this actually works right, I've had issues in the past
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -faligned-new")
endif()

# Enable OpenMP.
find_package(OpenMP)
# TODO: Handle finding the OpenMP directory. OpenMP_C_LIBRARIES is dangerous since it also lists libpthread.so.
