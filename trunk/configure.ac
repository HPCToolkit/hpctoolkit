# -*-Mode: autoconf;-*-
# $Header$

## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002, Rice University 
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
#   $Source$
#
# Description:
#   autoconf input file for HPCToolkit.
#   *Process with autoconf to produce configure*
#
# Modification history:
#   2004/03/03 - Nathan Tallent
#
#############################################################################

include(config/hpc-cxxutils.m4)

#----------------------------------------------------------------------------
# Initialization: 
# Update to autoconf 2.59 and automake 1.8.3
# Require autoconf 2.54 because automake 1.7.6 needs it.  
#  - automake 1.7.6 is needed to generate Intel 7.1 dependencies correctly.
#  - automake 1.6.1 failed to generate separate lists of object files for
#    the two library versions. automake 1.7.5 handles the specification
#    correctly
#----------------------------------------------------------------------------

AC_INIT(HPCToolkit, 3.7.0)
AC_CONFIG_SRCDIR([src/include/HPCToolkitVersionInfo.h])
AC_CONFIG_AUX_DIR([config])

AC_PREREQ(2.59)
AM_INIT_AUTOMAKE(1.8.3)

#----------------------------------------------------------------------------
# Prerequisites: programs, libraries
#----------------------------------------------------------------------------

# AC_PROG_CXX will set CXXFLAGS to something like -g -O2 if not
# already set.  We do not want this!
if test -z "$CFLAGS" ; then
  CFLAGS=""
fi
if test -z "$CXXFLAGS" ; then
  CXXFLAGS=""
fi

# Checks for programs.
AC_CANONICAL_HOST
AC_PROG_CXX(CC  cxx  icpc ecpc icc ecc  g++ gcc c++)
AC_PROG_CC(cc  icc ecc  gcc)
AC_PROG_CXXCPP
AC_PROG_RANLIB
AC_PROG_INSTALL

# Use C++ compiler for all checks
AC_LANG(C++)

# Checks for libraries.
# FIXME: binutils, xerces, openanalysis

# Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(inttypes.h)
HPC_CHECK_CXX_STDC_HEADERS

# Checks for typedefs, structures, and compiler characteristics.
# -none thus far-

# Checks for library functions.
# -none thus far-

hpctoolkit_top=`cd ${srcdir} ; pwd`
HPCTOOLKIT_PLATFORM=`cd ${hpctoolkit_top}/config; ./hpcplatform`
AC_SUBST([HPCTOOLKIT_PLATFORM])


# NOTE: we should allow the locations of xerces, OA, and binutils
#   installations to be passed in as options.

#-------------------------------------------------
# Xerces-C (XML parser)
#-------------------------------------------------

XERCES="${hpctoolkit_top}/../xercesc/${HPCTOOLKIT_PLATFORM}"
XERCES_IFLAGS="-I${XERCES}/include"
XERCES_LDFLAGS="-L${XERCES}/lib -lxerces-c"
AC_SUBST([XERCES])
AC_SUBST([XERCES_IFLAGS])
AC_SUBST([XERCES_LDFLAGS])

#-------------------------------------------------
# Open Analysis (CFG builder, etc.)
#-------------------------------------------------

OA="${hpctoolkit_top}/../OpenAnalysis/${HPCTOOLKIT_PLATFORM}"
OA_IFLAGS="-DOA_IRHANDLETYPE_SZ64 -I${OA}/include"
OA_LDFLAGS="-L${OA}/lib -lOAsz64"
AC_SUBST([OA_IFLAGS])
AC_SUBST([OA_LDFLAGS])

#-------------------------------------------------
# GNU's binutils (binary interface)
#-------------------------------------------------

GNUBINUTILS="${hpctoolkit_top}/../binutils/${HPCTOOLKIT_PLATFORM}"
GNUBINUTIL_IFLAGS="-I${GNUBINUTILS}/include"
GNUBINUTIL_LDFLAGS="-L${GNUBINUTILS}/lib -lbfd -lopcodes -liberty -lintl"
AC_SUBST([GNUBINUTIL_IFLAGS])
AC_SUBST([GNUBINUTIL_LDFLAGS])


#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------
OPT_DEVELOP="false"

AC_ARG_ENABLE(develop,
  [  --enable-develop    Build development version (enable debugging)],
  [case "${enableval}" in
     yes) develop="true" ; OPT_DEVELOP="true" ;;
     no)  develop="false" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-develop]) ;;
   esac],
  [develop="false"])
AM_CONDITIONAL(OPT_DEVELOP, test "$develop" = "true")

#----------------------------------------------------------------------------
# Host-dependent configuration
# $host: $host_cpu-$host_vendor-$host_os
#----------------------------------------------------------------------------

# Given a compiler name, compares it against the value of CXX and test
# for a match.  returns 0 if equal; 1 otherwise
# args: ($1): (compiler_name)
CXXcmp()
{
  if ( echo "$CXX" | grep "$1\$" >/dev/null 2>&1 ); then
    return 0
  fi
  return 1
}

#-------------------------------------------------
# Host-specific settings for HPCToolkit
#-------------------------------------------------

