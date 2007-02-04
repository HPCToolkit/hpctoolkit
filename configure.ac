# -*-Mode: autoconf;-*-
# $Id$

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
#   ...
#
#############################################################################

m4_include(config/hpc-cxxutils.m4)

#----------------------------------------------------------------------------
# Initialization: 
# Careful: 
#   - Libtool 1.5.18 has a bug interacting with Tru64's ksh.
# Require libtool 1.5.10 and automake 1.9.3 to 1) correctly link
#   templates with MIPSPro compiler on IRIX64, 2) link on alpha-Tru64
#   and 3) fix missing .so problem on Linux.
# Update to autoconf 2.59 and automake 1.8.3
# Require autoconf 2.54 because automake 1.7.6 needs it.  
#  - automake 1.7.6 is needed to generate Intel 7.1 dependencies correctly.
#  - automake 1.6.1 failed to generate separate lists of object files for
#    the two library versions. automake 1.7.5 handles the specification
#    correctly
#----------------------------------------------------------------------------

AC_INIT([HPCToolkit], [TRUNK-4.2.2=660], [hpc@cs.rice.edu], [HPCToolkit])
AC_CONFIG_SRCDIR([src/include/HPCToolkitVersionInfo.h])
AC_CONFIG_AUX_DIR([config])

AC_PREREQ(2.60)
AM_INIT_AUTOMAKE(1.9.6)
# Require libtool 1.5.22

AC_CANONICAL_HOST()

AC_MSG_NOTICE([INFO: On entry: CXX=${CXX} CXXFLAGS=${CXXFLAGS}])
AC_MSG_NOTICE([INFO: On entry: CC=${CC} CFLAGS=${CFLAGS}])

#----------------------------------------------------------------------------
# Setup
#----------------------------------------------------------------------------

# ${srcdir}, ${ac_top_srcdir}, ${ac_top_builddir}
hpctoolkit_top=`cd ${srcdir} && pwd`
hpctoolkit_build=`pwd`
HPCTOOLKIT_PLATFORM=`cd ${hpctoolkit_top}/config && ./hpcplatform`
AC_SUBST([HPCTOOLKIT_PLATFORM])

#----------------------------------------------------------------------------
# Prerequisites: programs, libraries
#----------------------------------------------------------------------------

# AC_PROG_CXX will set CXXFLAGS to something like -g -O2 if not
# already defined.  We do not want this!
HPC_ENSURE_DEFINED_CXXFLAGS()
HPC_ENSURE_DEFINED_CFLAGS()

# Check for programs.
# Note: libtool makes PROG_RANLIB obsolete
AC_PROG_LIBTOOL()
AC_PROG_CXX(HPC_CXX_LIST)
AC_PROG_CC(HPC_CC_LIST)
AC_PROG_CXXCPP()
AC_PROG_INSTALL()

AC_MSG_NOTICE([INFO: After check: CXX=${CXX} CXXFLAGS=${CXXFLAGS}])
AC_MSG_NOTICE([INFO: After check: CC=${CC} CFLAGS=${CFLAGS}])

# Use C++ compiler for all checks
AC_LANG(C++)

# Check for header files
AC_HEADER_STDC()
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(inttypes.h)
HPC_CHECK_CXX_STDC_HEADERS()

# Check pthread.h
HPC_CHECK_COMPILE_PTHREAD_H()

# Check for typedefs, structures, and compiler characteristics.
# -none thus far-

# Check for library functions.
# -none thus far-


#-------------------------------------------------
# Check for Xerces-C (XML parser)
#-------------------------------------------------

# NOTE: we should allow the locations of xerces, OA, and binutils
# installations to optionally be given on configure line or in the
# environment.

AC_MSG_CHECKING([for Xerces-C])

XERCES="${hpctoolkit_top}/../xercesc/${HPCTOOLKIT_PLATFORM}"
XERCES_IFLAGS="-I${XERCES}/include"
XERCES_LDFLAGS="-L${XERCES}/lib -lxerces-c"

AC_MSG_RESULT([${XERCES}])
AC_SUBST([XERCES])
AC_SUBST([XERCES_IFLAGS])
AC_SUBST([XERCES_LDFLAGS])

#-------------------------------------------------
# Check for monitor
#-------------------------------------------------

AC_MSG_CHECKING([for monitor])

MONITOR_LIBNM="lib"
if test "${HPCTOOLKIT_PLATFORM}" = "x86_64-Linux" ; then
  MONITOR_LIBNM="lib64"
fi

