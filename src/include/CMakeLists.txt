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

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckVariableExists)
include(TestBigEndian)
include(CheckTypeSize)

test_big_endian(big_endian)

# Clear the output file, each stanza below will append something to it
set(config_file ${CMAKE_CURRENT_BINARY_DIR}/hpctoolkit-config.h.in)
file(WRITE ${config_file} "")

file(APPEND ${config_file} "#cmakedefine USE_SYSTEM_BYTESWAP\n")
set(USE_SYSTEM_BYTESWAP NO)
if(NOT big_endian)
  check_symbol_exists(bswap_64 "byteswap.h" USE_SYSTEM_BYTESWAP)
endif()

file(APPEND ${config_file} "#cmakedefine BINUTILS_234\n")
if("${LibBFD_VERSION}" VERSION_GREATER_EQUAL 2.34)
  set(BINUTILS_234 YES)
else()
  set(BINUTILS_234 NO)
endif()

# TODO: Change this to an eoption
file(APPEND ${config_file} "#cmakedefine DATACENTRIC_TRACE\n")
set(DATACENTRIC_TRACE OFF)

# Test whether Dyninst uses Instruction::Ptr
file(APPEND ${config_file} "#cmakedefine DYNINST_INSTRUCTION_PTR\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dyninst_iptr.cpp [[
  #include <map>
  #include <iostream>
  #include <Symtab.h>
  #include <Instruction.h>
  #include <CFG.h>
  using namespace Dyninst;
  using namespace InstructionAPI;
  using namespace ParseAPI;
  int main() {
    std::map<Offset, Instruction::Ptr> imap;
    ((Block*)NULL)->getInsns(imap);
    std::cout << imap.begin()->second->size();
  }
]])
try_compile(DYNINST_INSTRUCTION_PTR ${CMAKE_CURRENT_BINARY_DIR}/dyninst_iptr
            ${CMAKE_CURRENT_BINARY_DIR}/dyninst_iptr.cpp
            LINK_LIBRARIES instructionAPI TBB::tbb)

# Test whether Dyninst supports CUDA
file(APPEND ${config_file} "#cmakedefine DYNINST_USE_CUDA\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dyninst_cuda.cpp [[
  #include <iostream>
  #include <Symtab.h>
  int main() {
    std::cout << (long) Dyninst::Arch_cuda;
  }
]])
try_compile(DYNINST_USE_CUDA ${CMAKE_CURRENT_BINARY_DIR}/dyninst_cuda
            ${CMAKE_CURRENT_BINARY_DIR}/dyninst_cuda.cpp
            LINK_LIBRARIES symtabAPI TBB::tbb)

# TODO: Change this out for an actual test
file(APPEND ${config_file} "#cmakedefine DYNINST_USE_LIBDW\n")
set(DYNINST_USE_LIBDW ON)

file(APPEND ${config_file} "#cmakedefine ENABLE_CLOCK_REALTIME\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/has_clock_realtime.c [[
  #include <sys/syscall.h>
  #include <signal.h>
  #include <time.h>
  #include <unistd.h>
  #ifndef sigev_notify_thread_id
  #define sigev_notify_thread_id _sigev_un._tid
  #endif
  int main() {
    struct sigevent sigev;
    timer_t timerid;
    sigev.sigev_notify = SIGEV_THREAD_ID;
    sigev.sigev_signo = SIGRTMIN + 3;
    sigev.sigev_value.sival_ptr = &timerid;
    sigev.sigev_notify_thread_id = syscall(SYS_gettid);
    return timer_create(CLOCK_REALTIME, &sigev, &timerid);
  }
]])
try_compile(ENABLE_CLOCK_REALTIME ${CMAKE_CURRENT_BINARY_DIR}/has_clock_realtime
            ${CMAKE_CURRENT_BINARY_DIR}/has_clock_realtime.c)

file(APPEND ${config_file} "#cmakedefine ENABLE_CLOCK_CPUTIME\n")
if(ENABLE_CLOCK_REALTIME)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/has_clock_cputime.c [[
    #include <sys/time.h>
    #include <time.h>
    int main() {
      clockid_t clock = CLOCK_THREAD_CPUTIME_ID;
      return 0;
    }
  ]])
  try_compile(ENABLE_CLOCK_CPUTIME ${CMAKE_CURRENT_BINARY_DIR}/has_clock_cputime
              ${CMAKE_CURRENT_BINARY_DIR}/has_clock_cputime.c)
