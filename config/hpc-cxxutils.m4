# -*-Mode: m4;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2002-2016, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

#############################################################################
#
# File:
#   $HeadURL$
#
# Description:
#   HPC custom C++ macros for use with autoconf.
#
#############################################################################

#---------------------------------------------------------------
# HPC_CXX_LIST, HPC_CC_LIST
#---------------------------------------------------------------

# Order of preference:
# 1. GNU-compatible compiler: GCC, Intel, Pathscale
# 2. Non-GNU compiler: PGI, Sun

define([HPC_CXX_LIST], [g++    \
                        icpc   \
                        pathCC \
                        pgCC   \
                        CC     \
                        c++    \
                        ])dnl

define([HPC_CC_LIST], [gcc    \
                       icc    \
                       pathcc \
                       pgcc   \
                       cc     \
                       ])dnl


#---------------------------------------------------------------
# HPC_CLEAR_CXXFLAGS, HPC_CLEAR_CCFLAGS
#---------------------------------------------------------------

AC_DEFUN([HPC_ENSURE_DEFINED_CXXFLAGS],
  [if test -z "$CXXFLAGS" ; then
     CXXFLAGS=""
   fi
  ])dnl

AC_DEFUN([HPC_ENSURE_DEFINED_CFLAGS],
  [if test -z "$CFLAGS" ; then
     CFLAGS=""
   fi
  ])dnl


#---------------------------------------------------------------
# HPC_DEF_IS_COMPILER_MAKING_STATIC_BINARIES()
#---------------------------------------------------------------

define([HPC_isCompilerMakingStaticBinaries],
       [MY_isCompilerMakingStaticBinaries $@])dnl

# HPC_is_statically_linked(): 
#   args: ($@: compiler + flags)
#   return 0 for yes, 1 otherwise

AC_DEFUN([HPC_DEF_IS_COMPILER_MAKING_STATIC_BINARIES],
  [HPC_DEF_IS_BINARY_STATICALLY_LINKED()
   MY_isCompilerMakingStaticBinaries()
   {
     my_compiler="$[]@"

     AC_LANG_PUSH([C++])
     CXX_ORIG="${CXX}"
     CXX="${my_compiler}"

     # should account for CXXFLAGS...

     # NOTE: do not cache (AC_CACHE_CHECK) b/c this may be called
     # multiple times!
     isStaticallyLinked="no"
     AC_MSG_CHECKING([whether '${CXX}' makes statically linked binaries])
     AC_LINK_IFELSE([AC_LANG_PROGRAM(
                      [
/*no includes needed*/
],
                      [char* hpctoolkit = "hpctoolkit;"])],
                    [if HPC_isBinaryStaticallyLinked conftest$EXEEXT ; then
		       isStaticallyLinked=yes
		     fi],
                    [isStaticallyLinked="no"])

     AC_MSG_RESULT([${isStaticallyLinked}])

     CXX="${CXX_ORIG}"
     AC_LANG_POP([C++])

     if test "${isStaticallyLinked}" = "yes" ; then
       return 0
     else
       return 1
     fi
   }])dnl


#---------------------------------------------------------------
# HPC_isBinaryStaticallyLinked
#---------------------------------------------------------------

# Given a binary, test whether it is statically linked.
# return 0 if it is, 1 otherwise
# args: ($1):

# Methods for checking whether a file is statically linked
#   'ldd'
#   'file'
#   'readelf -d' | grep NEEDED

AC_DEFUN([HPC_DEF_IS_BINARY_STATICALLY_LINKED],
  [HPC_isBinaryStaticallyLinked()
   {
     fnm=$[]1
  
     if ( ldd "$fnm" | ${GREP} ".so" >/dev/null 2>&1 ); then
       return 1 # false
     fi
     return 0 # true
   }])dnl


#---------------------------------------------------------------
# Check C++ standards-conforming access to C headers: #include <cheader>
#---------------------------------------------------------------