# General settings for internal libraries
HOST_CFLAGS=""
HOST_CXXFLAGS=""
HOST_AR=""
HOST_LIBTREPOSITORY=""

# Specific settings for programs
HOST_HPCVIEW_LDFLAGS=""
HOST_BLOOP_LDFLAGS=""
HOST_XPROF_LDFLAGS=""
HOST_XCSPROF_LDFLAGS=""
my_demangle_ldflags=""

# Options
if test "$OPT_DEVELOP" = "true" ; then
  my_opt_cflags="-g -O0"
  my_opt_cxxflags="-g -O0"
else 
  # On some compilers, -g implies -O0.  Make sure -O2 takes precedence!
  my_opt_flags="-g -O2"
  my_opt_cxxflags="-g -O2"
fi


case ${host} in
  alpha*-*-linux* | alpha*-*-osf*)
    if CXXcmp "cxx"; then
      # Compaq compiler
      HOST_CXXFLAGS="-std strict_ansi -rtti -pt -ptr ./cxx_trepository"
      HOST_LIBTREPOSITORY="./cxx_trepository/*.o"
      if test "$OPT_DEVELOP" = "true" ; then
        my_opt_flags="-g"
        my_opt_cxxflags="-gall"
      else 
        my_opt_flags="-g3 -O2"
        my_opt_cxxflags="-g3 -O2"
      fi 
    fi
    # DEC demangle
    my_demangle_ldflags="-lmld"
    ;;
  i386*-*-linux* | i686*-*-linux* | ia64*-*-linux*)
    if CXXcmp "icpc" || CXXcmp "ecpc" || CXXcmp "icc" || CXXcmp "ecc" ; then
      :
    fi
    ;;
  mips*-*-irix*)
    if CXXcmp "CC"; then
      # SGI MIPSpro
      HOST_CFLAGS="-64"
      HOST_CXXFLAGS="-64 -LANG:std"
      HOST_AR="$CXX $CXXFLAGS ${HOST_CXXFLAGS} -ar -o"
    fi
    if CXXcmp "g++" || CXXcmp "gcc" || CXXcmp "c++"; then
      HOST_CFLAGS="-mabi=64"
      HOST_CXXFLAGS="-mabi=64"
    fi
    # SGI demangle
    my_demangle_ldflags="-lmangle"
    ;;
  sparc*-*-solaris*)
    if CXXcmp "CC"; then
      # Sun Forte/ONE
      HOST_AR="$CXX $CXXFLAGS -xar -o"
    fi
    # Sun demangle
    my_demangle_ldflags="-ldemangle"
    ;;
  *)
    ;;
esac

# Grab optimization flags
HOST_CFLAGS="${HOST_CFLAGS} ${my_opt_flags}"
HOST_CXXFLAGS="${HOST_CXXFLAGS} ${my_opt_cxxflags}"

# Ensure IEEE support for Nan.h/C (Only essential on Tru64 and IRIX)
HOST_CFLAGS="${HOST_CFLAGS} -D_IEEE"
HOST_CXXFLAGS="${HOST_CXXFLAGS} -D_IEEE"

# Several programs need system's demangle option
HOST_BLOOP_LDFLAGS="${HOST_BLOOP_LDFLAGS} ${my_demangle_ldflags}"
HOST_XPROF_LDFLAGS="${HOST_XPROF_LDFLAGS} ${my_demangle_ldflags}"
HOST_XCSPROF_LDFLAGS="${HOST_XCSPROF_LDFLAGS} ${my_demangle_ldflags}"

#-------------------------------------------------
# Distribute these settings into makefiles
#-------------------------------------------------
AC_SUBST([HOST_CFLAGS])
AC_SUBST([HOST_CXXFLAGS])
AC_SUBST([HOST_AR])
AC_SUBST([HOST_LIBTREPOSITORY])
AM_CONDITIONAL(IS_HOST_AR, test "${HOST_AR}" != "")

AC_SUBST([HOST_BLOOP_LDFLAGS])
AC_SUBST([HOST_HPCVIEW_LDFLAGS])
AC_SUBST([HOST_XPROF_LDFLAGS])
AC_SUBST([HOST_XCSPROF_LDFLAGS])

#----------------------------------------------------------------------------
# Makefiles to create
#----------------------------------------------------------------------------

AC_OUTPUT([ \
  Makefile \
  src/Makefile \
  src/hpctoolkit/Makefile \
  src/hpctoolkit/bloop/Makefile \
  src/hpctoolkit/filters/Makefile \
  src/hpctoolkit/hpcquick/Makefile \
  src/hpctoolkit/hpcview/Makefile \
  src/hpctoolkit/misc/Makefile \
  src/hpctoolkit/ptran/Makefile \
  src/hpctoolkit/xcsprof/Makefile \
  src/hpctoolkit/xprof/Makefile \
  src/lib/Makefile \
  src/lib/binutils/Makefile \
  src/lib/hpcfile/Makefile \
  src/lib/ISA/Makefile \
  src/lib/perl/Makefile \
  src/lib/support/Makefile \
  src/lib/xml/Makefile \
])