else()
  set(ENABLE_CLOCK_CPUTIME OFF)
endif()

# TODO: This should depend on an option and some tests for Dyninst
file(APPEND ${config_file} "#cmakedefine ENABLE_OPENMP\n")
set(ENABLE_OPENMP ON)
file(APPEND ${config_file} "#cmakedefine ENABLE_OPENMP_SYMTAB\n")
set(ENABLE_OPENMP_SYMTAB ON)

# TODO: This should be set based on an option
file(APPEND ${config_file} "#cmakedefine ENABLE_VG_ANNOTATIONS\n")
set(ENABLE_VG_ANNOTATIONS ON)

# TODO: I've never used this one, maybe nobody should?
file(APPEND ${config_file} "#cmakedefine ENABLE_XOP\n")
set(ENABLE_XOP ON)

# CUDA is weird, for now just stick with a header test
file(APPEND ${config_file} "#cmakedefine HAVE_CUPTI_H\n")
set(HAVE_CUPTI_H FALSE)
if(TARGET CUDA::cuPTI)
  set(CMAKE_REQUIRED_LIBRARIES CUDA::cuPTI)
  check_include_files("cupti.h" HAVE_CUPTI_H LANGUAGE CXX)
  unset(CMAKE_REQUIRED_LIBRARIES)
endif()

file(APPEND ${config_file} "#cmakedefine HAVE_DLFCN_H\n")
check_include_files("dlfcn.h" HAVE_DLFCN_H LANGUAGE C)
file(APPEND ${config_file} "#cmakedefine HAVE_INTTYPES_H\n")
check_include_files("inttypes.h" HAVE_INTTYPES_H LANGUAGE C)

# TODO: This should be replaced with something more useful
file(APPEND ${config_file} "#cmakedefine HAVE_OS_MULTILIB\n")
set(HAVE_OS_MULTILIB FALSE)

file(APPEND ${config_file} "#cmakedefine HAVE_STDINT_H\n")
check_include_files("stdint.h" HAVE_STDINT_H LANGUAGE C)
file(APPEND ${config_file} "#cmakedefine HAVE_SYS_TYPES_H\n")
check_include_files("sys/types.h" HAVE_SYS_TYPES_H LANGUAGE C)

# TODO: The following only check the C side, not the C++ side.
file(APPEND ${config_file} "#cmakedefine HAVE_UINT\n")
file(APPEND ${config_file} "#cmakedefine HAVE_UINT_LANG_C\n")
check_variable_exists(uint HAVE_UINT_LANG_C)
set(HAVE_UINT ${HAVE_UINT_LANG_C})
file(APPEND ${config_file} "#cmakedefine HAVE_ULONG\n")
file(APPEND ${config_file} "#cmakedefine HAVE_ULONG_LANG_C\n")
check_variable_exists(ulong HAVE_ULONG_LANG_C)
set(HAVE_ULONG ${HAVE_ULONG_LANG_C})
file(APPEND ${config_file} "#cmakedefine HAVE_USHORT\n")
file(APPEND ${config_file} "#cmakedefine HAVE_USHORT_LANG_C\n")
check_variable_exists(ushort HAVE_USHORT_LANG_C)
set(HAVE_USHORT ${HAVE_USHORT_LANG_C})

file(APPEND ${config_file} "#cmakedefine HOST_BIG_ENDIAN\n")
set(HOST_BIG_ENDIAN ${big_endian})
file(APPEND ${config_file} "#cmakedefine HOST_LITTLE_ENDIAN\n")
if(big_endian)
  set(HOST_LITTLE_ENDIAN FALSE)
else()
  set(HOST_LITTLE_ENDIAN TRUE)
endif()