AC_DEFUN([HPC_CHECK_CXX_STDC_HEADERS],
  [AC_CACHE_CHECK(
     [whether ${CXX-CC} supports C++ style C headers (<cheader>)],
     [ac_cv_check_cxx_ctd_cheaders],
     [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM([
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <cassert>
#include <cerrno>

#include <cstdarg>
#include <cstring>
#include <cmath>
#include <csetjmp>
#include <csignal>

#include <cctype>
#include <climits>
#include <cfloat>
#include <clocale>
],
         [std::strcmp("fee fi", "fo fum");])],
       [ac_cv_check_cxx_ctd_cheaders="yes"],
       [ac_cv_check_cxx_ctd_cheaders="no"])])
   if test "$ac_cv_check_cxx_ctd_cheaders" = "no" ; then
     AC_DEFINE([NO_STD_CHEADERS], [], [Standard C headers])
   fi])dnl

# Here is an unreliable way of doing this...
#AC_CHECK_HEADERS([cstdlib cstdio cstring cassert],
#                 [AC_DEFINE([HAVE_STD_CHEADERS])],
#                 [AC_DEFINE([NO_STD_CHEADERS])])


#---------------------------------------------------------------
# Check if pthread.h causes a compile error b/c it uses __thread
# instead of __threadp.
#---------------------------------------------------------------

AC_DEFUN([HPC_CHECK_COMPILE_PTHREAD_H],
  [AC_CACHE_CHECK(
     [whether ${CXX-CC} compiles pthread.h (__thread vs. __threadp)],
     [ac_cv_check_cxx_pthread_h],
     [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM([
#include <pthread.h>
],
         [])],
       [ac_cv_check_cxx_pthread_h="yes"],
       [ac_cv_check_cxx_pthread_h="no"])]
     )
   if test "$ac_cv_check_cxx_pthread_h" = "no" ; then
     AC_DEFINE([__thread], [__threadp], [Fix pthread.h])
   fi])dnl


#---------------------------------------------------------------
# HPCcxxcmp
#---------------------------------------------------------------

# Macro for MYcxxcmp
# args: ($1): (proposed_compiler)
define([HPCcxxcmp], [MYcxxcmp $CXX $@])dnl
define([HPCcccmp], [MYcxxcmp $CC $@])dnl


# Given the current compiler and a list of proposed compiler names,
# return 0 if there is a match, 1 otherwise
# args: ($1 $2...): (current_compiler, proposed_compiler...)
AC_DEFUN([HPC_DEF_CXXCMP],
  [MYcxxcmp()
   {
     mycxx=$[]1
     shift
  
     while test $[]# -ge 1 ; do
       if ( echo "${mycxx}" | ${GREP} "$[]1\$" >/dev/null 2>&1 ); then
         return 0
       fi
       shift
     done
     return 1
   }])dnl


#---------------------------------------------------------------
# Check for PAPI
# (Use PAPI's high-level interface which is self-initializing)
#---------------------------------------------------------------

# FIXME: papi 3.5.0 has a bug that doesn't set rpath correctly
#        for now assume the .a is an appropriate test

# HPC_check_cxx_papi_link()
#   args: ($1: include-flags, $2: lib-path)
#   return 0 for success, 1 otherwise
AC_DEFUN([HPC_DEF_CHECK_CXX_PAPI_LINK],
  [HPC_check_cxx_papi_link()
   {
     myincflg="$[]1"
     mylibpath="$[]2"
     CXXFLAGS_ORIG=${CXXFLAGS}
     CXXFLAGS="${CXXFLAGS} ${myincflg}"
     LDFLAGS_ORIG=${LDFLAGS}
     LIBS_ORIG=${LIBS}
     LIBS="${LIBS} ${mylibpath}/libpapi.a"
     #LIBS="${LIBS} -L${mylibpath} -lpapi"

     # NOTE: do not cache (AC_CACHE_CHECK) b/c this may be called
     # multiple times!
     CAN_PAPI_LINK="no"
     AC_MSG_CHECKING([whether ${CXX-CC} can link with PAPI using path ${mylibpath}])
     AC_LINK_IFELSE([AC_LANG_PROGRAM([
#include <papi.h>
],
                      [PAPI_is_initialized()])],
                    [CAN_PAPI_LINK="yes"],
                    [CAN_PAPI_LINK="no"])

     AC_MSG_RESULT([${CAN_PAPI_LINK}])

     CXXFLAGS="${CXXFLAGS_ORIG}"
     LDFLAGS="${LDFLAGS_ORIG}"
     LIBS="${LIBS_ORIG}"

     if test "$CAN_PAPI_LINK" = "yes" ; then
       return 0
     else
       return 1
     fi
   }])dnl