MONITOR="${hpctoolkit_top}/../monitor/${HPCTOOLKIT_PLATFORM}"
MONITOR_IFLAGS="-I${MONITOR}/include"
MONITOR_LIBPATH="${MONITOR}/${MONITOR_LIBNM}"
MONITOR_LDFLAGS="-L${MONITOR_LIBPATH}"
MONITOR_LIBSO="${MONITOR_LIBPATH}/libmonitor.so"
MONITOR_LIBSO_INSTALLED="monitor/${MONITOR_LIBNM}/libmonitor.so"

if ( echo "${HPCTOOLKIT_PLATFORM}" | grep "Linux\$" >/dev/null 2>&1 ); then
  #AC_DEFINE([HAVE_MONITOR], [], [Monitor library])
  #AC_MSG_RESULT([yes (${MONITOR_LIBSO})])
  AC_MSG_RESULT([no (skipping!)])
else
  AC_MSG_RESULT([no (not needed)])
fi

AC_SUBST([MONITOR])
AC_SUBST([MONITOR_LIBPATH])
AC_SUBST([MONITOR_LIBSO])
AC_SUBST([MONITOR_LIBSO_INSTALLED])

#-------------------------------------------------
# Check for Open Analysis (CFG builder, etc.)
#-------------------------------------------------

AC_MSG_CHECKING([for OpenAnalysis])

OA="${hpctoolkit_top}/../OpenAnalysis/${HPCTOOLKIT_PLATFORM}"
OA_IFLAGS="-DOA_IRHANDLETYPE_SZ64 -I${OA}/include"
OA_LDFLAGS="-L${OA}/lib -lOAsz64"

AC_MSG_RESULT([${OA}])
AC_SUBST([OA_IFLAGS])
AC_SUBST([OA_LDFLAGS])

#-------------------------------------------------
# Check for GNU's binutils (binary interface)
#-------------------------------------------------

AC_MSG_CHECKING([for GNU binutils])

# The binutils installer (2.16.1, 2.17) creates libbfd.la and
# libopcodes.la but only libiberty.a.  Apparently, not having
# libiberty.la can cause libtool (1.5.20, 1.5.22) to prefer a system
# library (without a .la!) -- thus ignoring the -L directive.  So we
# provide an absolute reference.

GNUBINUTILS="${hpctoolkit_top}/../binutils/${HPCTOOLKIT_PLATFORM}"
GNUBINUTILS_IFLAGS="-I${GNUBINUTILS}/include"
GNUBINUTILS_LDFLAGS="-L${GNUBINUTILS}/lib -lbfd -lopcodes ${GNUBINUTILS}/lib/libiberty.a"

# on non-Linux systems we build libintl
case ${host} in
   *-*-linux*) 
   ;;

   *) 
   GNUBINUTILS_LDFLAGS="${GNUBINUTILS_LDFLAGS} ${GNUBINUTILS}/lib/libintl.a"
   ;;
esac

AC_MSG_RESULT([${GNUBINUTILS}])
AC_SUBST([GNUBINUTILS_IFLAGS])
AC_SUBST([GNUBINUTILS_LDFLAGS])


#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------

#-------------------------------------------------
# enable-develop
#-------------------------------------------------

AC_MSG_CHECKING([whether DEVELOP mode is enabled])

OPT_ENABLE_DEVELOP=no

AC_ARG_ENABLE([develop],
  AS_HELP_STRING([--enable-develop],
                 [Build development version (enable debugging)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_DEVELOP="yes" ;;
     no)  OPT_ENABLE_DEVELOP="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-develop]) ;;
   esac],
  [OPT_ENABLE_DEVELOP=no])

AC_MSG_RESULT([${OPT_ENABLE_DEVELOP}])
AM_CONDITIONAL(OPT_ENABLE_DEVELOP, test "${OPT_ENABLE_DEVELOP}" = "yes")

#-------------------------------------------------
# with-papi
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with PAPI])

OPT_HAVE_PAPI=no
OPT_PAPI=
OPT_PAPI_IFLAGS=
OPT_PAPI_LIBPATH=
OPT_PAPI_LDFLAGS=
OPT_PAPI_LIBSO=
OPT_PAPI_LIBA=

AC_ARG_WITH([papi],
  AS_HELP_STRING([--with-papi=<papi_install_path>],
                 [use given PAPI installation (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     OPT_HAVE_PAPI=yes
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-papi requires absolute path as argument; given '${withval}'])
     fi
     OPT_PAPI=${withval}
   fi],
  [if test -f /usr/local/papi/include/papi.h; then
    OPT_HAVE_PAPI=yes
    OPT_PAPI=/usr/local/papi
   elif test -f /usr/local/include/papi.h; then
    OPT_HAVE_PAPI=yes
    OPT_PAPI=/usr/local
  fi])

