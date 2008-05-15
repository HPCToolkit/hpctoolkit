# -*-Mode: autoconf;-*-
# $Id$

## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002-2007, Rice University 
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
# Organization:
# 1. Autoconf dependencies (autoconf version/automake version)
# 2. Special options that need to be checked before compiler tests
# 3. Basic prerequisites (compiler tests, tools, system, etc.)
# 4. Host-specific settings and tests 
# 5. Process and test --with/--enable options (which may need the host-specific settings)
# 6. Generate output
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

AC_INIT([hpctoolkit], [TRUNK-4.9.0=1280], [hpc@cs.rice.edu], [hpctoolkit])
AC_CONFIG_SRCDIR([src/include/HPCToolkitVersionInfo.h])
AC_CONFIG_AUX_DIR([config])

AC_PREREQ(2.61)
AM_INIT_AUTOMAKE(1.10)
# Require libtool 1.5.23b

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
# Special Options that should be tested first
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


#----------------------------------------------------------------------------
# Prerequisites: compilers, tools, system libraries
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
AC_PROG_CC_STDC()
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

# Check for types
AC_CHECK_SIZEOF([void*])
AC_CHECK_TYPES([ushort, uint, ulong])

# Check pthread.h
HPC_CHECK_COMPILE_PTHREAD_H()

# Check for multilib
# NOTE: can this be made more elegant?  This value should be used to
# automatically add/exclude lib32/lib64 paths below.
HAVE_OS_MULTILIB="no"
LIB_SEARCH_PATH="lib"
AC_MSG_CHECKING([for multilib platform])
case ${host} in
  x86_64*-*-linux* | mips64*-*-linux*)
    HAVE_OS_MULTILIB="yes"
    LIB_SEARCH_PATH="lib lib64 lib32"
    ;;
esac
if test "${HAVE_OS_MULTILIB}" = "yes" ; then
  AC_DEFINE([HAVE_OS_MULTILIB], [], [])
fi
AC_MSG_RESULT([${HAVE_OS_MULTILIB} (searching: ${LIB_SEARCH_PATH})])

# Check for typedefs, structures, and compiler characteristics.
# -none thus far-

# Check for library functions.
# -none thus far-



#----------------------------------------------------------------------------
# Host-dependent configuration
# $host: $host_cpu-$host_vendor-$host_os
#----------------------------------------------------------------------------

HPC_DEF_CXXCMP()

#-------------------------------------------------
# Host-specific defines
#-------------------------------------------------

# NOTE: To see what macros gcc-like compilers define:
#   gcc -dM -E - < /dev/null | sort
HOST_OS=""

case ${host} in
  # Linux
  i386*-*-linux* | i686*-*-linux* | x86_64*-*-linux* | ia64*-*-linux*)
    # __i386 / __x86_64 / __ia64 && __linux
    HOST_OS="Linux"
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  mips64*-*-linux*)
    # __mips64 && __linux / _MIPS_SIM == _ABIN32 / _MIPS_SIM == _ABI64
    HOST_OS="Linux"
    AC_DEFINE([HOST_PLATFORM_MIPS64LE_LINUX], [], [])
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  # MacOS
  powerpc*-*-darwin*)
    # __ppc__ / __MACH__
    HOST_OS="MacOS"
    AC_DEFINE([HOST_OS_MACOS], [], [])
    ;;

  i386*-*-darwin* | i686*-*-darwin*)
    HOST_OS="MacOS"
    AC_DEFINE([HOST_OS_MACOS], [], [])
    ;;

  # IRIX
  mips*-*-irix*)
    # __mips && __sgi && __unix
    # _MIPS_ISA == _MIPS_ISA_MIPS1 | _MIPS_ISA == _MIPS_ISA_MIPS2
    # _MIPS_ISA == _MIPS_ISA_MIPS3 | _MIPS_ISA == _MIPS_ISA_MIPS4
    HOST_OS="IRIX"
    AC_DEFINE([HOST_OS_IRIX], [], [])
    ;;

  # Solaris
  sparc*-*-solaris*)
    # __sparc && __sun && __unix
    HOST_OS="Solaris"
    AC_DEFINE([HOST_OS_SOLARIS], [], [])
    ;;

  # Tru64/OSF
  alpha*-*-osf*)
    # __alpha / __digital__ && __unix__
    HOST_OS="Tru64"
    AC_DEFINE([HOST_OS_TRU64], [], [])
    ;;

  *)
    AC_MSG_ERROR([HPCToolkit is not configured for HOST=${host}!])
    ;;
esac