# TODO: Figure out the correct values from CMAKE_SYSTEM_PROCESSOR, which is
# based on `uname -m` (since CMake doesn't munge the names for us)
file(APPEND ${config_file} "#cmakedefine HOST_CPU_ARM64\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(HOST_CPU_ARM64 TRUE)
else()
  set(HOST_CPU_ARM64 FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_CPU_IA64\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "ia64")
  set(HOST_CPU_IA64 TRUE)
else()
  set(HOST_CPU_IA64 FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_CPU_PPC\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64")
  set(HOST_CPU_PPC TRUE)
else()
  set(HOST_CPU_PPC FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_CPU_x86\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
  set(HOST_CPU_x86 TRUE)
else()
  set(HOST_CPU_x86 FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_CPU_x86_64\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  set(HOST_CPU_x86_64 TRUE)
else()
  set(HOST_CPU_x86_64 FALSE)
endif()

# TODO: Figure out the correct values from CMAKE_SYSTEM_NAME, which is
# based on `uname -s` (since CMake doesn't munge the names for us)
file(APPEND ${config_file} "#cmakedefine HOST_OS_IRIX\n")
set(HOST_OS_IRIX FALSE)
file(APPEND ${config_file} "#cmakedefine HOST_OS_LINUX\n")
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(HOST_OS_LINUX TRUE)
else()
  set(HOST_OS_LINUX FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_OS_MAXOS\n")
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(HOST_OS_MAXOS TRUE)
else()
  set(HOST_OS_MAXOS FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_OS_SOLARIS\n")
if(CMAKE_SYSTEM_NAME MATCHES "SunOS")
  set(HOST_OS_SOLARIS TRUE)
else()
  set(HOST_OS_SOLARIS FALSE)
endif()
file(APPEND ${config_file} "#cmakedefine HOST_OS_Tru64\n")
set(HOST_OS_Tru64 FALSE)
file(APPEND ${config_file} "#cmakedefine HOST_PLATFORM_MIPS64LE_LINUX\n")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "mips64" AND CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(HOST_PLATFORM_MIPS64LE_LINUX TRUE)
else()
  set(HOST_PLATFORM_MIPS64LE_LINUX FALSE)
endif()

# TODO: Make this a proper option/test
file(APPEND ${config_file} "#cmakedefine HOST_SYSTEM_IBM_BLUEGENE\n")
set(HOST_SYSTEM_IBM_BLUEGENE FALSE)

# TODO: Base this on some Git command's output
file(APPEND ${config_file} "#cmakedefine HPCTOOLKIT_GIT_VERSION\n")
set(HPCTOOLKIT_GIT_VERSION "(Git TODO)")

file(APPEND ${config_file} "#cmakedefine HPCTOOLKIT_INSTALL_PREFIX\n")
set(HPCTOOLKIT_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# TODO: This one is going to be a headache at some point
file(APPEND ${config_file} "#cmakedefine HPCTOOLKIT_SPACK_SPEC\n")
set(HPCTOOLKIT_SPACK_SPEC "(Spack TODO)")

file(APPEND ${config_file} "#cmakedefine HPCTOOLKIT_VERSION\n")
set(HPCTOOLKIT_VERSION "${PROJECT_VERSION}")
file(APPEND ${config_file} "#cmakedefine HPCTOOLKIT_VERSION_STRING\n")
set(HPCTOOLKIT_VERSION_STRING "A member of ${PROJECT_NAME}, version ${PROJECT_VERSION}")

# TODO: Make this a test of some kind, eventually
file(APPEND ${config_file} "#cmakedefine NO_STD_CHEADERS\n")
set(NO_STD_CHEADERS FALSE)

file(APPEND ${config_file} "#cmakedefine OPT_HAVE_CUDA\n")
if(TARGET CUDA::Toolkit)
  set(OPT_HAVE_CUDA TRUE)
else()
  set(OPT_HAVE_CUDA FALSE)
endif()

file(APPEND ${config_file} "#cmakedefine SIZEOF_VOIDP\n")
check_type_size("void*" SIZEOF_VOIDP)

# TODO: Make this an option instead
file(APPEND ${config_file} "#cmakedefine USE_LIBUNW\n")
if(TARGET LibUnwind::LibUnwind)
  set(USE_LIBUNW TRUE)
else()
  set(USE_LIBUNW FALSE)
endif()

file(APPEND ${config_file} "#cmakedefine USE_ROCM\n")
if(TARGET ROCm::ROCm)
  set(USE_ROCM TRUE)
else()
  set(USE_ROCM FALSE)
endif()

configure_file(${config_file} ${CMAKE_CURRENT_BINARY_DIR}/hpctoolkit-config.h)