# Allow <papi_install_path> to either point to a true installation or
# simply a build.  In the latter case, the papi library and header
# will have a different location.
#   <papi-install-path>/ include/papi.h
#                        lib/libpapi.so
#                        lib64/libpapi.so
#   <papi-build-path>/ papi.h
#                      libpapi.so
# Note: same test is in HPCToolkitRoot's Makefile.quick
if test "$OPT_HAVE_PAPI" = "yes" ; then
  if test -f "${OPT_PAPI}/include/papi.h"; then
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}/include"
  else
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}"
  fi
  if test -r "${OPT_PAPI}/lib64/libpapi.so"; then
    OPT_PAPI_LIBPATH="${OPT_PAPI}/lib64"
  elif test -r "${OPT_PAPI}/lib/libpapi.so"; then
    OPT_PAPI_LIBPATH="${OPT_PAPI}/lib"
  else
    OPT_PAPI_LIBPATH="${OPT_PAPI}"
  fi
fi

if test -n "${OPT_PAPI_LIBPATH}" ; then
  OPT_PAPI_LDFLAGS="-L${OPT_PAPI_LIBPATH} -lpapi"
  OPT_PAPI_LIBSO="${OPT_PAPI_LIBPATH}/libpapi.so"
  OPT_PAPI_LIBA="${OPT_PAPI_LIBPATH}/libpapi.a"
fi

# Check for PAPI > 3.0 [skip for now]

if test "${OPT_HAVE_PAPI}" = "yes"; then 
  AC_MSG_RESULT([yes (${OPT_PAPI_LIBSO})])
else
  AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(OPT_HAVE_PAPI, test "${OPT_HAVE_PAPI}" = "yes")
AC_SUBST(OPT_PAPI_IFLAGS)
AC_SUBST(OPT_PAPI_LIBPATH)
AC_SUBST(OPT_PAPI_LDFLAGS)
AC_SUBST(OPT_PAPI_LIBSO)
AC_SUBST(OPT_PAPI_LIBA)

#-------------------------------------------------
# with-libunwind
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with libunwind])

OPT_HAVE_LIBUNWIND=no
OPT_LIBUNWIND=
OPT_LIBUNWIND_IFLAGS=
OPT_LIBUNWIND_LIBPATH=
OPT_LIBUNWIND_LDFLAGS=
OPT_LIBUNWIND_LIBSO=
OPT_LIBUNWIND_LIBA=

AC_ARG_WITH([libunwind],
  AS_HELP_STRING([--with-libunwind=<libunwind_install_path>],
                 [use given LIBUNWIND installation (absolute path) with csprof (default is NO)]),
  [if test $withval != no; then
     OPT_HAVE_LIBUNWIND=yes
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-libunwind requires absolute path as argument; given '${withval}'])
     fi
     OPT_LIBUNWIND=${withval}
   fi],
  [])

# Check for LIBUNWIND > .9xxx [skip for now]

if test "${OPT_HAVE_LIBUNWIND}" = "yes"; then 
  AC_MSG_RESULT([yes (${OPT_LIBUNWIND_LIBSO})])
else
  AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(OPT_HAVE_LIBUNWIND, test "${OPT_HAVE_LIBUNWIND}" = "yes")
AC_SUBST(OPT_LIBUNWIND_IFLAGS)
AC_SUBST(OPT_LIBUNWIND_LIBPATH)
AC_SUBST(OPT_LIBUNWIND_LDFLAGS)
AC_SUBST(OPT_LIBUNWIND_LIBSO)
AC_SUBST(OPT_LIBUNWIND_LIBA)


#-------------------------------------------------
# enable-hpcrun: Only build hpcrun if on Linux and PAPI is present
#-------------------------------------------------

AC_MSG_CHECKING([whether to build hpcrun])

OPT_ENABLE_HPCRUN="no"
if test "${OPT_HAVE_PAPI}" = "yes" &&  
   ( echo "${HPCTOOLKIT_PLATFORM}" | grep "Linux\$" >/dev/null 2>&1 ); then
  OPT_ENABLE_HPCRUN="yes"
fi

AC_MSG_RESULT([${OPT_ENABLE_HPCRUN}])
AM_CONDITIONAL(OPT_ENABLE_HPCRUN, test "${OPT_ENABLE_HPCRUN}" = "yes")


#-------------------------------------------------
# enable-csprof: [not yet available]
# enable-csprof-sync: 
# enable-csprof-async: 
#   - on Linux, requires libunwind
#-------------------------------------------------