#-------------------------------------------------
# Host-specific compiler settings
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
HOST_HPCRUN_LDFLAGS=""
HOST_HPCSTRUCT_LDFLAGS=""
HOST_HPCPROF_LDFLAGS=""
HOST_HPCPROF_FLAT_LDFLAGS=""
HOST_HPCPROFTT_LDFLAGS=""
HOST_XPROF_LDFLAGS=""
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

  # Linux
  i386*-*-linux* | i686*-*-linux* | x86_64*-*-linux* | ia64*-*-linux* | mips64*-*-linux*)
    # Intel compiler
    if HPCcxxcmp([icpc ecpc]) ; then
      :
    fi
    if HPCcccmp([icc ecc]) ; then
      :
    fi

    # Pathscale compiler
    if HPCcxxcmp([pathCC]) ; then
      :
    fi
    if HPCcccmp([pathcc]) ; then
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

  # IRIX
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

  # Solaris
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

  # Tru64/OSF
  alpha*-*-osf*)
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

  *)
    ;;
esac

# Grab optimization flags
HOST_CFLAGS="${HOST_CFLAGS} ${my_opt_cflags}"
HOST_CXXFLAGS="${HOST_CXXFLAGS} ${my_opt_cxxflags}"

# Several programs need system's demangle option
HOST_HPCSTRUCT_LDFLAGS="${HOST_HPCSTRUCT_LDFLAGS} ${my_demangle_ldflags}"
HOST_HPCPROF_LDFLAGS="${HOST_HPCPROF_LDFLAGS} ${my_demangle_ldflags}"
HOST_XPROF_LDFLAGS="${HOST_XPROF_LDFLAGS} ${my_demangle_ldflags}"

#-------------------------------------------------
# Distribute these settings into makefiles
#-------------------------------------------------
AC_SUBST([HOST_CFLAGS])
AC_SUBST([HOST_CXXFLAGS])
AC_SUBST([HOST_AR])
AC_SUBST([HOST_LIBTREPOSITORY])
AC_SUBST([HOST_LINK_NO_START_FILES])
AM_CONDITIONAL(IS_HOST_AR, test "${HOST_AR}" != "")

AC_SUBST([HOST_HPCRUN_LDFLAGS])
AC_SUBST([HOST_HPCSTRUCT_LDFLAGS])
AC_SUBST([HOST_HPCPROF_LDFLAGS])
AC_SUBST([HOST_HPCPROF_FLAT_LDFLAGS])
AC_SUBST([HOST_HPCPROFTT_LDFLAGS])
AC_SUBST([HOST_XPROF_LDFLAGS])


#----------------------------------------------------------------------------
# Prerequisites: external libraries
#----------------------------------------------------------------------------

#-------------------------------------------------
# Check for Xerces-C (XML parser)
#-------------------------------------------------

# NOTE: we should allow the locations of xerces, OA, and binutils
# installations to optionally be given on configure line or in the
# environment.

AC_MSG_CHECKING([for XercesC])

XERCES="${hpctoolkit_top}/../xercesc/${HPCTOOLKIT_PLATFORM}"

AC_ARG_WITH([xerces],
  AS_HELP_STRING([--with-xerces=<XercesC path>],
                 [use given XercesC installation]),
  [if test $withval != no; then
     XERCES=${withval}
   fi],
  [])

XERCES_IFLAGS="-I${XERCES}/include"
XERCES_LDFLAGS="-L${XERCES}/lib -lxerces-c"

AC_MSG_RESULT([${XERCES}])
AC_SUBST([XERCES])
AC_SUBST([XERCES_IFLAGS])
AC_SUBST([XERCES_LDFLAGS])


#-------------------------------------------------
# Check for Open Analysis (CFG builder, etc.)
#-------------------------------------------------

AC_MSG_CHECKING([for OpenAnalysis])

OA="${hpctoolkit_top}/../open-analysis/${HPCTOOLKIT_PLATFORM}"

AC_ARG_WITH([oa],
  AS_HELP_STRING([--with-oa=<OpenAnalysis path>],
                 [use given OpenAnalysis installation]),
  [if test $withval != no; then
     OA=${withval}
   fi],
  [])

OA_IFLAGS="-DOA_IRHANDLETYPE_SZ64 -I${OA}/include"
OA_LDFLAGS="-L${OA}/lib -lOAsz64"

AC_MSG_RESULT([${OA}])
AC_SUBST([OA_IFLAGS])
AC_SUBST([OA_LDFLAGS])


#-------------------------------------------------
# Check for GNU's binutils (binary interface)
#-------------------------------------------------
# cf. enable-hpc-binutils

