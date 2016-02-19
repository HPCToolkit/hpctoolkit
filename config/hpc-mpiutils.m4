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
#   HPC custom MPI macros for use with autoconf.
#
#############################################################################

#---------------------------------------------------------------
# HPC_MPICXX_LIST, HPC_MPICC_LIST
#---------------------------------------------------------------

define([HPC_MPICXX_LIST], [mpicxx   \
                           mpiCC \
                          ])dnl

define([HPC_MPICC_LIST], [mpicc   \
                         ])dnl


#---------------------------------------------------------------
# HPC_DEF_CHECK_MPICXX()
#---------------------------------------------------------------

# HPC_check_mpicxx(): 
#   args: ($1: compiler)
#   return 0 for success, 1 otherwise

AC_DEFUN([HPC_DEF_CHECK_MPICXX],
  [HPC_check_mpicxx()
   {
     my_mpicxx="$[]1"

     AC_LANG_PUSH([C++])
     CXX_ORIG=${CXX}
     CXX=${my_mpicxx}

     # NOTE: do not cache (AC_CACHE_CHECK) b/c this may be called
     # multiple times!
     CAN_MPICXX_LINK="no"
     AC_MSG_CHECKING([whether MPI C++ compiler ${CXX} works correctly])
     AC_LINK_IFELSE([AC_LANG_PROGRAM(
                      [
#include <mpi.h>
#include <iostream>
#include <string>
],
                      [std::string hpctoolkit = "hpctoolkit;"])],
                    [CAN_MPICXX_LINK="yes"],
                    [CAN_MPICXX_LINK="no"])

     AC_MSG_RESULT([${CAN_MPICXX_LINK}])

     CXX="${CXX_ORIG}"
     AC_LANG_POP([C++])

     if test "$CAN_MPICXX_LINK" = "yes" ; then
       return 0
     else
       return 1
     fi
   }])dnl



#---------------------------------------------------------------
# HPC_PROG_MPICXX
#---------------------------------------------------------------

# HPC_PROG_MPICXX(): check for MPI C++ compiler
# args: ($1): (proposed-compiler-list)
AC_DEFUN([HPC_PROG_MPICXX],
  [my_compiler_list="$1"
   HPC_DEF_CHECK_MPICXX()
   AC_CACHE_CHECK(
     [for MPI C++ compiler],
     [ac_cv_path_MPICXX],
     [AC_PATH_PROGS_FEATURE_CHECK(
        [MPICXX],
        ${my_compiler_list},
        [if HPC_check_mpicxx ${ac_path_MPICXX} ; then
           ac_cv_path_MPICXX=$ac_path_MPICXX
           ac_path_MPICXX_found=:
         fi],
        [ac_cv_path_MPICXX=no])
     ])

   # AC_PATH_PROGS_FEATURE_CHECK does not call HPC_check_mpicxx if
   # MPICXX was set in the environment.
   if test "${ac_path_MPICXX_found}" != ":" -a "${ac_cv_path_MPICXX}" != "no" ; then
     if ! HPC_check_mpicxx ${ac_cv_path_MPICXX} ; then
       ac_cv_path_MPICXX=no;
     fi
   fi

   # Determine final value of MPICXX
   if test "${ac_cv_path_MPICXX}" != "no" ; then
     MPICXX=${ac_cv_path_MPICXX}
   else 
     MPICXX=
   fi

   AC_ARG_VAR([MPICXX], [MPI C++ compiler])
  ])dnl



#AC_CACHE_CHECK([for MPI C compiler], [ac_cv_path_M4], 
#  [AC_PATH_PROGS_FEATURE_CHECK([M4], [m4 gm4], 
#    [[m4out=‘echo ’changequote([,])indir([divnum])’ | $ac_path_M4‘ 
#      test "x$m4out" = x0 \ 
#      && ac_cv_path_M4=$ac_path_M4 ac_path_M4_found=:]], 
#    [AC_MSG_ERROR([could not find m4 that supports indir])])
# ]) 
#
#AC_SUBST([M4], [$ac_cv_path_M4])