AC_MSG_CHECKING([whether to build csprof (synchronous)])

OPT_ENABLE_CSPROF_SYNC=no

AC_ARG_ENABLE([csprof-sync],
  AS_HELP_STRING([--enable-csprof-sync],
                 [Build csprof (synchronous)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_CSPROF_SYNC="yes" ;;
     no)  OPT_ENABLE_CSPROF_SYNC="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-csprof-sync]) ;;
   esac],
  [OPT_ENABLE_CSPROF_SYNC=no])

AC_MSG_RESULT([${OPT_ENABLE_CSPROF_SYNC}])
AM_CONDITIONAL(OPT_ENABLE_CSPROF_SYNC, test "${OPT_ENABLE_CSPROF_SYNC}" = "yes")


AC_MSG_CHECKING([whether to build csprof (asynchronous)])

OPT_ENABLE_CSPROF_ASYNC=no

AC_ARG_ENABLE([csprof-async],
  AS_HELP_STRING([--enable-csprof-async],
                 [Build csprof (synchronous)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_CSPROF_ASYNC="yes" ;;
     no)  OPT_ENABLE_CSPROF_ASYNC="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-csprof-async]) ;;
   esac],
  [OPT_ENABLE_CSPROF_ASYNC=no])

AC_MSG_RESULT([${OPT_ENABLE_CSPROF_ASYNC}])
AM_CONDITIONAL(OPT_ENABLE_CSPROF_ASYNC, test "${OPT_ENABLE_CSPROF_ASYNC}" = "yes")


# On Linux, verify we have libunwind!!!
if test "${OPT_ENABLE_CSPROF_SYNC}" = "yes" || 
   test "${OPT_ENABLE_CSPROF_ASYNC}" = "yes" ; then
  if test "${OPT_HAVE_LIBUNWIND}" = "no" &&  
     ( echo "${HPCTOOLKIT_PLATFORM}" | grep "Linux\$" >/dev/null 2>&1 ); then
    AC_MSG_ERROR([libunwind is needed to build csprof on Linux])
  fi
fi


#-------------------------------------------------
# enable-devtools: Tools of interest to developers
#-------------------------------------------------

AC_MSG_CHECKING([whether to build developer tools (devtools)])

OPT_ENABLE_DEVTOOLS="no"

AC_ARG_ENABLE([devtools],
  AS_HELP_STRING([--enable-devtools],
                 [Build development tools (enable debugging)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_DEVTOOLS="yes" ;;
     no)  OPT_ENABLE_DEVTOOLS="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-devtools]) ;;
   esac],
  [OPT_ENABLE_DEVTOOLS=no])

AC_MSG_RESULT([${OPT_ENABLE_DEVTOOLS}])
AM_CONDITIONAL(OPT_ENABLE_DEVTOOLS, test "${OPT_ENABLE_DEVTOOLS}" = "yes")


#----------------------------------------------------------------------------
# Host-dependent configuration
# $host: $host_cpu-$host_vendor-$host_os
#----------------------------------------------------------------------------

HPC_DEF_CXXCMP

#-------------------------------------------------
# Host-specific settings for HPCToolkit
#-------------------------------------------------

# FIXME: Now that we use libtool to build all HPCToolkit libraries, we
# may not need this HOST_AR stuff.  (Remove from here and makefiles.)

# General settings for internal libraries
HOST_CFLAGS=""
HOST_CXXFLAGS=""
HOST_AR=""
HOST_LIBTREPOSITORY=""
HOST_LINK_NO_START_FILES="-nostartfiles"

# Specific settings for programs
HOST_HPCVIEW_LDFLAGS=""
HOST_BLOOP_LDFLAGS=""
HOST_XPROF_LDFLAGS=""
HOST_XCSPROF_LDFLAGS=""
HOST_HPCRUN_LDFLAGS=""
HOST_HPCPROF_LDFLAGS=""
my_demangle_ldflags=""

# Options: Default for GCC
if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
  my_opt_cflags="-g -O0"
  my_opt_cxxflags="-g -O0"
else 
  # On some compilers, -g implies -O0.  Make sure -O2 takes precedence!
  my_opt_cflags="-g -O2"
  my_opt_cxxflags="-g -O2"
fi