AC_MSG_CHECKING([for GNU binutils])

GNUBINUTILS="${hpctoolkit_top}/../binutils/${HPCTOOLKIT_PLATFORM}"

AC_ARG_WITH([binutils],
  AS_HELP_STRING([--with-binutils=<GNU binutils path>],
                 [use given GNU binutils installation]),
  [if test $withval != no; then
     GNUBINUTILS=${withval}
   fi],
  [])


# The binutils installer (2.16.1, 2.17) creates libbfd.la and
# libopcodes.la but only libiberty.a.  Apparently, not having
# libiberty.la can cause libtool (1.5.20, 1.5.22) to prefer a system
# library (without a .la!) -- thus ignoring the -L directive.  So we
# provide an absolute reference.
GNUBINUTILS_IFLAGS="-I${GNUBINUTILS}/include"
GNUBINUTILS_LDFLAGS="-L${GNUBINUTILS}/lib -lbfd -lopcodes ${GNUBINUTILS}/lib/libiberty.a"

# on non-Linux systems we build libintl
if test "${HOST_OS}" != "Linux"; then
   GNUBINUTILS_LDFLAGS="${GNUBINUTILS_LDFLAGS} ${GNUBINUTILS}/lib/libintl.a"
fi

AC_MSG_RESULT([${GNUBINUTILS}])
AC_SUBST([GNUBINUTILS_IFLAGS])
AC_SUBST([GNUBINUTILS_LDFLAGS])


#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------

#-------------------------------------------------
# with-monitor
#-------------------------------------------------

AC_MSG_CHECKING([for monitor])

OPT_WITH_MONITOR=no
OPT_INSTALL_MONITOR=no
MONITOR=
MONITOR_LIBS=
HPC_MONITOR=


# Use monitor by default
if test "${HOST_OS}" = "Linux"; then
  OPT_WITH_MONITOR=yes
  OPT_INSTALL_MONITOR=yes
  MONITOR="${hpctoolkit_top}/../monitor/${HPCTOOLKIT_PLATFORM}"
fi

AC_ARG_WITH([monitor],
  AS_HELP_STRING([--with-monitor=<libmonitor path>],
                 [use given libmonitor installation]),
  [if test $withval != no; then
     OPT_WITH_MONITOR=yes
     OPT_INSTALL_MONITOR=no
     MONITOR=${withval}
   else
     OPT_WITH_MONITOR=no
   fi],
  [])

for lib in ${LIB_SEARCH_PATH} ; do
  if test -r "${MONITOR}/${lib}/libmonitor.so"; then
    MONITOR_LIBS="${MONITOR_LIBS} ${MONITOR}/${lib}/libmonitor.so"
  fi
done
MONITOR_LIBS=`echo ${MONITOR_LIBS}`

MONITOR_IFLAGS="-I${MONITOR}/include"
MONITOR_LDFLAGS="... -lmonitor"
if test "${OPT_INSTALL_MONITOR}" = "yes" ; then
  HPC_MONITOR="hpctoolkit/monitor"  # lib/...
else 
  HPC_MONITOR="${MONITOR}"
fi


if test "${OPT_WITH_MONITOR}" = "yes" ; then
  AC_DEFINE([HAVE_MONITOR], [], [Monitor library])
  AC_MSG_RESULT([yes (${MONITOR_LIBS})])
else
  AC_MSG_RESULT([no (not needed)])
fi

AC_SUBST([MONITOR])
AC_SUBST([HPC_MONITOR])
AC_SUBST([OPT_INSTALL_MONITOR])


#-------------------------------------------------
# with-papi
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with PAPI])

HPC_DEF_CHECK_CXX_PAPI_LINK

OPT_HAVE_PAPI=no
OPT_PAPI=
OPT_PAPI_IFLAGS=
OPT_PAPI_LDFLAGS=
OPT_PAPI_LIBPATH=
OPT_PAPI_LIB=

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
# Note: same test is in hpctoolkit-oss's Makefile.quick
if test "$OPT_HAVE_PAPI" = "yes" ; then
  if test -f "${OPT_PAPI}/include/papi.h"; then
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}/include"
  else
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}"
  fi
  
  for lib in ${LIB_SEARCH_PATH} ; do
    if test -r "${OPT_PAPI}/${lib}/libpapi.so"; then
      if HPC_check_cxx_papi_link ${OPT_PAPI_IFLAGS} ${OPT_PAPI}/${lib} ; then
        OPT_PAPI_LIBPATH="${OPT_PAPI}/${lib}"
        OPT_PAPI_LIB="${OPT_PAPI_LIBPATH}/libpapi.so"
        OPT_PAPI_LDFLAGS="-L${OPT_PAPI_LIBPATH} -lpapi"
        break;
      fi
    fi
  done

  #for lib in ${LIB_SEARCH_PATH} ; do
  #  if test -r "${OPT_PAPI}/${lib}/libpapi.so"; then
  #    OPT_PAPI_LIB="${OPT_PAPI_LIB} ${OPT_PAPI}/${lib}/libpapi.so"
  #  fi
  #done
  #OPT_PAPI_LIB=`echo ${OPT_PAPI_LIB}`