case ${host} in
  alpha*-*-linux* | alpha*-*-osf*)
    # Compaq compiler
    if HPCcxxcmp([cxx]); then
      # Before using libtool, we had used local
      # repositories and included template definitions in respective
      # archives.  However, libtool doesn't take kindly to including
      # .o's and .lo's in the same archive.  Thus, we now use a global
      # repository, even though it means archives are not self-contained.
      HOST_CXXFLAGS="-std strict_ansi -rtti -pt -ptr ${hpctoolkit_build}/cxx_trepository"
      #old: HOST_LIBTREPOSITORY="./cxx_trepository/*.o"
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cxxflags="-gall"
      else 
        my_opt_cxxflags="-g3 -O2"
      fi
    fi
    if HPCcccmp([cc]); then
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cflags="-g"
      else 
        my_opt_cflags="-g3 -O2"
      fi
    fi

    # DEC demangle
    my_demangle_ldflags="-lmld"
    ;;

  i386*-*-linux* | i686*-*-linux* | x86_64*-*-linux* | ia64*-*-linux*)
    # Intel compiler
    if HPCcxxcmp([icpc ecpc]) ; then
      :
    fi
    if HPCcccmp([icc ecc]) ; then
      :
    fi

    # PGI compiler
    if HPCcxxcmp([pgCC]) ; then
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cxxflags="-g -O0"
      else
        my_opt_cxxflags="-gopt -O2"
      fi
    fi
    if HPCcccmp([pgcc]) ; then
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cflags="-g -O0 -c9x"
      else
        my_opt_cflags="-gopt -O2 -c9x"
      fi
    fi
    ;;

  mips*-*-irix*)
    # SGI MIPSpro
    if HPCcxxcmp([CC]) ; then
      HOST_CXXFLAGS="-64 -LANG:std"
      HOST_AR="$CXX $CXXFLAGS ${HOST_CXXFLAGS} -ar -o"
    fi
    if HPCcccmp([cc]) ; then
      HOST_CFLAGS="-64"
    fi
    
    # GCC
    if HPCcxxcmp([g++ c++]) ; then
      HOST_CXXFLAGS="-mabi=64"
    fi
    if HPCcccmp([gcc]) ; then
      HOST_CFLAGS="-mabi=64"
    fi
   
    # SGI demangle
    my_demangle_ldflags="-lmangle"
    ;;

  sparc*-*-solaris*)
    # Sun Forte/ONE
    if HPCcxxcmp([CC]); then
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cxxflags="-g"
      else 
        my_opt_cxxflags="-fast -g"
      fi
      HOST_AR="$CXX $CXXFLAGS -xar -o"
    fi
    if HPCcccmp([cc]); then
      if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
        my_opt_cflags="-g"
      else 
        my_opt_cflags="-fast -g"
      fi
    fi
    
    # Sun demangle
    my_demangle_ldflags="-ldemangle"
    ;;

  *)
    ;;
esac

# Grab optimization flags
HOST_CFLAGS="${HOST_CFLAGS} ${my_opt_cflags}"
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
AC_SUBST([HOST_LINK_NO_START_FILES])
AM_CONDITIONAL(IS_HOST_AR, test "${HOST_AR}" != "")

AC_SUBST([HOST_BLOOP_LDFLAGS])
AC_SUBST([HOST_HPCVIEW_LDFLAGS])
AC_SUBST([HOST_XPROF_LDFLAGS])
AC_SUBST([HOST_XCSPROF_LDFLAGS])
AC_SUBST([HOST_HPCRUN_LDFLAGS])
AC_SUBST([HOST_HPCPROF_LDFLAGS])

#----------------------------------------------------------------------------
# Makefiles to create
#----------------------------------------------------------------------------

AC_CONFIG_FILES([ \
  Makefile \
  src/Makefile \
  src/hpctoolkit/Makefile \
  src/hpctoolkit/bloop/Makefile \
  src/hpctoolkit/filters/Makefile \
  src/hpctoolkit/hpcquick/Makefile \
  src/hpctoolkit/hpcprof/Makefile \
  src/hpctoolkit/hpcrun/Makefile \
  src/hpctoolkit/hpcview/Makefile \
  src/hpctoolkit/lump/Makefile \
  src/hpctoolkit/misc/Makefile \
  src/hpctoolkit/ptran/Makefile \
  src/hpctoolkit/xcsprof/Makefile \
  src/hpctoolkit/xprof/Makefile \
  src/lib/Makefile \
  src/lib/banal/Makefile \
  src/lib/binutils/Makefile \
  src/lib/isa/Makefile \
  src/lib/perl/Makefile \
  src/lib/prof-juicy/Makefile \
  src/lib/prof-lean/Makefile \
  src/lib/support/Makefile \
  src/lib/xml/Makefile \
  \
  src/lib/perl/HPCToolkitVersionInfo.pm \
])

AC_CONFIG_COMMANDS()

AC_OUTPUT()