fi

# Check for PAPI > 3.0 [skip for now]

if test "${OPT_HAVE_PAPI}" = "yes"; then 
  if test -n "${OPT_PAPI_LIB}" ; then
    AC_MSG_RESULT([yes (${OPT_PAPI_LIB})])
  else
    AC_MSG_ERROR([cannot find usable PAPI in '${LIB_SEARCH_PATH}'])
  fi
else
  AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(OPT_HAVE_PAPI, test "${OPT_HAVE_PAPI}" = "yes")
AC_SUBST(OPT_PAPI)
AC_SUBST(OPT_PAPI_IFLAGS)
AC_SUBST(OPT_PAPI_LDFLAGS)
AC_SUBST(OPT_PAPI_LIBPATH)
AC_SUBST(OPT_PAPI_LIB)


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
if test "${OPT_HAVE_PAPI}" = "yes" && test "${HOST_OS}" = "Linux"; then
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
  if test "${OPT_HAVE_LIBUNWIND}" = "no" && test "${HOST_OS}" = "Linux"; then
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


#-------------------------------------------------
# enable-hpc-binutils: Enable use of HPCToolkit's binutils 
#-------------------------------------------------

AC_MSG_CHECKING([whether to enable HPCToolkit GNU binutils functionality])

OPT_ENABLE_HPC_GNUBINUTILS="yes"

AC_ARG_ENABLE([hpc-binutils],
  AS_HELP_STRING([--enable-hpc-binutils],
                 [Enable extra HPCToolkit GNU binutils functionality]),
  [case "${enableval}" in
     yes) OPT_ENABLE_HPC_GNUBINUTILS="yes" ;;
     no)  OPT_ENABLE_HPC_GNUBINUTILS="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-hpc-binutils]) ;;
   esac],
  [OPT_ENABLE_HPC_GNUBINUTILS=yes])

AC_MSG_RESULT([${OPT_ENABLE_HPC_GNUBINUTILS}])

if test "${OPT_ENABLE_HPC_GNUBINUTILS}" = "yes" ; then
  AC_DEFINE([HAVE_HPC_GNUBINUTILS], [], [HPCToolkit binutils])
fi


#-------------------------------------------------
# with-objcopy
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with objcopy])

OPT_OBJCOPY=objcopy

AC_ARG_WITH([objcopy],
  AS_HELP_STRING([--with-objcopy=<obcopy_path>],
                 [use given objcopy when hiding hpcrun library symbols]),
  [if test $withval != no; then
     OPT_OBJCOPY=${withval}
   fi],
  [])

AC_MSG_RESULT([yes (${OPT_OBJCOPY})])

AC_SUBST(OPT_OBJCOPY)


#----------------------------------------------------------------------------
# Makefiles to create
#----------------------------------------------------------------------------

AC_CONFIG_FILES([ \
  Makefile \
  \
  doc/Makefile \
  doc/man/Makefile \
  doc/man/HPCToolkitVersionInfo.tex \
  \
  lib/Makefile \
  \
  src/Makefile \
  src/hpctoolkit/Makefile \
  src/hpctoolkit/hpclump/Makefile \
  src/hpctoolkit/hpcprof/Makefile \
  src/hpctoolkit/hpcprof-flat/Makefile \
  src/hpctoolkit/hpcproftt/Makefile \
  src/hpctoolkit/hpcrun/Makefile \
  src/hpctoolkit/hpcstruct/Makefile \
  src/hpctoolkit/misc/Makefile \
  src/hpctoolkit/xprof/Makefile \
  src/lib/Makefile \
  src/lib/analysis/Makefile \
  src/lib/banal/Makefile \
  src/lib/binutils/Makefile \
  src/lib/isa/Makefile \
  src/lib/perl/Makefile \
  src/lib/prof-juicy/Makefile \
  src/lib/prof-juicy-x/Makefile \
  src/lib/prof-lean/Makefile \
  src/lib/support/Makefile \
  src/lib/xml/Makefile \
])

AC_CONFIG_COMMANDS()

AC_OUTPUT()
