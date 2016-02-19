dnl -*-Mode: autoconf;-*-
dnl * BeginRiceCopyright *****************************************************
dnl
dnl $HeadURL$
dnl $Id$
dnl
dnl --------------------------------------------------------------------------
dnl Part of HPCToolkit (hpctoolkit.org)
dnl
dnl Information about sources of support for research and development of
dnl HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
dnl --------------------------------------------------------------------------
dnl
dnl Copyright ((c)) 2002-2016, Rice University
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl * Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl
dnl * Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer in the
dnl   documentation and/or other materials provided with the distribution.
dnl
dnl * Neither the name of Rice University (RICE) nor the names of its
dnl   contributors may be used to endorse or promote products derived from
dnl   this software without specific prior written permission.
dnl
dnl This software is provided by RICE and contributors "as is" and any
dnl express or implied warranties, including, but not limited to, the
dnl implied warranties of merchantability and fitness for a particular
dnl purpose are disclaimed. In no event shall RICE or contributors be
dnl liable for any direct, indirect, incidental, special, exemplary, or
dnl consequential damages (including, but not limited to, procurement of
dnl substitute goods or services; loss of use, data, or profits; or
dnl business interruption) however caused and on any theory of liability,
dnl whether in contract, strict liability, or tort (including negligence
dnl or otherwise) arising in any way out of the use of this software, even
dnl if advised of the possibility of such damage.
dnl
dnl ******************************************************* EndRiceCopyright *

dnl ##########################################################################
dnl
dnl  File:
dnl    $HeadURL$
dnl
dnl  Description:
dnl    autoconf input file for HPCToolkit.
dnl    *Process with autoconf to produce configure*
dnl
dnl  Modification history:
dnl    2004/03/03 - Nathan Tallent
dnl    ...
dnl
dnl  Organization:
dnl  1. Autoconf dependencies (autoconf version/automake version)
dnl  2. Special options that need to be checked before compiler tests
dnl  3. Basic prerequisites (compiler tests, tools, system, etc.)
dnl  4. Host-specific settings and tests 
dnl  5. Process and test --with/--enable options (which may need the host-specific settings)
dnl  6. Generate output
dnl
dnl ##########################################################################

dnl--------------------------------------------------------------------------
dnl Autoconf dictionary:
dnl   build : platform on which the package is configured and
dnl           compiled. Defaults to the result of running config.guess.
dnl   host  : platform on which the package runs. Defaults to build.
dnl   target: platform for which any compiler tools in the package
dnl           produce code (rarely needed). Defaults to host.
dnl--------------------------------------------------------------------------

m4_include(config/hpc-cxxutils.m4)
m4_include(config/hpc-mpiutils.m4)

#----------------------------------------------------------------------------
# Initialization: 
#----------------------------------------------------------------------------

AC_INIT([hpctoolkit], [5.4.2],
        [hpctoolkit-forum@rice.edu],
	[hpctoolkit],
	[http://hpctoolkit.org/])
AC_CONFIG_SRCDIR([src/include/hpctoolkit-config.h.in])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

AC_MSG_NOTICE([*** INFO: from user: CXX='${CXX}' CXXFLAGS='${CXXFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: from user: CC='${CC}' CFLAGS='${CFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPICC='${MPICC}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPICXX='${MPICXX}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPIF77='${MPIF77}' ***])

# NOTE: please update config/README.version
AC_PREREQ(2.67)
AM_INIT_AUTOMAKE(1.11.1)
LT_PREREQ([2.2.10])

AM_MAINTAINER_MODE()

AC_CANONICAL_BUILD()
AC_CANONICAL_HOST()

MPICC_ORIG=${MPICC}
MPICXX_ORIG=${MPICXX}
MPIF77_ORIG=${MPIF77}

#----------------------------------------------------------------------------
# Setup
#----------------------------------------------------------------------------

# ${srcdir}, ${ac_top_srcdir}, ${ac_top_builddir}
hpctoolkit_top=`cd ${srcdir} && pwd`
hpctoolkit_build=`pwd`
HPCTOOLKIT_PLATFORM=`cd ${hpctoolkit_top}/config && ./hpcplatform`
AC_SUBST([HPCTOOLKIT_PLATFORM])

#-------------------------------------------------
# CONFIG_SITE
#-------------------------------------------------

# If CONFIG_SITE contains the path to a file, then autoconf sources
# that file to reset default values for libdir, etc.  This may break
# our launch scripts by producing an install layout that they don't
# expect.

warn_config_site=no
if test -f "$CONFIG_SITE" ; then
  AC_MSG_WARN([CONFIG_SITE is set in the environment])
  warn_config_site=yes
fi
libdir='${exec_prefix}/lib'

#-------------------------------------------------
# SVN version
#-------------------------------------------------

# Git does not provide linear rev numbers, so we can't append this to
# the package version (sigh).

AC_DEFINE_UNQUOTED([HPCTOOLKIT_VERSION],
	  ["${PACKAGE_VERSION}"],
	  [HPCToolkit version])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_VERSION_STRING],
	  ["A member of HPCToolkit, version ${PACKAGE_VERSION}"],
	  [HPCToolkit version string])

#-------------------------------------------------
# is_cross_compile: non-empty string if cross-compiling
#-------------------------------------------------

is_cross_compile=""
if test "${build}" != "${host}" ; then
  is_cross_compile=${host}
fi


#----------------------------------------------------------------------------
# Special Options that should be tested first
#----------------------------------------------------------------------------

#-------------------------------------------------
# HPC_LT_LDFLAGS, HPCPROFMPI_LT_LDFLAGS: pass libtool link mode flags
#-------------------------------------------------

AC_ARG_VAR([HPC_LT_LDFLAGS],
           [pass libtool link mode flags to hpcstruct, hpcprof*, hpclump])

AC_ARG_VAR([HPCPROFMPI_LT_LDFLAGS],
           [pass libtool link mode flags to hpcprof-mpi (before HPC_LT_LDFLAGS)])


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
# enable-strip-debug
#-------------------------------------------------

# Strip the debug section from the libraries used in hpcrun/hpclink so
# that our code does not appear in the viewer.

default=yes
if test "$OPT_ENABLE_DEVELOP" = yes ; then
  default=no
fi

AC_ARG_ENABLE([strip-debug],
  [AS_HELP_STRING([--enable-strip-debug],
      [strip the debug section from the hpcrun libraries
       (default yes, unless --enable-develop is used)])],
  [],
  [enable_strip_debug="$default"])

case "$enable_strip_debug" in
  no ) ;;
  * )  enable_strip_debug=yes ;;
esac

AC_MSG_CHECKING([whether to strip debug section from hpcrun libraries])
AC_MSG_RESULT([$enable_strip_debug])

AM_CONDITIONAL(OPT_STRIP_HPCRUN_LIBS, [test "$enable_strip_debug" = yes])


#----------------------------------------------------------------------------
# Prerequisites: compilers, tools, system libraries
#----------------------------------------------------------------------------

# AC_PROG_CXX will set CXXFLAGS to something like -g -O2 if not
# already defined.  We do not want this!
HPC_ENSURE_DEFINED_CXXFLAGS()
HPC_ENSURE_DEFINED_CFLAGS()

# Check for programs.
# Note: libtool makes PROG_RANLIB obsolete

AC_PROG_CXX(HPC_CXX_LIST)

AC_PROG_CC(HPC_CC_LIST)
AC_PROG_CC_C99()
if test "${ac_cv_prog_cc_c99}" = "no" ; then
  AC_MSG_ERROR([${CC} ${CFLAGS} does not support C99 mode!])
fi
AM_PROG_CC_C_O()

# Note: this will set CC and CFLAGS if not already set
LT_INIT()
AC_SUBST([LIBTOOL_DEPS])

AM_PROG_AS()
AC_PROG_AWK
AC_PROG_SED

AC_PROG_CXXCPP()

AC_ARG_VAR([LD],
    [ld program for binaries produced by CC, only needed if
     autoconf fails to find a suitable program])

HPC_PROG_MPICXX(HPC_MPICXX_LIST)

AC_PROG_INSTALL()

AC_MSG_NOTICE([*** INFO: post autoconf tests: CXX='${CXX}' CXXFLAGS='${CXXFLAGS}'  ***])
AC_MSG_NOTICE([*** INFO: post autoconf tests: CC='${CC}' CFLAGS='${CFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: post autoconf tests: MPICC='${MPICC}' ***])
AC_MSG_NOTICE([*** INFO: post autoconf tests: MPICXX='${MPICXX}' ***])
AC_MSG_NOTICE([*** INFO: post autoconf tests: MPIF77='${MPIF77}' ***])


#-------------------------------------------------
# C compiler checks
#-------------------------------------------------

AC_MSG_NOTICE([*** Using C comiler for tests ***])

AC_LANG([C])

# Check for header files
AC_HEADER_STDC()
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(inttypes.h)

AS_UNSET([ac_cv_header_unistd_h])   # retest for C++
AS_UNSET([ac_cv_header_inttypes_h]) # retest for C++


# Check for types
AC_CHECK_SIZEOF([void*])
AC_CHECK_TYPES([ushort, uint, ulong])

if test "${ac_cv_type_voidp}" = "yes" ; then
  AC_DEFINE([HAVE_VOIDP_LANG_C], [1], [C compiler supports type "voidp"])
fi
if test "${ac_cv_type_ushort}" = "yes" ; then
  AC_DEFINE([HAVE_USHORT_LANG_C], [1], [C compiler supports type "ushort"])
fi
if test "${ac_cv_type_uint}" = "yes" ; then
  AC_DEFINE([HAVE_UINT_LANG_C], [1], [C compiler supports type "uint"])
fi
if test "${ac_cv_type_ulong}" = "yes" ; then
  AC_DEFINE([HAVE_ULONG_LANG_C], [1], [C compiler supports type "ulong"])
fi

AS_UNSET([ac_cv_sizeof_voidp])      # retest for C++
AS_UNSET([ac_cv_type_ushort])       # retest for C++
AS_UNSET([ac_cv_type_uint])         # retest for C++
AS_UNSET([ac_cv_type_ulong])        # retest for C++


#-------------------------------------------------
# C++ compiler checks
#-------------------------------------------------

AC_MSG_NOTICE([*** Using C++ comiler for tests ***])

AC_LANG([C++])

# Check for header files
#AC_HEADER_STDC()
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(inttypes.h)
AC_CHECK_HEADERS(cxxabi.h)
HPC_CHECK_CXX_STDC_HEADERS()

# Check for types
AC_CHECK_SIZEOF([void*])
AC_CHECK_TYPES([ushort, uint, ulong])

host_sizeof_voidp="${ac_cv_sizeof_voidp}"

# Check pthread.h
HPC_CHECK_COMPILE_PTHREAD_H()


#-------------------------------------------------
# Check for multilib
#-------------------------------------------------

# NOTE: can this be made more elegant?  This value should be used to
# automatically add/exclude lib32/lib64 paths below.
HAVE_OS_MULTILIB="no"
LIB_SEARCH_PATH="lib"
AC_MSG_CHECKING([for multilib platform])
case "${host}" in
  x86_64*-*-linux* | mips64*-*-linux* | powerpc64*-*-linux*)
    HAVE_OS_MULTILIB="yes"
    LIB_SEARCH_PATH="lib lib64 lib32"
    ;;
esac
if test "${HAVE_OS_MULTILIB}" = "yes" ; then
  AC_DEFINE([HAVE_OS_MULTILIB], [1], [HOST OS: 32 and 64 bit OS libraries])
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
# set unwinder variable
#-------------------------------------------------

UNWINDER=no

#---------------------------------------
# set specific libunw variable
#---------------------------------------
USE_LIBUNW=no

#-------------------------------------------------
# Host-specific defines
#-------------------------------------------------

# NOTE: To see what macros gcc-like compilers define:
#   gcc -dM -E - < /dev/null | sort
#
# FIXME: This should probably be split into separate CPU and OS
# sections.

HOST_OS=
HOST_CPU_MIPS=no
HOST_CPU_PPC=no
HOST_CPU_X86=no
HOST_CPU_X86_64=no
HOST_CPU_X86_FAMILY=no
HOST_CPU_IA64=no

case "${host}" in
  # Linux
  i386*-*-linux* | i686*-*-linux*)
    # __i386 && __linux
    HOST_OS="Linux"
    HOST_CPU_X86=yes
    HOST_CPU_X86_FAMILY=yes
    UNWINDER=x86-family
    AC_DEFINE([HOST_CPU_x86], [1], [HOST CPU: x86 (32-bit)])
    AC_DEFINE([HOST_OS_LINUX], [1], [HOST OS: Linux])
    ;;

  x86_64*-*-linux*)
    # __x86_64 && __linux
    HOST_OS="Linux"
    if test "${host_sizeof_voidp}" = "4"; then
      HOST_CPU_X86=yes
      AC_DEFINE([HOST_CPU_x86], [1], [HOST CPU: x86 (32-bit)])
    else
      HOST_CPU_X86_64=yes
      AC_DEFINE([HOST_CPU_x86_64], [1], [HOST CPU: x86-64])
    fi
    HOST_CPU_X86_FAMILY=yes
    UNWINDER=x86-family
    AC_DEFINE([HOST_OS_LINUX], [1], [HOST OS: Linux])
    ;;

  ia64*-*-linux*)
    # __ia64 && __linux
    HOST_OS="Linux"
    HOST_CPU_IA64=yes
    UNWINDER=libunw
    AC_DEFINE([HOST_CPU_IA64], [1], [HOST CPU: ia64 (itanium)])
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  mips64*-*-linux*)
    # __mips64 && __linux / _MIPS_SIM == _ABIN32 / _MIPS_SIM == _ABI64
    HOST_OS="Linux"
    HOST_CPU_MIPS=yes
    UNWINDER=mips
    AC_DEFINE([HOST_PLATFORM_MIPS64LE_LINUX], [1], [HOST platform: MIPS64LE_LINUX])
    AC_DEFINE([HOST_OS_LINUX], [1], [HOST OS: Linux])
    ;;

  powerpc64*-*-linux* | powerpc-*-linux*)
    # []
    HOST_OS="Linux"
    HOST_CPU_PPC=yes
    UNWINDER=ppc64
    AC_DEFINE([HOST_OS_LINUX], [1], [HOST OS: Linux])
    AC_DEFINE([HOST_CPU_PPC], [1], [HOST CPU: PowerPC (ppc)])
    ;;

  # MacOS
  powerpc*-*-darwin*)
    # __ppc__ / __MACH__
    HOST_OS="MacOS"
    HOST_CPU_PPC=yes
    UNWINDER=ppc64
    AC_DEFINE([HOST_OS_MACOS], [1], [HOST OS: MacOS])
    AC_DEFINE([HOST_CPU_PPC], [1], [HOST CPU: PowerPC (ppc)])
    ;;

  i386*-*-darwin* | i686*-*-darwin*)
    HOST_OS="MacOS"
    HOST_CPU_X86=yes
    UNWINDER=x86-family
    AC_DEFINE([HOST_CPU_x86], [1], [HOST CPU: x86 (32-bit)])
    AC_DEFINE([HOST_OS_MACOS], [1], [HOST OS: MacOS])
    ;;

  x86_64*-*-darwin*)
    HOST_OS="MacOS"
    if test "${host_sizeof_voidp}" = "4"; then
      HOST_CPU_X86=yes
      AC_DEFINE([HOST_CPU_x86], [1], [HOST CPU: x86 (32-bit)])
    else
      HOST_CPU_X86_64=yes
      AC_DEFINE([HOST_CPU_x86_64], [1], [HOST CPU: x86-64])
    fi
    HOST_CPU_X86_FAMILY=yes
    UNWINDER=x86-family
    AC_DEFINE([HOST_OS_MACOS], [1], [HOST OS: MacOS])
    ;;

  # IRIX
  mips*-*-irix*)
    # __mips && __sgi && __unix
    # _MIPS_ISA == _MIPS_ISA_MIPS1 | _MIPS_ISA == _MIPS_ISA_MIPS2
    # _MIPS_ISA == _MIPS_ISA_MIPS3 | _MIPS_ISA == _MIPS_ISA_MIPS4
    HOST_OS="IRIX"
    HOST_CPU_MIPS=yes
    UNWINDER=mips
    AC_DEFINE([HOST_OS_IRIX], [1], [HOST OS: IRIX])
    ;;

  # Solaris
  sparc*-*-solaris*)
    # __sparc && __sun && __unix
    HOST_OS="Solaris"
    AC_DEFINE([HOST_OS_SOLARIS], [1], [HOST OS: Solaris])
    ;;

  # Tru64/OSF
  alpha*-*-osf*)
    # __alpha / __digital__ && __unix__
    HOST_OS="Tru64"
    AC_DEFINE([HOST_OS_TRU64], [1], [HOST OS: Tru64])
    ;;

  *)
    AC_MSG_ERROR([HPCToolkit is not configured for HOST=${host}!])
    ;;
esac

HOST_CPU_K1OM=no
case "$host" in
  *k1om* ) HOST_CPU_K1OM=yes ;;
esac

AM_CONDITIONAL([HOST_OS_LINUX],   [test "${HOST_OS}" = Linux])
AM_CONDITIONAL([HOST_CPU_K1OM],   [test "${HOST_CPU_K1OM}" = yes])
AM_CONDITIONAL([HOST_CPU_MIPS],   [test "${HOST_CPU_MIPS}" = yes])
AM_CONDITIONAL([HOST_CPU_PPC],    [test "${HOST_CPU_PPC}" = yes])
AM_CONDITIONAL([HOST_CPU_X86],    [test "${HOST_CPU_X86}" = yes])
AM_CONDITIONAL([HOST_CPU_X86_64], [test "${HOST_CPU_X86_64}" = yes])
AM_CONDITIONAL([HOST_CPU_X86_FAMILY], [test "${HOST_CPU_X86_FAMILY}" = yes])
AM_CONDITIONAL([HOST_CPU_IA64], [test "$HOST_CPU_IA64" = yes])

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
HOST_HPCSTRUCT_LDFLAGS="-lm"
HOST_HPCPROF_LDFLAGS="-lm"
HOST_HPCPROF_FLAT_LDFLAGS="-lm"
HOST_HPCPROFTT_LDFLAGS="-lm"
HOST_XPROF_LDFLAGS=""
my_demangle_ldflags=""

# Options: Default for GCC
if test "$OPT_ENABLE_DEVELOP" = "yes" ; then
  my_opt_cflags="-g -O0 -Wall"
  my_opt_cxxflags="-g -O0 -Wall"
else 
  # On some compilers, -g implies -O0.  Make sure -O3 takes precedence!
  my_opt_cflags="-g -O3 -Wall"
  my_opt_cxxflags="-g -O3 -Wall"
fi


case "${host}" in

  # Linux x86
  i386*-*-linux* | i686*-*-linux* | x86_64*-*-linux*)
    # GCC
    if HPCcxxcmp([g++ c++]) ; then
      if test "${host_sizeof_voidp}" = "4"; then
        HOST_CXXFLAGS="-march=i486"
      fi
    fi
    if HPCcccmp([gcc cc]) ; then
      if test "${host_sizeof_voidp}" = "4"; then
        HOST_CFLAGS="-march=i486"
      fi
    fi

    # Intel compiler
    if HPCcxxcmp([icpc ecpc]) ; then
      # Silence the following warnings:
      #   remark #383: value copied to temporary, reference to temporary used
      #   remark #981: operands are evaluated in unspecified order
      #   remark #1572: FP equality and inequality comparisons are unreliable
      HOST_CXXFLAGS="-wd383 -wd981 -wd1572"
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
        my_opt_cflags="-gopt -O3 -c9x"
      fi
    fi
    ;;

  # Linux ia64
  ia64*-*-linux*)
    ;;

  # Linux mips
  mips64*-*-linux*)
    ;;

  # Linux power
  powerpc64*-*-linux*)
    # IBM compiler
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
        my_opt_cflags="-g3 -O3"
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


# FIXME: Boost >= 1.55 misreads the features of the g++ k1om compiler,
# so we add these flags as a workaround.  But this should be a compile
# test for the CXX compiler.

case "$host" in
  *k1om* )
      HOST_CXXFLAGS="$HOST_CXXFLAGS -DBOOST_NO_CXX11_ALLOCATOR"
      HOST_CXXFLAGS="$HOST_CXXFLAGS -DBOOST_NO_CXX11_VARIADIC_TEMPLATES"
      ;;
esac


#-------------------------------------------------
# Big or little endian and byteswap
#-------------------------------------------------

# Test if host is big or little endian with an option to override in
# the rare case that autoconf detects the wrong value.

AC_C_BIGENDIAN([default=big], [default=little], [default=none], [default=none])

AC_ARG_ENABLE([endian],
  [AS_HELP_STRING([--enable-endian],
      [host endianness (big, little), only needed if autoconf fails
       to detect correctly])],
  [host_endian="$enable_endian"],
  [host_endian="$default"])

case "$host_endian" in
  big )
    AC_DEFINE([HOST_BIG_ENDIAN], [1], [Host is big endian.])
    ;;
  little )
    AC_DEFINE([HOST_LITTLE_ENDIAN], [1], [Host is little endian.])
    ;;
  none )
    AC_MSG_WARN([Unable to autodetect host endianness.])
    AC_MSG_ERROR([Set --enable-endian to 'big' or 'little' and rerun configure.])
    ;;
  * )
    AC_MSG_WARN([Bad value for host endianness: $host_endian])
    AC_MSG_ERROR([Set --enable-endian to 'big' or 'little' and rerun configure.])
    ;;
esac

AC_MSG_NOTICE([host endian: $host_endian])

# Test if <byteswap.h> exists and defines bswap_64.  If yes, then it
# has assembly code for the fastest way to convert between big/little
# endian.

AC_LANG_PUSH([C++])
AC_LINK_IFELSE([
#include <stdio.h>
#include <stdint.h>
#include <byteswap.h>
int main(int argc, char **argv)
{
  uint64_t x = (uint64_t) argc;
  uint64_t y = bswap_64(x);
  return y > 2;
}
], [use_byteswap=yes], [use_byteswap=no])
AC_LANG_POP()

AC_MSG_NOTICE([use system byteswap.h: $use_byteswap])

if test "$use_byteswap" = yes ; then
  AC_DEFINE([USE_SYSTEM_BYTESWAP], [1], [Use system byteswap.h])
fi


#-------------------------------------------------
# CXXFLAGS for std C++11
#-------------------------------------------------

# Find a CXX flag for standard C++11 and add to CXXFLAGS if needed.
# We use some C++11 features (auto) in the parseAPI code in
# src/lib/banal.  Unfortunately, due to include files, we pretty much
# need this everywhere for C++ code.

AC_ARG_VAR([CXX11_FLAG],
  [CXX flag for C++11 standard, normally only needed for non-GNU CXX])

c11_list='argvar none -std=c++11 -std=c++0x -std=gnu++0x'
cxx_c11_avail=no
cxx_c11_flag=

ORIG_CXXFLAGS="$CXXFLAGS"
AC_LANG_PUSH([C++])

AC_MSG_CHECKING([CXXFLAGS for C++11 standard])

for opt in $c11_list
do
  case "$opt" in
    argvar ) flag="$CXX11_FLAG" ;;
    none )   flag=  ;;
    * )      flag="$opt" ;;
  esac
  CXXFLAGS="$HOST_CXXFLAGS $ORIG_CXXFLAGS $flag"
  AC_COMPILE_IFELSE([
#include <list>
int mylen(std::list <int> lst)
{
  int ans = 0;
  for (auto it = lst.begin(); it != lst.end(); it++) {
    ans += 1;
  }
  return ans;
}
], [ans=yes], [ans=no])
  if test "$ans" = yes ; then
    cxx_c11_avail=yes
    cxx_c11_flag="$flag"
    break
  fi
done

AC_MSG_RESULT([$cxx_c11_flag])

AC_LANG_POP

if test "$cxx_c11_avail" = no ; then
  AC_MSG_WARN([unable to find CXXFLAGS for std C++11])
  AC_MSG_ERROR([set the CXX11_FLAG variable and rerun configure])
fi

CXXFLAGS="$ORIG_CXXFLAGS"
HOST_CXXFLAGS="$HOST_CXXFLAGS $cxx_c11_flag"

AC_SUBST([cxx_c11_flag])


#-------------------------------------------------
# LD -- find correct arch type for ld
#-------------------------------------------------

# Autoconf gets the wrong arch type for ld in a cross compile, at
# least on Intel MIC.  Here, we test that ld supports relocatable
# linking (ld -r).  If not, then try other arch types until we find
# one that works.

test_ld_r()
{
  ld_cmd="$*"
  AC_MSG_CHECKING([relocatable linking: $ld_cmd ])
  rm -f "$obj3"
  $ld_cmd -r "$obj1" "$obj2" -o "$obj3" >&AS_MESSAGE_LOG_FD 2>&1
  if test $? -eq 0 && test -f "$obj3" ; then
    AC_MSG_RESULT(yes)
    return 0
  else
    AC_MSG_RESULT(no)
    return 1
  fi
}

src1="conftest.$$.1.c"
src2="conftest.$$.2.c"
obj1="conftest.$$.1.o"
obj2="conftest.$$.2.o"
obj3="conftest.$$.3.o"

rm -f "$src1" "$src2" "$obj1" "$obj2" "$obj3"

echo 'int foo(void) { return 2; }' >"$src1"
echo 'int baz(void) { return 4; }' >"$src2"

$CC $CFLAGS -c "$src1" -o "$obj1" >&AS_MESSAGE_LOG_FD 2>&1
$CC $CFLAGS -c "$src2" -o "$obj2" >&AS_MESSAGE_LOG_FD 2>&1

found_ld=no

if test_ld_r $LD ; then
  # autoconf's choice for LD works
  found_ld=yes
else
  # try other arch types from LD -V
  # be careful not to create a subshell (redirect fd)
  ld_base=`echo $LD | awk '{ print $1 }'`

  emul=false
  arch_list=`$ld_base -V 2>&1`
  for arch in $arch_list
  do
    if $emul && test_ld_r $ld_base -m $arch ; then
      LD="$ld_base -m $arch"
      found_ld=yes
      break
    fi
    # skip output until 'Supported emulations:'
    case "$arch" in
      *emulat* ) emul=true ;;
    esac
  done
fi

rm -f "$src1" "$src2" "$obj1" "$obj2" "$obj3"

if test "$found_ld" != yes ; then
  AC_MSG_ERROR([unable to find correct arch type for ld])
fi


#-------------------------------------------------
# Test if CXX and MPICXX are compatible
#-------------------------------------------------

# Test CXX and MPICXX compatibility by compiling a short program with
# CXX and linking it with MPICXX.  Basically, we require that CXX is
# the underlying compiler for MPICXX and this is how we test for that.
# Note: the Autoconf macros would delete the object files, so we need
# to run the compile and link steps ourself.

base="hpc-conftest-$$"
hello_src="$base".cpp
hello_o="$base".o
hello_bin="$base".bin
hello_out="$base".out
ACTUAL_CXXFLAGS="$CXXFLAGS $HOST_CXXFLAGS"

if test -n "$MPICXX" ; then
  AC_MSG_CHECKING([whether CXX and MPICXX are compatible])

  rm -f "$hello_src" "$hello_o" "$hello_bin" "$hello_out"
  cat <<EOF >"$hello_src"
#include <iostream>
#include <string>
int main()
{
  std::cout << "hello, world\n";
  return 0;
}
EOF
  $CXX -c $ACTUAL_CXXFLAGS -o "$hello_o" "$hello_src" >/dev/null 2>&1
  $MPICXX -o "$hello_bin" "$hello_o" >"$hello_out" 2>&1

  if test $? -eq 0 && test -f "$hello_bin" ; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    AC_MSG_WARN([MPICXX does not link with CXX (see config.log for details)])
    AC_MSG_WARN([probably MPICXX does match CXX for compiler family or version])
    cat <<EOF >&AS_MESSAGE_LOG_FD
------------------------------------------------------------
CXX:     $CXX $ACTUAL_CXXFLAGS
MPICXX:  $MPICXX

EOF
    cat "$hello_src" >&AS_MESSAGE_LOG_FD
    echo >&AS_MESSAGE_LOG_FD
    cat "$hello_out" >&AS_MESSAGE_LOG_FD
    cat <<EOF >&AS_MESSAGE_LOG_FD
------------------------------------------------------------
EOF
  MPICXX=
  fi

  rm -f "$hello_src" "$hello_o" "$hello_bin" "$hello_out"

else
  AC_MSG_NOTICE([skipping MPICXX compatibility check (MPICXX not set)])
fi


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

AC_MSG_NOTICE([*** INFO: post hpc tests: CXX='${CXX}' CXXFLAGS='${CXXFLAGS}' HOST_CXXFLAGS='${HOST_CXXFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: post hpc tests: CC='${CC}' CFLAGS='${CFLAGS}' HOST_CFLAGS='${HOST_CFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: post hpc tests: MPICC='${MPICC}' ***])
AC_MSG_NOTICE([*** INFO: post hpc tests: MPICXX='${MPICXX}' ***])
AC_MSG_NOTICE([*** INFO: post hpc tests: MPIF77='${MPIF77}' ***])

#----------------------------------------------------------------------------
# Prerequisites: external libraries
#----------------------------------------------------------------------------

#
# Directory relative to hpctoolkit's prefix where we put external
# package libraries, if we want to copy them.
#
hpc_ext_libs_dir=lib/hpctoolkit/ext-libs
AC_SUBST([hpc_ext_libs_dir])


#-------------------------------------------------
# Option: --with-externals=PATH
#
# If specified, then use PATH as the root of the external prereqs tree
# and the file PATH/externs.conf for the locations of binutils, open
# analysis, xerces, etc.  May be absolute or relative path.
#
# Note: using externals is not strictly necessary, but if not used,
# then you'll need to build all of hpctoolkit's prereqs manually and
# pass them all to configure.
#-------------------------------------------------

ext_cc_path=unknown
ext_cxx_path=unknown
ext_wordsize=unknown

warn_no_externals=no

AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([], [])])
AC_ARG_WITH([externals],
  [AS_HELP_STRING([--with-externals=PATH],
	  [path to hpctoolkit-externals build or install directory
	   (normally required)])],
  [],
  [with_externals=no])

if test "${with_externals}" != "no"; then
  ext_file="${with_externals}/externs.conf"
  if test ! -f "$ext_file" ; then
    AC_MSG_ERROR([missing externals file: $ext_file])
  fi
  AC_MSG_NOTICE([using externals tree: $with_externals])
  externals_root=`( cd "$with_externals" && pwd )`
  . "$ext_file"
  use_externals=yes
else
  AC_MSG_WARN([not using --with-externals])
  use_externals=no
  warn_no_externals=yes
fi


#-------------------------------------------------
# Check consistency of C, C++ and wordsize
#-------------------------------------------------

# Verify that toolkit and externals use the same C and C++ compilers
# and that they produce the same wordsize.  This is especially
# important for C++ and libstdc++.  For now, give a pass if externals
# isn't new enough to export these variables.

CC_VERSION='unknown non-gnu'
version=`$CC --version 2>/dev/null`
if echo "$version" | grep -i copyright >/dev/null ; then
  CC_VERSION=`echo "$version" | head -1`
fi

CC_PATH=
base=`echo $CC | awk '{ print $1 }'`
case "$base" in
  /* ) CC_PATH="$base" ;;
  * )  AC_PATH_PROG([CC_PATH], [$base]) ;;
esac
if test "x$CC_PATH" = x ; then
  CC_PATH=unknown
fi

warn_non_gcc=no
echo $version | grep -E -i -e 'copy.*free.*soft.*found' >/dev/null
if test $? -ne 0 ; then
  warn_non_gcc=yes
  AC_MSG_WARN([Not using the GNU C compiler.])
fi

CXX_VERSION='unknown non-gnu'
version=`$CXX --version 2>/dev/null`
if echo "$version" | grep -i copyright >/dev/null ; then
  CXX_VERSION=`echo "$version" | head -1`
fi

CXX_PATH=
base=`echo $CXX | awk '{ print $1 }'`
case "$base" in
  /* ) CXX_PATH="$base" ;;
  * )  AC_PATH_PROG([CXX_PATH], [$base]) ;;
esac
if test "x$CXX_PATH" = x ; then
  CXX_PATH=unknown
fi

echo $version | grep -E -i -e 'copy.*free.*soft.*found' >/dev/null
if test $? -ne 0 ; then
  warn_non_gcc=yes
  AC_MSG_WARN([Not using the GNU C++ compiler.])
fi

AC_MSG_NOTICE([C compiler: $CC_VERSION])
AC_MSG_NOTICE([C++ compiler: $CXX_VERSION])

warn_compiler_mismatch=no
if test "$ext_cc_path" != unknown && test "$ext_cc_path" != "$CC_PATH"
then
  warn_compiler_mismatch=yes
  AC_MSG_WARN([The C compiler does not match the one used to build \
hpctoolkit-externals.])
fi
if test "$ext_cxx_path" != unknown && test "$ext_cxx_path" != "$CXX_PATH"
then
  warn_compiler_mismatch=yes
  AC_MSG_WARN([The C++ compiler does not match the one used to build \
hpctoolkit-externals.])
fi

host_wordsize=unknown
wordsize_cflag=
case "$host_sizeof_voidp" in
  4 ) host_wordsize=32 ; wordsize_cflag='-m32' ;;
  8 ) host_wordsize=64 ; wordsize_cflag='-m64' ;;
  * ) AC_MSG_WARN([unable to determine host wordsize]) ;;
esac

if test "$ext_wordsize" != unknown && test "$host_wordsize" != "$ext_wordsize"
then
  AC_MSG_ERROR([mismatched wordsize: hpctoolkit is configured for \
${host_wordsize}-bit but externals is ${ext_wordsize}-bit])
fi

# On ia64, -m64 is an unrecognized option (and unnecessary).
if test "$HOST_CPU_IA64" = yes ; then
  wordsize_cflag=
fi

AC_SUBST([wordsize_cflag])


#-------------------------------------------------
# Check for GNU's binutils (binary interface)
#-------------------------------------------------
# cf. enable-hpc-binutils

AC_MSG_CHECKING([for GNU binutils])

GNUBINUTILS=no
if test "$use_externals" = yes ; then
  GNUBINUTILS="$ext_binutils_prefix"
fi

AC_ARG_WITH([binutils],
  AS_HELP_STRING([--with-binutils=PATH],
                 [use given GNU binutils installation]),
  [if test $withval != no; then
     GNUBINUTILS=${withval}
   fi],
  [])

# Note: With binutils 2.20.1, libiberty may be installed in a
# different lib than libbfd and libopcodes (e.g., lib64 vs. lib).
#
# Note: With prior versions of binutils and libtool, we had to supply
# an absolute path for libiberty.  Will this suffice?
GNUBINUTILS_IFLAGS="-I${GNUBINUTILS}/include"

GNUBINUTILS_LDFLAGS="-L${GNUBINUTILS}/lib"
if test "${host_sizeof_voidp}" = "8"; then
  # -lbfd -lopcodes are in lib; -liberty is in lib64
  GNUBINUTILS_LDFLAGS="${GNUBINUTILS_LDFLAGS} -L${GNUBINUTILS}/lib64"
fi

GNUBINUTILS_LDLIBS="-lbfd -lopcodes -liberty"

# on non-Linux systems we build libintl
if test "${HOST_OS}" != "Linux"; then
  GNUBINUTILS_LDLIBS="${GNUBINUTILS_LDLIBS} ${GNUBINUTILS}/lib/libintl.a"
fi


AC_MSG_RESULT([${GNUBINUTILS}])
AC_SUBST([GNUBINUTILS_IFLAGS])
AC_SUBST([GNUBINUTILS_LDFLAGS])
AC_SUBST([GNUBINUTILS_LDLIBS])


#-------------------------------------------------
# Option: --with-boost=PATH
#-------------------------------------------------

BOOST=no
if test "$use_externals" = yes ; then
  BOOST="$ext_boost_prefix"
fi

AC_ARG_WITH([boost],
  [AS_HELP_STRING([--with-boost=PATH],
      [path to boost install directory])],
  [BOOST="$withval"],
  [])

AC_MSG_CHECKING([for boost])

case "$BOOST" in
  /* )
    if test ! -f "${BOOST}/include/boost/shared_ptr.hpp" ; then
      AC_MSG_ERROR([invalid boost directory: $BOOST])
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([boost directory must be absolute path: $BOOST])
    ;;
esac

AC_MSG_RESULT([$BOOST])

BOOST_INC="${BOOST}/include"

AC_SUBST([BOOST_INC])


#-------------------------------------------------
# Option: --with-libdwarf=PATH
#-------------------------------------------------

LIBDWARF=no
if test "$use_externals" = yes ; then
    LIBDWARF="$ext_libdwarf_prefix"
fi

AC_ARG_WITH([libdwarf],
  [AS_HELP_STRING([--with-libdwarf=PATH],
  	    [path to libdwarf install directory])],
  [LIBDWARF="$withval"],
  [])

AC_MSG_CHECKING([for libdwarf])

case "${LIBDWARF}" in
  /* )
    if test -f "${LIBDWARF}/include/libdwarf.h" ; then :
    else
        AC_MSG_ERROR([invalid libdwarf directory: $LIBDWARF])
    fi
    LIBDWARF_COPY=yes
    ;;
  no )
    LIBDWARF_COPY=no
    ;;
  * )
    AC_MSG_ERROR([libdwarf directory must be absolute path: $LIBDWARF])
    ;;
esac

LIBDWARF_LIB="${LIBDWARF}/lib"
LIBDWARF_INC="${LIBDWARF}/include"

AC_MSG_RESULT([$LIBDWARF])

AC_SUBST([LIBDWARF_LIB])
AC_SUBST([LIBDWARF_INC])
AC_SUBST([LIBDWARF_COPY])


#-------------------------------------------------
# Option: --with-libelf=PATH
#-------------------------------------------------

LIBELF=no
if test "$use_externals" = yes ; then
  LIBELF="$ext_libelf_prefix"
fi

AC_ARG_WITH([libelf],
  [AS_HELP_STRING([--with-libelf=PATH],
  	    [path to libelf install directory])],
  [LIBELF="$withval"],
  [])

AC_MSG_CHECKING([for libelf])

valid_libelf_dir()
{
    test -f "$1/include/libelf.h" || test -f "$1/include/libelf/libelf.h"
}

case "${LIBELF}" in
  /* )
    if valid_libelf_dir "$LIBELF" ; then :
    else
        AC_MSG_ERROR([invalid libelf directory: $LIBELF])
    fi
    LIBELF_COPY=yes
    ;;
  no )
    LIBELF_COPY=no
    ;;
  * )
    AC_MSG_ERROR([libelf directory must be absolute path: $LIBELF])
    ;;
esac

LIBELF_LIB="${LIBELF}/lib"
LIBELF_INC="${LIBELF}/include"

AC_MSG_RESULT([$LIBELF])

AC_SUBST([LIBELF_LIB])
AC_SUBST([LIBELF_INC])
AC_SUBST([LIBELF_COPY])


#-------------------------------------------------
# Option: --with-libmonitor=PATH
#-------------------------------------------------

LIBMONITOR=no
OPT_LIBMONITOR_DYNAMIC=no
OPT_LIBMONITOR_STATIC=no
if test "$use_externals" = yes ; then
  LIBMONITOR="$ext_libmonitor_prefix"
fi

AC_ARG_WITH([libmonitor],
  [AS_HELP_STRING([--with-libmonitor=PATH],
  	    [path to libmonitor install directory])],
  [LIBMONITOR="$withval"],
  [])

AC_MSG_CHECKING([for libmonitor])

case "${LIBMONITOR}" in
  /* )
    LIBMONITOR_INC="${LIBMONITOR}/include"
    LIBMONITOR_BIN=${LIBMONITOR}/bin
    LIBMONITOR_LIB=${LIBMONITOR}/lib
    if test -f "${LIBMONITOR_INC}/monitor.h" ; then :
    else
      AC_MSG_ERROR([invalid libmonitor directory: $LIBMONITOR])
    fi
    LIBMONITOR_LIB="${LIBMONITOR}/lib"
    if test -f "${LIBMONITOR_LIB}/libmonitor.so" ; then
      OPT_LIBMONITOR_DYNAMIC=yes
    fi
    wrap_file="${LIBMONITOR_LIB}/libmonitor_wrap.a"
    if test -f "$wrap_file" ; then
      OPT_LIBMONITOR_STATIC=yes
      LIBMONITOR_WRAP_NAMES=`nm "$wrap_file" | grep __wrap_ |
        $SED -e 's/.*__wrap_//' | tr '\n' ' '`
    fi
    LIBMONITOR_COPY=yes
    LIBMONITOR_RUN_DIR="$hpc_ext_libs_dir"
    ;;
  no )
    LIBMONITOR_COPY=no
    LIBMONITOR_RUN_DIR=no
    ;;
  * )
    AC_MSG_ERROR([libmonitor directory must be absolute path: $LIBMONITOR])
    ;;
esac

AC_MSG_RESULT([$LIBMONITOR])

echo "libmonitor wrap names:" >&AS_MESSAGE_LOG_FD
echo "$LIBMONITOR_WRAP_NAMES" >&AS_MESSAGE_LOG_FD

AC_SUBST([LIBMONITOR_INC])
AC_SUBST([LIBMONITOR_LIB])
AC_SUBST([LIBMONITOR_COPY])
AC_SUBST([LIBMONITOR_RUN_DIR])
AC_SUBST([LIBMONITOR_WRAP_NAMES])


#-------------------------------------------------
# with-libunwind
#-------------------------------------------------

#
# ### FIXME: rationalize this: libunw chosen EITHER when ia64 or when --with-libunwind is set
# ###        current configure will NOT do the right thing if libunw turned off but arch=ia64
#
AC_MSG_CHECKING([whether configured to use libunwind instead of native])

OPT_HAVE_LIBUNWIND=no
OPT_LIBUNWIND=
OPT_LIBUNWIND_IFLAGS=
OPT_LIBUNWIND_LIBPATH=
OPT_LIBUNWIND_LDFLAGS=
OPT_LIBUNWIND_LIBSO=
OPT_LIBUNWIND_LIBA=
LIBUNWIND_COPY=no
LIBUNWIND_RUN_DIR=
LIBUNWIND_HPCLINK_LIBS=

if test "$use_externals" = yes ; then
  OPT_LIBUNWIND="$ext_libunwind_prefix"
  case "$OPT_LIBUNWIND" in
    /* ) OPT_HAVE_LIBUNWIND=yes ;;
  esac
fi

AC_ARG_WITH([libunwind],
  AS_HELP_STRING([--with-libunwind=PATH],
                 [use given LIBUNWIND installation (absolute path) with hpcrun (default is NO)]),
  [case $withval in
      yes) : ;;
      /*) OPT_LIBUNWIND=${withval} ;;
      *) AC_MSG_ERROR([--with-libunwind requires absolute path as argument; given '${withval}']) ;;
   esac
   OPT_HAVE_LIBUNWIND=yes
   UNWINDER=libunw],
  [])

if test "$UNWINDER" = "libunw"; then
   AC_DEFINE([USE_LIBUNW], [1], [libunwind is the specified unwinder])
fi

# Check for LIBUNWIND > .9xxx [skip for now]

# Check for libunwind sanity
if test "${OPT_HAVE_LIBUNWIND}" = "yes"; then
  if ! test -f "$OPT_LIBUNWIND/include/libunwind.h"; then
     AC_MSG_ERROR([libunwind required, but specified library path is invalid (no libunwind.h)])
  fi
  if test ! \( -f "$OPT_LIBUNWIND/lib/libunwind.so" -o -f "$OPT_LIBUNWIND/lib/libunwind.a" \) ; then
     AC_MSG_ERROR([libunwind required, but specified library path is invalid (no libunwind.a or libunwind.so)])
  fi
fi

if test "${OPT_HAVE_LIBUNWIND}" = "yes"; then 
  AC_MSG_RESULT([yes (${OPT_LIBUNWIND_LIBSO})])

  LIBUNWIND_COPY=yes
  LIBUNWIND_LIB="$OPT_LIBUNWIND/lib"
  LIBUNWIND_RUN_DIR="$hpc_ext_libs_dir"
  OPT_LIBUNWIND_IFLAGS="-I$OPT_LIBUNWIND/include"
  OPT_LIBUNWIND_LIBPATH="$OPT_LIBUNWIND"

  LIBUNWIND_HPCLINK_LIBS=" -lunwind -lunwind-generic"

  OPT_LIBUNWIND_LDFLAGS="-L$LIBUNWIND_LIB $LIBUNWIND_HPCLINK_LIBS"

else
  AC_MSG_RESULT([no])
fi

if test -z "$HPCLINK_LD_FLAGS"; then
   HPCLINK_LD_FLAGS="$OPT_LIBUNWIND_LDFLAGS"
else
   HPCLINK_LD_FLAGS="$HPCLINK_LD_FLAGS $OPT_LIBUNWIND_LDFLAGS"
fi

AM_CONDITIONAL(OPT_HAVE_LIBUNWIND, test "${OPT_HAVE_LIBUNWIND}" = "yes")
AC_SUBST(OPT_LIBUNWIND_IFLAGS)
AC_SUBST(OPT_LIBUNWIND_LIBPATH)
AC_SUBST(OPT_LIBUNWIND_LDFLAGS)
AC_SUBST(OPT_LIBUNWIND_LIBSO)
AC_SUBST(OPT_LIBUNWIND_LIBA)
AC_SUBST(LIBUNWIND_COPY)
AC_SUBST(LIBUNWIND_LIB)
AC_SUBST(LIBUNWIND_RUN_DIR)
AC_SUBST(LIBUNWIND_HPCLINK_LIBS)

#
# set up automake choice of unwinder
#
# -- NOTE: ia64 ALWAYS uses libunw
#
AM_CONDITIONAL([UNW_X86], [test "$UNWINDER" = x86-family])
AM_CONDITIONAL([UNW_PPC64], [test "$UNWINDER" = ppc64])
AM_CONDITIONAL([UNW_MIPS], [test "$UNWINDER" = mips])
AM_CONDITIONAL([UNW_LIBUNW], [test "$UNWINDER" = libunw])

#-------------------------------------------------
# with-monitor
#-------------------------------------------------

AC_MSG_CHECKING([for monitor])

OPT_WITH_MONITOR=no
OPT_INSTALL_MONITOR=no
MONITOR=
MONITOR_LIBS=
HPC_MONITOR=

# By default, disable old-monitor and hpcrun-flat in a cross compile.
# It can be reenabled with an explicit --with-monitor option.

if test "$use_externals" = yes && test -z "$is_cross_compile"
then
  MONITOR="$ext_old_monitor_prefix"
  case "$MONITOR" in
    /* )  OPT_WITH_MONITOR=yes
	  OPT_INSTALL_MONITOR=yes
	  ;;
  esac
fi

AC_ARG_WITH([monitor],
  AS_HELP_STRING([--with-monitor=PATH],
                 [use given (old) libmonitor installation]),
  [if test $withval != no; then
     OPT_WITH_MONITOR=yes
     OPT_INSTALL_MONITOR=yes
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
if test "${OPT_INSTALL_MONITOR}" = "yes" ; then
  HPC_MONITOR="hpctoolkit/monitor"  # lib/...
else 
  HPC_MONITOR="${MONITOR}"
fi

if test "${OPT_WITH_MONITOR}" = "yes" ; then
  AC_DEFINE([HAVE_MONITOR], [1], [Monitor library])
  AC_MSG_RESULT([yes (${MONITOR_LIBS})])
else
  AC_MSG_RESULT([no (not needed)])
fi

AC_SUBST([MONITOR])
AC_SUBST([HPC_MONITOR])
AC_SUBST([OPT_INSTALL_MONITOR])


#-------------------------------------------------
# Check for Open Analysis (CFG builder, etc.)
#-------------------------------------------------

OA=no
OA_IFLAGS=
OA_LDFLAGS=
OA_AVAIL=no

if test "$use_externals" = yes ; then
  OA="$ext_open_analysis_prefix"
fi

AC_ARG_WITH([open-analysis],
  [AS_HELP_STRING([--with-open-analysis=PATH],
      [path to OpenAnalysis install directory])],
  [OA="$withval"],
  [])

AC_MSG_CHECKING([for OpenAnalysis])

case "$OA" in
  /* )
    if test -f "${OA}/include/OpenAnalysis/Utils/RIFG.hpp" \
       && test -f "${OA}/lib/libOAsz64.a"
    then :
    else
      AC_MSG_ERROR([invalid OpenAnalysis directory: $OA])
    fi
    OA_IFLAGS="-DOA_IRHANDLETYPE_SZ64 -I${OA}/include"
    OA_LDFLAGS="${OA}/lib/libOAsz64.a"
    OA_AVAIL=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([OpenAnalysis directory must be absolute path: $OA])
    ;;
esac

AC_MSG_RESULT([$OA])

AC_SUBST([OA_IFLAGS])
AC_SUBST([OA_LDFLAGS])


#-------------------------------------------------
# with-papi
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with PAPI])

#HPC_DEF_CHECK_CXX_PAPI_LINK

OPT_HAVE_PAPI=no
OPT_PAPI_DYNAMIC=no
OPT_PAPI_STATIC=no
OPT_PAPI=
OPT_PAPI_IFLAGS=
OPT_PAPI_LDFLAGS=
OPT_PAPI_LIBPATH=

AC_ARG_WITH([papi],
  AS_HELP_STRING([--with-papi=PATH],
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
#
if test "$OPT_HAVE_PAPI" = "yes" ; then
  if test -f "${OPT_PAPI}/include/papi.h"; then
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}/include"
  elif test -f "${OPT_PAPI}/papi.h" ; then
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}"
  else
    AC_MSG_ERROR([no papi.h in ${OPT_PAPI}/include])
  fi
  
  for lib in ${LIB_SEARCH_PATH} ; do
    if test -r "${OPT_PAPI}/${lib}/libpapi.so" \
       || test -f "${OPT_PAPI}/${lib}/libpapi.a"
    then
      OPT_PAPI_LIBPATH="${OPT_PAPI}/${lib}"
      OPT_PAPI_LDFLAGS="-L${OPT_PAPI_LIBPATH} -lpapi"
      if test -f "${OPT_PAPI}/${lib}/libpapi.so" ; then
	OPT_PAPI_DYNAMIC=yes
      fi
      if test -f "${OPT_PAPI}/${lib}/libpapi.a" ; then
	OPT_PAPI_STATIC=yes
      fi
      break
    fi
  done
  if test -z "${OPT_PAPI_LIBPATH}" ; then
    AC_MSG_ERROR([no libpapi.so or libpapi.a in $LIB_SEARCH_PATH])
  fi
fi

# Check for PAPI > 3.0 [skip for now]

if test "${OPT_HAVE_PAPI}" = "yes"; then
  AC_MSG_RESULT([yes (${OPT_PAPI})])
  AC_MSG_NOTICE([PAPI shared libpapi.so: $OPT_PAPI_DYNAMIC])
  AC_MSG_NOTICE([PAPI static libpapi.a: $OPT_PAPI_STATIC])
else
  AC_MSG_RESULT([no])
fi

# The dynamic case also needs libpapi.so's prerequisite libraries
# (-lperfctr, -lpfm).  In the static case, libpapi.a already includes
# these functions.  At least, that's what we've seen so far.
#
papi_extra_libs=
so_file="${OPT_PAPI_LIBPATH}/libpapi.so"
if test -f "$so_file" ; then
  ldd "$so_file" | grep libperfctr >/dev/null 2>&1
  if test $? = 0 ; then
    papi_extra_libs="${papi_extra_libs} -lperfctr"
  fi
  ldd "$so_file" | grep libpfm >/dev/null 2>&1
  if test $? = 0 ; then
    papi_extra_libs="${papi_extra_libs} -lpfm"
  fi
fi

AM_CONDITIONAL(OPT_PAPI_DYNAMIC, test "$OPT_PAPI_DYNAMIC" = yes)
AM_CONDITIONAL(OPT_PAPI_STATIC,  test "$OPT_PAPI_STATIC" = yes)

AC_SUBST(OPT_PAPI)
AC_SUBST(OPT_PAPI_IFLAGS)
AC_SUBST(OPT_PAPI_LDFLAGS)
AC_SUBST(OPT_PAPI_LIBPATH)
AC_SUBST([papi_extra_libs])

# Option to use the new papi-c.c sample source file.  This is mostly
# temporary until the new file integrates both component and
# non-component PAPI.  Requires PAPI 5.1 or later.

AC_ARG_ENABLE([papi-c],
  AS_HELP_STRING([--enable-papi-c],
      [use new papi component sample source, requires papi 5.1 or later]),
  [use_papi_c="$enableval"],
  [use_papi_c=check])

AC_MSG_CHECKING([whether to use new papi component])

case "$use_papi_c" in
  no | check ) ;;
  * ) use_papi_c=yes ;;
esac

if test "$OPT_HAVE_PAPI" = no ; then
  use_papi_c=no
fi

if test "$use_papi_c" = check ; then
  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="$CFLAGS $OPT_PAPI_IFLAGS"
  LIBS="$OPT_PAPI_LDFLAGS"
  AC_LANG_PUSH([C])

  AC_LINK_IFELSE([
#include "papi.h"
int main()
{
  return PAPI_get_eventset_component(0);
}
], [use_papi_c=yes], [use_papi_c=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"
fi

AC_MSG_RESULT([$use_papi_c])

AM_CONDITIONAL(OPT_PAPI_COMPONENT, [test "$use_papi_c" = yes])

AC_ARG_ENABLE([papi-c-cupti],
  AS_HELP_STRING([--enable-papi-c-cupti],
      [enable component PAPI's CUPTI functionality (requires papi-c with cuda component installed]),
  [use_papi_c_cupti="$enableval"],
  [use_papi_c_cupti=no])

AC_MSG_CHECKING([whether to use component papi CUPTI functionality])

case "$use_papi_c_cupti" in
  no ) ;;
  * ) use_papi_c_cupti=yes ;;
esac

if test "$use_papi_c" = no ; then
  use_papi_c_cupti=no
fi

if test "$use_papi_c_component" = yes ; then
  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="$CFLAGS $OPT_PAPI_IFLAGS"
  LIBS="$OPT_PAPI_LDFLAGS"
  AC_LANG_PUSH([C])

  AC_LINK_IFELSE([
#include "papi.h"
extern void CUDA_init_component(void);
int main()
{
  CUDA_init_component();
}
], [use_papi_c_cupti=yes], [use_papi_c_cupti=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"
fi

AC_MSG_RESULT([$use_papi_c_cupti])
AM_CONDITIONAL(OPT_PAPI_CUPTI, [test "$use_papi_c_cupti" = yes])

#-------------------------------------------------
# Option: --with-symtabAPI=PATH
#-------------------------------------------------

SYMTABAPI=no
if test "$use_externals" = yes ; then
  SYMTABAPI="$ext_symtabAPI_prefix"
fi

AC_ARG_WITH([symtabAPI],
  [AS_HELP_STRING([--with-symtabAPI=PATH],
      [path to symtabAPI install directory])],
  [SYMTABAPI="$withval"],
  [])

SYMTABAPI_COPY=no
SYMTABAPI_COPY_DYN=no
SYMTABAPI_COPY_PARSE=no
SYMTABAPI_LIB_LIST=

AC_MSG_CHECKING([for symtabAPI])

case "${SYMTABAPI}" in
  /* )
    if test -f "${SYMTABAPI}/include/Symtab.h" && \
       test -f "${SYMTABAPI}/lib/libsymtabAPI.so"
    then :
    else
        AC_MSG_ERROR([invalid symtabAPI directory: $SYMTABAPI])
    fi
    SYMTABAPI_COPY=yes
    SYMTABAPI_LIB_LIST='-lsymtabAPI -lcommon'
    if test -f "${SYMTABAPI}/lib/libdynElf.so" ; then
      SYMTABAPI_COPY_DYN=yes
      SYMTABAPI_LIB_LIST="$SYMTABAPI_LIB_LIST -ldynDwarf -ldynElf"
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([symtabAPI directory must be absolute path: $SYMTABAPI])
    ;;
esac

AC_MSG_RESULT([$SYMTABAPI])

AC_MSG_CHECKING([for parseAPI])

PARSEAPI_AVAIL=no

if test "$SYMTABAPI" != no \
   && test -f "${SYMTABAPI}/include/CFG.h" \
   && test -f "${SYMTABAPI}/lib/libparseAPI.so"
then
  SYMTABAPI_COPY_PARSE=yes
  PARSEAPI_AVAIL=yes
fi

AC_MSG_RESULT([$PARSEAPI_AVAIL])

SYMTABAPI_INC="${SYMTABAPI}/include"
SYMTABAPI_LIB="${SYMTABAPI}/lib"

AC_SUBST([SYMTABAPI_INC])
AC_SUBST([SYMTABAPI_LIB])
AC_SUBST([SYMTABAPI_LIB_LIST])
AC_SUBST([SYMTABAPI_COPY])
AC_SUBST([SYMTABAPI_COPY_DYN])
AC_SUBST([SYMTABAPI_COPY_PARSE])


#-------------------------------------------------
# Option: --with-xed2=PATH
#-------------------------------------------------

# Now use static and shared libxed.  hpcrun requires .so or .a + fPIC,
# hpclink requires .a, and fnbounds, etc can use either.  hpcprof-mpi
# requires .a, but only if fully static (Cray).
#
# We support both old XED (2013) with single .a + fPIC and new (2015)
# XED with separate .so and .a without fPIC, so we test if .a can link
# into a shared library.

XED2=no
XED2_INC=
XED2_LIB_DIR=no
XED2_LIB_FLAGS=
XED2_HPCRUN_LIBS=
XED2_HPCLINK_LIBS=
XED2_PROF_MPI_LIBS=
XED2_COPY=no
no_xed_for_hpcrun=no
no_xed_for_hpclink=no

if test "$use_externals" = yes ; then
  XED2="$ext_xed2_prefix"
fi

AC_ARG_WITH([xed2],
  [AS_HELP_STRING([--with-xed2=PATH],
  	    [path to xed2 install directory])],
  [XED2="$withval"],
  [])

# initial test that include and lib files exist

AC_MSG_CHECKING([for xed2])

xed2_avail=no
case "$XED2" in
  /* )
    if test ! -f "${XED2}/include/xed-interface.h" ; then
      AC_MSG_ERROR([invalid xed2 directory: $XED2])
    fi
    if test ! -f "${XED2}/lib/libxed.so" && test ! -f "${XED2}/lib/libxed.a" ; then
      AC_MSG_ERROR([invalid xed2 directory: $XED2])
    fi
    XED2_INC="${XED2}/include"
    XED2_LIB_DIR="${XED2}/lib"
    XED2_COPY=yes
    xed2_avail=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([xed2 directory must be absolute path: $XED2])
    ;;
esac

AC_MSG_RESULT([$XED2])

if test "$HOST_CPU_X86_FAMILY" = yes && test "$XED2" = no ; then
  AC_MSG_WARN([xed2 is required for x86 platforms])
  no_xed_for_hpcrun=yes
  no_xed_for_hpclink=yes
fi

if test "$xed2_avail" = yes ; then
  #
  # test if libxed.a was compiled with -fPIC
  #
  xed2_static_fpic=no
  if test -f "${XED2}/lib/libxed.a" ; then
    ORIG_CFLAGS="$CFLAGS"
    ORIG_LIBS="$LIBS"
    CFLAGS="-shared -fPIC -I${XED2}/include"
    LIBS="${XED2}/lib/libxed.a"
    AC_LANG_PUSH([C])
    AC_MSG_CHECKING([if libxed.a was compiled with -fPIC])
    AC_LINK_IFELSE([
#include <stdio.h>
#include <xed-interface.h>
int my_xed_init(void)
{
  xed_tables_init();
  return 0;
}
], [xed2_static_fpic=yes], [xed2_static_fpic=no])
    AC_MSG_RESULT([$xed2_static_fpic])
    AC_LANG_POP
    CFLAGS="$ORIG_CFLAGS"
    LIBS="$ORIG_LIBS"
  fi
  #
  # default lib flags: .so if exists, else .a
  #
  if test -f "${XED2}/lib/libxed.so" ; then
    XED2_LIB_FLAGS="-L${XED2}/lib -lxed"
  elif test -f "${XED2}/lib/libxed.a" ; then
    XED2_LIB_FLAGS="${XED2}/lib/libxed.a"
  else
    AC_MSG_ERROR([unable to find libxed.so or libxed.a])
  fi
  #
  # hpcrun: .so, or .a + fPIC, else fail
  #
  if test -f "${XED2}/lib/libxed.so" ; then
    XED2_HPCRUN_LIBS="-L${XED2}/lib -lxed"
  elif test "$xed2_static_fpic" = yes ; then
    XED2_HPCRUN_LIBS="${XED2}/lib/libxed.a"
  else
    AC_MSG_WARN([unable to build hpcrun: no libxed with -fPIC])
    no_xed_for_hpcrun=yes
  fi
  #
  # hpclink: .a only (with or without -fPIC)
  #
  if test -f "${XED2}/lib/libxed.a" ; then
    XED2_HPCLINK_LIBS="${XED2}/lib/libxed.a"
  else
    AC_MSG_WARN([unable to build hpclink: no static libxed.a])
    no_xed_for_hpclink=yes
  fi
  #
  # hpcprof-mpi: same as hpclink if fully static, else default
  #
  echo "x$HPC_LT_LDFLAGS" | grep -e all-static >/dev/null 2>&1
  if test $? = 0 ; then
    XED2_PROF_MPI_LIBS="$XED2_HPCLINK_LIBS"
  else
    XED2_PROF_MPI_LIBS="$XED2_LIB_FLAGS"
  fi
  #
  # add libirc.a, if exists for k1om
  #
  if test -f "${XED2}/lib/libirc.a" ; then
    XED2_LIB_FLAGS="${XED2_LIB_FLAGS} ${XED2}/lib/libirc.a"
    XED2_HPCRUN_LIBS="${XED2_HPCRUN_LIBS} ${XED2}/lib/libirc.a"
    XED2_HPCLINK_LIBS="${XED2_HPCLINK_LIBS} ${XED2}/lib/libirc.a"
    XED2_PROF_MPI_LIBS="${XED2_PROF_MPI_LIBS} ${XED2}/lib/libirc.a"
  fi
  AC_MSG_NOTICE([xed2 lib flags: $XED2_LIB_FLAGS])
fi

AC_SUBST([XED2_INC])
AC_SUBST([XED2_LIB_DIR])
AC_SUBST([XED2_LIB_FLAGS])
AC_SUBST([XED2_HPCRUN_LIBS])
AC_SUBST([XED2_HPCLINK_LIBS])
AC_SUBST([XED2_PROF_MPI_LIBS])
AC_SUBST([XED2_COPY])


#-------------------------------------------------
# Check for Xerces-C (XML parser)
#-------------------------------------------------

# NOTE: we should allow the locations of xerces, OA, and binutils
# installations to optionally be given on configure line or in the
# environment.

AC_MSG_CHECKING([for XercesC])

XERCES=no
if test "$use_externals" = yes ; then
  XERCES="$ext_xerces_prefix"
fi

AC_ARG_WITH([xerces],
  AS_HELP_STRING([--with-xerces=PATH],
                 [use given XercesC installation]),
  [if test $withval != no; then
     XERCES=${withval}
   fi],
  [])

XERCES_IFLAGS="-I${XERCES}/include"
XERCES_LDFLAGS="-L${XERCES}/lib"
XERCES_LDLIBS="-lxerces-c"

# tallent: should not need -- libtool should handle this
# xerces (on Linux) uses pthreads
#case "${host}" in
#  *-*-linux*)
#    XERCES_LDFLAGS="${XERCES_LDFLAGS} -lpthread"
#    ;;
#esac


if test "$XERCES" = no ; then
  XERCES_COPY=no
else
  XERCES_COPY=yes
  XERCES_LIB="${XERCES}/lib"
fi

AC_MSG_RESULT([${XERCES}])
AC_SUBST([XERCES])
AC_SUBST([XERCES_IFLAGS])
AC_SUBST([XERCES_LDFLAGS])
AC_SUBST([XERCES_LDLIBS])
AC_SUBST([XERCES_COPY])
AC_SUBST([XERCES_LIB])


#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------

#-------------------------------------------------
# enable-back-end
#-------------------------------------------------

# Build only those tools (hpcrun, fnbounds and prereqs) that run on
# the back end in a cross-compile config.

AC_ARG_ENABLE([back-end],
  [AS_HELP_STRING([--enable-back-end],
      [build only those tools needed to run hpcrun and hpclink
       on the back-end compute nodes])],
  [],
  [enable_back_end=no])

# disable the back-end label until the build method settles down.

back_end_label=
case "$enable_back_end" in
  yes ) back_end_label=back-end
        enable_back_end=yes
        ;;
  no )  ;;
  * )   back_end_label="$enable_back_end"
        enable_back_end=yes
        ;;
esac

if test "$enable_back_end" = yes ; then
  BACK_END_LABEL="-$back_end_label"
fi
back_end_label=
BACK_END_LABEL=

if test "$enable_back_end" = yes ; then
  OPT_INSTALL_MONITOR=no
  XERCES_COPY=no
fi

AC_SUBST([BACK_END_LABEL])

AM_CONDITIONAL(OPT_BUILD_BACK_END,  [test $enable_back_end = yes])
AM_CONDITIONAL(OPT_BUILD_FRONT_END, [test $enable_back_end = no])

AM_CONDITIONAL(OPT_BUILD_LIB_ALL,  [test $enable_back_end = no])
AM_CONDITIONAL(OPT_BUILD_TOOL_ALL, [test $enable_back_end = no])


#-------------------------------------------------
# enable-hpcrun-flat: Only build hpcrun if on Linux and PAPI is present
#-------------------------------------------------

AC_ARG_ENABLE([hpcrun-flat],
  [AS_HELP_STRING([--enable-hpcrun-flat],
	  [build hpcrun-flat (default yes)])],
  [],
  [enable_hpcrun_flat=yes])

AC_MSG_CHECKING([whether to build hpcrun-flat])

OPT_ENABLE_HPCRUN_FLAT="no"

if test "${enable_hpcrun_flat}" = yes \
   && test "${OPT_PAPI_DYNAMIC}" = yes \
   && test "${OPT_WITH_MONITOR}" = yes \
   && test "${HOST_OS}" = "Linux"
then
  OPT_ENABLE_HPCRUN_FLAT="yes"
fi

AC_MSG_RESULT([$OPT_ENABLE_HPCRUN_FLAT])
AM_CONDITIONAL(OPT_ENABLE_HPCRUN_FLAT, test "${OPT_ENABLE_HPCRUN_FLAT}" = "yes")


#-------------------------------------------------
# enable-hpcrun:
#-------------------------------------------------

AC_ARG_ENABLE([hpcrun],
  [AS_HELP_STRING([--enable-hpcrun],
	  [build hpcrun (default yes)])],
  [],
  [enable_hpcrun=yes])

AC_MSG_CHECKING([whether to build hpcrun])

OPT_ENABLE_HPCRUN=yes
enable_hpcrun_mesg=yes
if test "$enable_hpcrun" = no ; then
  OPT_ENABLE_HPCRUN=no
  enable_hpcrun_mesg='no (disabled)'
elif test "$LIBDWARF" = no   || test "$LIBELF" = no ||  \
     test "$LIBMONITOR" = no || test "$SYMTABAPI" = no || \
     test "$no_xed_for_hpcrun" = yes
then
  OPT_ENABLE_HPCRUN=no
  enable_hpcrun_mesg='no (missing prerequisites)'
fi

# tallent: $hpc_cpu_x86 is never set
#if test "$hpc_cpu_x86" = yes && test "$XED2" = no ; then
#  OPT_ENABLE_HPCRUN=no
#  enable_hpcrun_mesg='no (missing prerequisites)'
#fi

AC_MSG_RESULT([$enable_hpcrun_mesg])
AM_CONDITIONAL([OPT_ENABLE_HPCRUN], [test "${OPT_ENABLE_HPCRUN}" = yes])


# Find location of libstdc++.  This is used to set LD_LIBRARY_PATH in
# hpcfnbounds.
#
# FIXME (tallent): This is completely broken for cross-compiling
# (there is no reason for the paths to be persistent).  Moreover, we
# should not be responsible for configuring runtime paths for
# libstdc++.
#
# Try --print-file-name first.  If that doesn't work, then try ldd.
# If --print-file-name works, then its answer is more reliable because
# it doesn't depend on LD_LIBRARY_PATH.  But if you use CC= without
# setting LD_LIBRARY_PATH, then ldd will give the wrong answer.
#
# Of course, --print-file-name and libstdc++.so are GNU-specific.
# (Intel icc seems to support both, but computes an answer based on
# LD_LIBRARY_PATH, which can't be right.)  Is there a better option
# here?

libcxx_file=`${CXX} -print-file-name=libstdc++.so`

case "$libcxx_file" in
  /* ) ;;
  * )
    src="hpc_hello.$$.cc"
    obj="hpc_hello.$$.out"
    rm -f "$src" "$obj"
    cat <<"EOF" >"$src"
main(){}
EOF
    ${CXX} -o "$obj" "$src"
    libcxx_file=`ldd "$obj" | $AWK '/libstdc\+\+/ { print $3 }'`
    rm -f "$src" "$obj"
    ;;
esac

libcxx_dir=`dirname $libcxx_file`
libcxx_base=`basename $libcxx_file`
HPCRUN_LIBCXX_PATH=`( cd $libcxx_dir 2>/dev/null && /bin/pwd )`

case "$HPCRUN_LIBCXX_PATH" in
  /* ) AC_MSG_NOTICE([found libstdc++.so at ${HPCRUN_LIBCXX_PATH}/${libcxx_base}]) ;;
  * )  AC_MSG_WARN([unable to find libstdc++.so]) ;;
esac

AC_SUBST([HPCRUN_LIBCXX_PATH])


#-------------------------------------------------
# enable-hpcrun-static:
#-------------------------------------------------

# The motivation for this option is that it can be very tricky to
# build the static profiling code when cross-compiling for a multilib
# system.
#
# The problem is related to the fact that the final libtool target is
# a .o (rather than a .la) linked together from other .o's.  Libtool
# correctly creates the individual .o's.  But when the time comes to
# link the individual .o's together, problems occur.  The link begins
# promisingly using --mode=link and the cross compiler (e.g. scgcc32).
# However, libtool then uses the linker (ld) directly.  Unfortunately,
# the linker doesn't know about cross-compiling multilib magic and
# uses the default ABI rather than the correct one (e.g., 64 instead
# of n32).  Even worse, trying to solve the problem using by setting
# LDFLAGS (e.g. LDFLAGS="-Wl,-m...") appears futile because the flag
# is not passed to ld when it is called directly.

AC_MSG_CHECKING([whether to build hpcrun for statically linked executables])

OPT_ENABLE_HPCRUN_STATIC="yes"

AC_ARG_ENABLE([hpcrun-static],
  AS_HELP_STRING([--enable-hpcrun-static],
	  [build hpcrun for statically linked executables (default yes)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_HPCRUN_STATIC="yes" ;;
     no)  OPT_ENABLE_HPCRUN_STATIC="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-hpcrun-static]) ;;
   esac],
  [OPT_ENABLE_HPCRUN_STATIC=yes])

if test "$OPT_LIBMONITOR_STATIC" = no || test "$no_xed_for_hpclink" = yes ; then
  OPT_ENABLE_HPCRUN_STATIC=no
fi

AC_MSG_RESULT([${OPT_ENABLE_HPCRUN_STATIC}])
AM_CONDITIONAL(OPT_ENABLE_HPCRUN_STATIC, test "${OPT_ENABLE_HPCRUN_STATIC}" = "yes")

AC_SUBST(HPCLINK_LD_FLAGS)

AC_MSG_CHECKING([whether to build hpcrun for dynamically linked executables])

OPT_ENABLE_HPCRUN_DYNAMIC=no
if test "$OPT_ENABLE_HPCRUN" = yes && test "$OPT_LIBMONITOR_DYNAMIC" = yes ; then
  OPT_ENABLE_HPCRUN_DYNAMIC=yes
fi

AC_MSG_RESULT([$OPT_ENABLE_HPCRUN_DYNAMIC])

AM_CONDITIONAL([OPT_ENABLE_HPCRUN_DYNAMIC],
  [test "$OPT_ENABLE_HPCRUN_DYNAMIC" = yes])


#-------------------------------------------------
# enable-hpcserver:
#-------------------------------------------------

# hpcserver is new, so the default is no for now.

AC_ARG_ENABLE([hpcserver],
  [AS_HELP_STRING([--enable-hpcserver],
          [build hpcserver and hpcserver-mpi (default no)])],
  [],
  [enable_hpcserver=no])

case "$enable_hpcserver" in
  yes | Yes | YES ) enable_hpcserver=yes ;;
  * ) enable_hpcserver=no ;;
esac

AM_CONDITIONAL([OPT_ENABLE_HPCSERVER], [test "$enable_hpcserver" = yes])


#-------------------------------------------------
# hpcstruct: CFG support from OpenAnalysis and ParseAPI
#-------------------------------------------------

# Test symtabAPI/parseAPI for inline function and CFG loop tree
# support, and test for old or new method for getting an inline
# function name.

ORIG_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="${HOST_CXXFLAGS} ${CXXFLAGS} -I${BOOST_INC} -I${SYMTABAPI_INC}"
AC_LANG_PUSH([C++])

#
# Test for inline function support, getInlinedParent().
#
inline_avail=no

AC_MSG_CHECKING([symtabAPI for inline function support])

if test "$SYMTABAPI" != no
then
  AC_COMPILE_IFELSE([
#include <iostream>
#include <Symtab.h>
#include <Function.h>
using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
void get_parent(FunctionBase * func)
{
  FunctionBase *parent = func->getInlinedParent();
  cout << parent;
}
], [inline_avail=yes], [inline_avail=no])
fi

AC_MSG_RESULT([$inline_avail])

#
# Test if symtabAPI is using the new method for inline function names.
# Symtab 9.0 uses a new method (getName) for function names which is
# incompatible with the old method (getAllPrettyNames).
#
new_symtab_getname=no

AC_MSG_CHECKING([symtabAPI for new function name iterator])

if test "$inline_avail" = yes
then
  AC_COMPILE_IFELSE([
#include <iostream>
#include <Symtab.h>
#include <Function.h>
using namespace Dyninst;
using namespace SymtabAPI;
void get_name(FunctionBase * func)
{
  std::cout << func->getName();
}
], [new_symtab_getname=yes], [new_symtab_getname=no])
fi

AC_MSG_RESULT([$new_symtab_getname])

#
# Test if symtabAPI includes parseAPI CFG loop support.
#
parseapi_avail=no

AC_MSG_CHECKING([parseAPI for CFG loop support])

if test "$inline_avail" = yes && test "$PARSEAPI_AVAIL" = yes
then
  AC_COMPILE_IFELSE([
#include <iostream>
#include <vector>
#include <Symtab.h>
#include <Function.h>
#include <CFG.h>
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace std;
void get_blocks(LoopTreeNode * node)
{
  Loop * loop = node->loop;
  vector <Block *> bvec;
  loop->getLoopBasicBlocks(bvec);
  std::cout << bvec.size();
}
], [parseapi_avail=yes], [parseapi_avail=no])
fi

AC_MSG_RESULT([$parseapi_avail])

AC_LANG_POP
CXXFLAGS="$ORIG_CXXFLAGS"

#
# Use OpenAnalysis or ParseAPI or both as available for building CFG's
# in hpcstruct.  Warn if neither is available.
#
# OpenAnalysis can be disabled with '--without-open-analysis' and
# ParseAPI can be disabled with '--disable-parseapi'.
#

AC_ARG_ENABLE([parseapi],
  [AS_HELP_STRING([--disable-parseapi],
      [disable using parseapi for building control flow graphs in
       hpcstruct (default is to use if available)])],
  [],
  [enable_parseapi=use])

if test "$enable_parseapi" = no ; then
  parseapi_avail=no
fi

cfg_mesg=
if test "$OA_AVAIL" = yes ; then
  cfg_mesg='open-analysis'
fi
if test "$parseapi_avail" = yes ; then
  cfg_mesg="$cfg_mesg parseapi"
fi
cfg_mesg=`echo $cfg_mesg`

if test "$OA_AVAIL" = no && test "$parseapi_avail" = no ; then
  AC_MSG_WARN([unable to build hpcstruct: no open-analysis or parseapi])
  cfg_mesg=no
fi

if test "$new_symtab_getname" = yes ; then
  AC_DEFINE([SYMTAB_NEW_NAME_ITERATOR], [1], [Use new symtabAPI getName iterator])
fi

AM_CONDITIONAL([OPT_ENABLE_BANAL_WITH_SYMTAB], [test "$inline_avail" = yes])
AM_CONDITIONAL([OPT_ENABLE_BANAL_WITH_PARSEAPI], [test "$parseapi_avail" = yes])
AM_CONDITIONAL([OPT_ENABLE_BANAL_WITH_OA], [test "$OA_AVAIL" = yes])

# Hpcstruct gets a launch script if (1) Xed is shared (some versions
# of MIC), or (2) use symtab for inline support.

hpcstruct_launch_script=yes

AM_CONDITIONAL(OPT_HPCSTRUCT_LAUNCH_SCRIPT, [test "$hpcstruct_launch_script" = yes])


#-------------------------------------------------
# enable-mpi:
#-------------------------------------------------

AC_MSG_CHECKING([whether to build MPI-based analysis tools (e.g., hpcprof-mpi)])

OPT_REQ_ENABLE_MPI="auto" # yes, no, auto

AC_ARG_ENABLE([mpi],
  AS_HELP_STRING([--enable-mpi],
                 [Enable MPI support (uses MPICXX variable)]),
  [case "${enableval}" in
     yes) OPT_REQ_ENABLE_MPI="yes" ;;
     no)  OPT_REQ_ENABLE_MPI="no" ;;
     *)   AC_MSG_ERROR([bad value ${enableval} for --enable-mpi]) ;;
   esac],
  [if test -n "${MPICXX_ORIG}" ; then
     OPT_REQ_ENABLE_MPI=yes
   fi])

OPT_ENABLE_MPI=no
if test -n "${MPICXX}" -a "${OPT_REQ_ENABLE_MPI}" != "no" ; then
  OPT_ENABLE_MPI=yes
fi

AC_MSG_RESULT([${OPT_ENABLE_MPI} (request: ${OPT_REQ_ENABLE_MPI})])


if test "${OPT_REQ_ENABLE_MPI}" = "yes" -a "${OPT_ENABLE_MPI}" = no ; then
  AC_MSG_ERROR([MPI support requested but MPI compiler is invalid!])
fi

AM_CONDITIONAL(OPT_ENABLE_MPI, test "${OPT_ENABLE_MPI}" = "yes")


#-------------------------------------------------
# enable-mpi-wrapper: check MPI compiler
#-------------------------------------------------

AC_MSG_CHECKING([whether to build MPI monitoring support])

OPT_REQ_ENABLE_MPI_WRAP="no" # yes, no, auto

AC_ARG_ENABLE([mpi-wrapper],
  AS_HELP_STRING([--enable-mpi-wrapper],
                 [Enable MPI wrapper support (uses MPIF77 and CFLAGS)]),
  [case "${enableval}" in
     yes) OPT_REQ_ENABLE_MPI_WRAP="yes" ;;
     no)  OPT_REQ_ENABLE_MPI_WRAP="no" ;;
     *)   AC_MSG_ERROR([bad value ${enableval} for --enable-mpi-wrapper]) ;;
   esac])

OPT_ENABLE_MPI_WRAP=no
if test "${OPT_REQ_ENABLE_MPI_WRAP}" != "no" ; then
  AC_MSG_CHECKING([whether can compile MPI code with CFLAGS='$CFLAGS'])

  AC_LANG_PUSH([C])
  OLD_CC="$CC"
  CC="$MPICC"
  CFLAGS="$CFLAGS"
  AC_COMPILE_IFELSE([#include <mpi.h>],
        [ans=yes],
        [ans=no])

  CC="$OLD_CC"
  OPT_ENABLE_MPI_WRAP=$ans
  AC_LANG_POP
fi

AC_MSG_RESULT([${OPT_ENABLE_MPI_WRAP} (request: ${OPT_REQ_ENABLE_MPI_WRAP})])


if test "${OPT_REQ_ENABLE_MPI_WRAP}" = "yes" -a "${OPT_ENABLE_MPI_WRAP}" = no ; then
  AC_MSG_ERROR([MPI wrapper support requested but MPI compiler is invalid!])
fi

AM_CONDITIONAL(OPT_ENABLE_MPI_WRAP, test "${OPT_ENABLE_MPI_WRAP}" = "yes")

#-------------------------------------------------
# enable-mpi-wrapper: check F77 symbols
#-------------------------------------------------

if test "${OPT_ENABLE_MPI_WRAP}" = "yes" ; then

  if test -z "${MPIF77}" ; then
    AC_CHECK_PROGS(MPIF77, mpif77 mpxlf_r mpixlf77 mpxlf cmpifc xlf f77 g77 pgf77 fort ifc, mpif77)
  fi

  if test -n "$MPIF77" -a "$MPIF77" != no ; then

    AC_MSG_CHECKING(Whether fortran symbols have underscores)
    /bin/rm -f ffunc.f flink.c
    echo "      subroutine f_fun()" > ffunc.f
    echo "      return" >> ffunc.f
    echo "      end" >> ffunc.f
    $MPIF77 $FFLAGS -c ffunc.f 1>/dev/null 2>/dev/null
    echo "main(){ FF(); return 0; }" > flink.c
    if $CC -o flink -DFF=f_fun flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(same as C)
      F77_SYMBOLS=symbol
    elif $CC -o flink -DFF=f_fun_ flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(lowercase with underscore)
      F77_SYMBOLS=symbol_
    elif $CC -o flink -DFF=f_fun__ flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(lowercase with 2 underscores)
      F77_SYMBOLS=symbol__
    elif $CC -o flink -DFF=F_FUN flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(uppercase)
      F77_SYMBOLS=SYMBOL
    elif $CC -o flink -DFF=F_FUN_ flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(uppercase with underscore)
      F77_SYMBOLS=SYMBOL_
    elif $CC -o flink -DFF=F_FUN__ flink.c ffunc.o $LDFLAGS $LIBS 1>/dev/null 2>/dev/null; then
      AC_MSG_RESULT(uppercase with 2 underscores)
      F77_SYMBOLS=SYMBOL__
    else
      AC_MSG_RESULT(giving up)
      AC_MSG_ERROR(could not determine F77 symbol names)
    fi
    /bin/rm -f ffunc.f ffunc.o flink flink.c flink.o ffunc
  else
    F77_SYMBOLS=symbol_
    AC_MSG_RESULT(Fortran compiler not found. Guessing lowercase with underscore)

  fi
fi

AC_SUBST(F77_SYMBOLS,$F77_SYMBOLS)


#-------------------------------------------------
# enable-lush, enable-lush-pthreads, with-cilk
#-------------------------------------------------

opt_enable_lush=no

AC_ARG_ENABLE([lush],
  [AS_HELP_STRING([--enable-lush],
                  [use lush backtrace in hpcrun])],
  [case "${enableval}" in
     yes) opt_enable_lush="yes" ;;
     no)  opt_enable_lush="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-lush]) ;;
   esac],
  [opt_enable_lush=no])

AC_MSG_CHECKING([whether to use lush])

if test "$opt_enable_lush" != yes ; then
  opt_enable_lush=no
fi

AC_MSG_RESULT([$opt_enable_lush])

AM_CONDITIONAL([OPT_ENABLE_LUSH], [test "$opt_enable_lush" = yes])



OPT_WITH_CILK=no
OPT_CILK_ROOT=

AC_ARG_WITH([cilk],
  AS_HELP_STRING([--with-cilk=PATH],
                 [use given (MIT) Cilk installation for LUSH agents]),
  [if test $withval != no; then
     OPT_WITH_CILK=yes
     OPT_CILK_ROOT=${withval}
   else
     OPT_WITH_CILK=no
   fi],
  [])


OPT_CILK_IFLAGS="-I${OPT_CILK_ROOT}/include"

AM_CONDITIONAL([OPT_WITH_CILK], [test "$OPT_WITH_CILK" = yes])
AC_SUBST([OPT_CILK_IFLAGS])


# Note: the --enable-lush-pthreads option may be temporary and may be
# folded into other options.

AC_ARG_ENABLE([lush-pthreads],
  [AS_HELP_STRING([--enable-lush-pthreads],
                  [enable lush pthreads (temp)])],
  [],
  [enable_lush_pthreads=no])

if test "$enable_lush_pthreads" = yes && test "$opt_enable_lush" = no ; then
  AC_MSG_ERROR([enable-lush-pthreads requires enable-lush])
fi

wrap1='pthread_mutex_lock pthread_mutex_trylock pthread_mutex_unlock'
wrap2='pthread_spin_lock pthread_spin_trylock pthread_spin_unlock pthread_spin_destroy'
wrap3='pthread_cond_init pthread_cond_destroy pthread_cond_wait'
wrap4='pthread_cond_timedwait pthread_cond_signal pthread_cond_broadcast'

hpclink_extra_wrap_names=
if test "$enable_lush_pthreads" = yes ; then
  hpclink_extra_wrap_names="$wrap1 $wrap2 $wrap3 $wrap4"
fi

AC_MSG_NOTICE([lush pthreads: $enable_lush_pthreads])
AM_CONDITIONAL([OPT_ENABLE_LUSH_PTHREADS], [test "$enable_lush_pthreads" = yes])

AC_SUBST([hpclink_extra_wrap_names])


#-------------------------------------------------
# enable-xop: support for AMD XOP instructions
#-------------------------------------------------

AC_ARG_ENABLE([xop],
  AS_HELP_STRING([--enable-xop],
      [include support for decoding AMD XOP instructions, x86-64 only
      (default no)]),
  [],
  [enable_xop=no])

AC_MSG_CHECKING([whether to support AMD XOP instructions])
AC_MSG_RESULT([$enable_xop])

if test "x$enable_xop" = xyes ; then
  AC_DEFINE([ENABLE_XOP], [1], [Support for AMD XOP instructions])
fi


#-------------------------------------------------
# check for POSIX real-time timers
#-------------------------------------------------

# POSIX timer_create() and CLOCK_REALTIME go back to at least 2001, so
# they're pretty safe.  But we also need the SIGEV_THREAD_ID notify
# method, the SYS_gettid syscall and the _sigev_un._tid field in
# struct sigevent.  These are Linux extensions and may not compile on
# all systems.

AC_LANG_PUSH([C])
ORIG_LIBS="$LIBS"
LIBS="-lrt"

AC_MSG_CHECKING([for CLOCK_REALTIME and SIGEV_THREAD_ID])

AC_LINK_IFELSE([
#include <sys/syscall.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id _sigev_un._tid
#endif
int main()
{
  struct sigevent sigev;
  timer_t timerid;
  sigev.sigev_notify = SIGEV_THREAD_ID;
  sigev.sigev_signo = SIGRTMIN + 3;
  sigev.sigev_value.sival_ptr = &timerid;
  sigev.sigev_notify_thread_id = syscall(SYS_gettid);
  return timer_create(CLOCK_REALTIME, &sigev, &timerid);
}
], [ans=yes], [ans=no])

AC_MSG_RESULT([$ans])

# If CLOCK_REALTIME exists, then also try CLOCK_THREAD_CPUTIME_ID.

if test "$ans" = yes ; then
  AC_DEFINE([ENABLE_CLOCK_REALTIME], [1],
            [Support for CLOCK_REALTIME and SIGEV_THREAD_ID])

  AC_MSG_CHECKING([for CLOCK_THREAD_CPUTIME_ID])
  AC_LINK_IFELSE([
#include <sys/time.h>
#include <time.h>
int main()
{
  clockid_t clock = CLOCK_THREAD_CPUTIME_ID;
  return 0;
}
], [ans=yes], [ans=no])
  AC_MSG_RESULT([$ans])

  if test "$ans" = yes ; then
    AC_DEFINE([ENABLE_CLOCK_CPUTIME], [1], [Support for CLOCK_THREAD_CPUTIME_ID])
  fi
fi

LIBS="$ORIG_LIBS"
AC_LANG_POP


#-------------------------------------------------
# enable-bluegene: BG/* support
#-------------------------------------------------

AC_MSG_CHECKING([whether to enable IBM BlueGene/P support])

OPT_ENABLE_IBM_BLUEGENE="no"

AC_ARG_ENABLE([bluegene],
  AS_HELP_STRING([--enable-bluegene],
                 [Enable BlueGene/P support]),
  [case "${enableval}" in
     yes) OPT_ENABLE_IBM_BLUEGENE="yes" ;;
     no)  OPT_ENABLE_IBM_BLUEGENE="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-bluegene]) ;;
   esac],
  [if test "$HOST_CPU_PPC" = yes && test -d /bgsys ; then
     OPT_ENABLE_IBM_BLUEGENE=yes
   fi])

AC_MSG_RESULT([${OPT_ENABLE_IBM_BLUEGENE}])

if test "${OPT_ENABLE_IBM_BLUEGENE}" = "yes" ; then
  AC_DEFINE([HOST_SYSTEM_IBM_BLUEGENE], [1], [IBM BlueGene/P support])
fi

#-------------------------------------------------
# Option: --enable-bgq-cnk=PATH
#-------------------------------------------------
#
# hpcrun (&hpclink) on BGQ needs the back-end compiler for a few files.
#
enable_bgq=no
AC_ARG_ENABLE([bgq-cnk],
    [AS_HELP_STRING([--enable-bgq-cnk],
	[enable BGQ- and cnk-specific options: build parts of hpcrun with
	 back-end powerpc64-bgq-linux-gcc])],
    [enable_bgq="yes"],
    [])

# Search for the back-end BGQ compiler on PATH and test for Blue
# Gene/P (suppress the warning on P).

ppc64_bgq_avail=
if test -d /bgsys ; then
    AC_CHECK_PROG([ppc64_bgq_avail], [powerpc64-bgq-linux-gcc], [yes], [no])
else
    ppc64_bgq_avail=no
fi

hpcrun_use_bgq_backend=no
warn_bgq_backend=no

case "$enable_bgq" in 
    yes )
	if test "$ppc64_bgq_avail" != yes ; then
	    AC_MSG_ERROR([unable to find powerpc64-bgq-linux-gcc for enable-bgq-cnk])
	fi
	hpcrun_use_bgq_backend=yes
	;;
    no ) ;;
    * )  ;;
esac

AC_SUBST([hpcrun_use_bgq_backend])
AC_MSG_RESULT([$hpcrun_use_bgq_backend])
AM_CONDITIONAL(OPT_BGQ_BACKEND, test "$hpcrun_use_bgq_backend" = "yes")

#-------------------------------------------------
# Option: --with-upc=PATH
#-------------------------------------------------

# The IBM BlueGene UPC sampling source, for which we need header and
# library files.  On intrepid, these files are in:
#   inc: /bgsys/drivers/ppcfloor/arch/include/spi
#   lib: /bgsys/drivers/ppcfloor/runtime/SPI
#
# The --with-upc path is just the least common ancestor of these two
# directories. but we may need to generalize this to --with-upc-inc
# and --with-upc-lib.

AC_MSG_CHECKING([for UPC])

AC_ARG_WITH([upc],
  AS_HELP_STRING([--with-upc=PATH],
      [path to IBM BlueGene UPC installation]),
  [error_if_missing=yes],
  [if test "$OPT_ENABLE_IBM_BLUEGENE" = yes ; then
     with_upc=/bgsys/drivers/ppcfloor
   else
     with_upc=no
   fi
   error_if_missing=no])

AC_ARG_WITH([upc-lib],
  AS_HELP_STRING([--with-upc-lib=LIBRARY],
      [path to UPC library(s), absolute or relative to WITH_UPC/runtime/SPI]),
  [],
  [with_upc_lib=libSPI.cna.a])

OPT_UPC=no
OPT_UPC_IFLAGS=
OPT_UPC_LDFLAGS=
opt_upc_mesg=no

case "${with_upc}" in
  /* )
    if test -f "${with_upc}/arch/include/spi/UPC.h"
    then
      OPT_UPC=yes
      OPT_UPC_IFLAGS="-I ${with_upc}/arch/include"
      opt_upc_mesg="$with_upc"
      for lib in $with_upc_lib ; do
	case "$lib" in
	  /* ) abs_lib="$lib" ;;
	  * )  abs_lib="${with_upc}/runtime/SPI/${lib}" ;;
	esac
	if test -f "$abs_lib" ; then
	  OPT_UPC_LDFLAGS="${OPT_UPC_LDFLAGS} ${abs_lib}"
	else
	  OPT_UPC=no
	  opt_upc_mesg=no
	  if test "$error_if_missing" = yes ; then
	    AC_MSG_ERROR([invalid UPC directory: $with_upc])
	  fi
	fi
      done
    else
      if test "$error_if_missing" = yes ; then
	AC_MSG_ERROR([invalid UPC directory: $with_upc])
      fi
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([UPC directory must be absolute path: $with_upc])
    ;;
esac

AC_MSG_RESULT([$opt_upc_mesg])

AM_CONDITIONAL([OPT_ENABLE_UPC], [test "$OPT_UPC" = yes])
AC_SUBST(OPT_UPC_IFLAGS)
AC_SUBST(OPT_UPC_LDFLAGS)


#-------------------------------------------------
# Option: --with-cuda=PATH
#-------------------------------------------------

# CUDA support for CPU-GPU-Blame-shifting
# CUDA default install path is /usr/local/cuda
# Include files will be in CUDADIR/include
# CUDA 64 bit libraries under CUDADIR/lib64 and 32 bit in CUDADIR/lib
# We need CUPTI for callbacks which is in CUDADIR/extras/CUPTI
# CUPTI 64 bit libraries under CUPTIDIR/lib64 and 32 bit in CUPTIDIR/lib


if test "${host_sizeof_voidp}" = "4"; then
    CUDA_LIB_SEARCH_ORDER="lib32 lib"
else
    CUDA_LIB_SEARCH_ORDER="lib64 lib"
fi


OPT_HAVE_CUDA=no
OPT_CUDA=
OPT_CUDA_INCPATH=
OPT_CUDA_LDFLAGS=
OPT_CUDA_LIBPATH=
OPT_CUPTI_INCPATH=
OPT_CUPTI_LIBPATH=
OPT_CUPTI_LDFLAGS=

AC_ARG_WITH([cuda],
  AS_HELP_STRING([--with-cuda=PATH],
                 [use given CUDA installation (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     OPT_HAVE_CUDA=yes
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-cuda requires absolute path as argument; given '${withval}'])
     fi
     OPT_CUDA=${withval}
   fi])

if test "$OPT_HAVE_CUDA" = "yes" ; then
 AC_ARG_WITH([cuda-include],
  AS_HELP_STRING([--with-cuda-include=PATH],
                 [use given CUDA include (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-cuda-include requires absolute path as argument; given '${withval}'])
     fi
     OPT_CUDA_INCPATH=${withval}
  fi],

     [if test -f ${OPT_CUDA}/include/cuda.h; then
         OPT_CUDA_INCPATH=${OPT_CUDA}/include
     else
         AC_MSG_ERROR(['${OPT_CUDA}/include/cuda.h' not found])
     fi])  

  OPT_CUDA_IFLAGS="-I${OPT_CUDA_INCPATH} "

  AC_ARG_WITH([cuda-lib],
  AS_HELP_STRING([--with-cuda-lib=PATH],
                 [use given CUDA lib (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-cuda-lib requires absolute path as argument; given '${withval}'])
     fi
     OPT_CUDA_LIBPATH=${withval}
   fi], 
  
   [for lib in ${CUDA_LIB_SEARCH_ORDER} ; do
    if test -f "${OPT_CUDA}/${lib}/libcudart.so" 
    then
      OPT_CUDA_LIBPATH="${OPT_CUDA}/${lib}"
      break
    fi
  done 
     if test  -z "${OPT_CUDA_LIBPATH}" ; then
       AC_MSG_ERROR([libcudart.so not found])
     fi
   ] )

  OPT_CUDA_LDFLAGS="-L${OPT_CUDA_LIBPATH} -lcudart "

fi

AM_CONDITIONAL([OPT_ENABLE_CUDA], [test "$OPT_HAVE_CUDA" = yes])

AC_SUBST([OPT_CUDA])
AC_SUBST([OPT_CUDA_IFLAGS])
AC_SUBST([OPT_CUDA_LDFLAGS])

#-------------------------------------------------
# Option: --with-cupti=PATH
#-------------------------------------------------
OPT_HAVE_CUPTI=no
OPT_CUPTI=
OPT_CUPTI_IFLAGS=
OPT_CUPTI_LDFLAGS=
OPT_CUPTI_LIBPATH=

AC_ARG_WITH([cupti],
  AS_HELP_STRING([--with-cupti=PATH],
                 [use given CUDA/CUPTI installation (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     OPT_HAVE_CUPTI=yes
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-cupti requires absolute path as argument; given '${withval}'])
     fi
     OPT_CUPTI=${withval}
   fi])

#
# Valid cupti library
#
if test "$OPT_HAVE_CUPTI" = yes; then
   if test ! -f $OPT_CUPTI/include/cupti.h; then
      AC_MSG_ERROR([--with-cupti=$OPT_CUPTI invalid! No cupti.h])
   fi
fi

if test "$use_papi_c_cupti" != no; then
   if test "$OPT_HAVE_CUPTI" = yes; then
       OPT_CUPTI_IFLAGS="-I$OPT_CUPTI/include"
       if test "$OPT_HAVE_CUDA" = no; then
          cuda_dir=$(dirnmame $(dirname $OPT_CUPTI))/include
	  OPT_CUDA_IFLAGS="-I$cuda_dir"
       fi
       OPT_CUPTI_IFLAGS="$OPT_CUDA_IFLAGS $OPT_CUPTI_IFLAGS"
   else
      AC_MSG_ERROR([papi-c-cupti specified, but no cupti lib specified!])
   fi
fi

AM_CONDITIONAL([OPT_ENABLE_CUPTI], [test "$OPT_HAVE_CUPTI" = yes])

AC_SUBST([OPT_CUPTI_IFLAGS])
AC_SUBST([OPT_CUPTI])

#-------------------------------------------------
# enable-data-centric-tracing: data-centric tracing
#-------------------------------------------------

AC_MSG_CHECKING([whether to enable data-centric tracing])

OPT_ENABLE_DATACENTRIC_TRACE="no"

AC_ARG_ENABLE([data-centric-tracing],
  AS_HELP_STRING([--enable-data-centric-tracing],
                 [Enable data-centric tracing (prototype)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_DATACENTRIC_TRACE="yes" ;;
     no)  OPT_ENABLE_DATACENTRIC_TRACE="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-data-centric-tracing]) ;;
   esac],
  [OPT_ENABLE_DATACENTRIC_TRACE=no])

AC_MSG_RESULT([${OPT_ENABLE_DATACENTRIC_TRACE}])

if test "${OPT_ENABLE_DATACENTRIC_TRACE}" = "yes" ; then
  AC_DEFINE([DATACENTRIC_TRACE], [1], [Data-centric tracing])
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
  AC_DEFINE([HAVE_HPC_GNUBINUTILS], [1], [HPCToolkit binutils])
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
# Should we expect statically linked binaries
#----------------------------------------------------------------------------


#HPC_DEF_IS_COMPILER_MAKING_STATIC_BINARIES()

#CXX_MAKES_STATIC_BINARIES="no"
#if HPC_isCompilerMakingStaticBinaries(${CXX} ${CXXFLAGS} ${HOST_CXXFLAGS}) ; then
#  CXX_MAKES_STATIC_BINARIES="yes"
#fi

#AM_CONDITIONAL(CXX_MAKES_STATIC_BINARIES,
#               test "${CXX_MAKES_STATIC_BINARIES}" = "yes")


#MPICXX_MAKES_STATIC_BINARIES="no"
#if test "${OPT_ENABLE_MPI}" = "yes" && HPC_isCompilerMakingStaticBinaries(${MPICXX} ${CXXFLAGS} ${HOST_CXXFLAGS}) ; then
#  MPICXX_MAKES_STATIC_BINARIES="yes"
#fi

#AM_CONDITIONAL(MPICXX_MAKES_STATIC_BINARIES,
#               test "${MPICXX_MAKES_STATIC_BINARIES}" = "yes")


#----------------------------------------------------------------------------
# Adjust some install paths for Makefiles
#----------------------------------------------------------------------------

my_pkglibdir='$(libdir)/$(PACKAGE)'
my_pkglibexecdir='$(libexecdir)/$(PACKAGE)'
BACK_END_LABEL=

AC_SUBST([my_pkglibdir])
AC_SUBST([my_pkglibexecdir])
AC_SUBST([BACK_END_LABEL])


#----------------------------------------------------------------------------
# Makefiles to create
#----------------------------------------------------------------------------

AC_CONFIG_HEADERS([src/include/hpctoolkit-config.h])

AC_CONFIG_FILES([ \
  Makefile \
  \
  doc/Makefile \
  doc/man/Makefile \
  doc/man/HPCToolkitVersionInfo.tex \
  doc/manual/Makefile \
  doc/www/Makefile \
  \
  lib/Makefile \
  \
  src/Makefile \
  src/tool/Makefile \
  src/tool/hpcfnbounds/Makefile \
  src/tool/hpclump/Makefile \
  src/tool/hpcprof/Makefile \
  src/tool/hpcprof-mpi/Makefile \
  src/tool/hpcprof-flat/Makefile \
  src/tool/hpcproftt/Makefile \
  src/tool/hpcrun/Makefile \
  src/tool/hpcrun/utilities/bgq-cnk/Makefile \
  src/tool/hpcrun-flat/Makefile \
  src/tool/hpcserver/Makefile \
  src/tool/hpcserver/mpi/Makefile \
  src/tool/hpcstruct/Makefile \
  src/tool/misc/Makefile \
  src/tool/xprof/Makefile \
  src/lib/Makefile \
  src/lib/analysis/Makefile \
  src/lib/banal/Makefile \
  src/lib/binutils/Makefile \
  src/lib/isa/Makefile \
  src/lib/prof/Makefile \
  src/lib/profxml/Makefile \
  src/lib/prof-lean/Makefile \
  src/lib/stubs-gcc_s/Makefile \
  src/lib/support/Makefile \
  src/lib/support-lean/Makefile \
  src/lib/xml/Makefile \
])


AC_CONFIG_FILES([src/tool/hpcrun/scripts/hpcrun],
  [chmod +x src/tool/hpcrun/scripts/hpcrun])
AC_CONFIG_FILES([src/tool/hpcrun/scripts/hpclink],
  [chmod +x src/tool/hpcrun/scripts/hpclink])
AC_CONFIG_FILES([src/tool/hpcrun/scripts/hpcsummary],
  [chmod +x src/tool/hpcrun/scripts/hpcsummary])
AC_CONFIG_FILES([src/tool/hpcfnbounds/hpcfnbounds],
  [chmod +x src/tool/hpcfnbounds/hpcfnbounds])
AC_CONFIG_FILES([src/tool/hpcprof/hpcprof],
  [chmod +x src/tool/hpcprof/hpcprof])
AC_CONFIG_FILES([src/tool/hpcprof-mpi/hpcprof-mpi],
  [chmod +x src/tool/hpcprof-mpi/hpcprof-mpi])
AC_CONFIG_FILES([src/tool/hpcprof-flat/hpcprof-flat],
  [chmod +x src/tool/hpcprof-flat/hpcprof-flat])
AC_CONFIG_FILES([src/tool/hpcproftt/hpcproftt],
  [chmod +x src/tool/hpcproftt/hpcproftt])
AC_CONFIG_FILES([src/tool/hpcstruct/hpcstruct],
  [chmod +x src/tool/hpcstruct/hpcstruct])

AC_SUBST_FILE([copyright_hash])
copyright_hash="${srcdir}/config/copyright-hash"

AC_SUBST_FILE([export_hpctoolkit])
export_hpctoolkit="${srcdir}/config/export-hpctoolkit"

AC_CONFIG_COMMANDS()

AC_OUTPUT()

dnl # Hack to rename the scripts for a back-end build.
dnl if test "$enable_back_end" = yes ; then
dnl   dir=src/tool/hpcrun/scripts
dnl   for file in hpcrun hpclink
dnl   do
dnl     cp -f "${dir}/${file}" "${dir}/${file}${BACK_END_LABEL}"
dnl   done
dnl fi


#-------------------------------------------------
# Summary of the config options.
#-------------------------------------------------

AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([HPCToolkit summary])
AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([  HPCToolkit version: ${PACKAGE_VERSION}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  prefix: $prefix])
AC_MSG_NOTICE([  build:  $build])
AC_MSG_NOTICE([  host:   $host])
AC_MSG_NOTICE([  host endian: $host_endian])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  hpcrun?:      ${enable_hpcrun_mesg}])
AC_MSG_NOTICE([  hpclink?:     ${OPT_ENABLE_HPCRUN_STATIC}])
AC_MSG_NOTICE([  hpcrun-flat?: ${OPT_ENABLE_HPCRUN_FLAT}])
AC_MSG_NOTICE([  mpi support?: ${OPT_ENABLE_MPI}])
AC_MSG_NOTICE([  mpi wrapper support?: ${OPT_ENABLE_MPI_WRAP}])
AC_MSG_NOTICE([  hpcrun/lush?: ${opt_enable_lush}])
AC_MSG_NOTICE([  datacentric?: ${OPT_ENABLE_DATACENTRIC_TRACE}])
AC_MSG_NOTICE([  hpcserver?:   ${enable_hpcserver}])
AC_MSG_NOTICE([  hpcstruct:    $cfg_mesg])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  C compiler: $CC_VERSION])
AC_MSG_NOTICE([  Path:      $CC_PATH])
AC_MSG_NOTICE([  CC:        '${CC}'])
AC_MSG_NOTICE([  CFLAGS:    '${CFLAGS} ${HOST_CFLAGS}'])
AC_MSG_NOTICE([  C++ compiler: $CXX_VERSION])
AC_MSG_NOTICE([  Path:      $CXX_PATH])
AC_MSG_NOTICE([  CXX:       '${CXX}'])
AC_MSG_NOTICE([  CXXFLAGS:  '${CXXFLAGS} ${HOST_CXXFLAGS}'])
AC_MSG_NOTICE([  C++11:     '${cxx_c11_flag}'])
AC_MSG_NOTICE([  CCAS:      '${CCAS}'])
AC_MSG_NOTICE([  CCASFLAGS: '${CCASFLAGS}'])
AC_MSG_NOTICE([  LDFLAGS:   '${LDFLAGS}'])
AC_MSG_NOTICE([  MPICC:     '${MPICC}'])
AC_MSG_NOTICE([  MPICXX:    '${MPICXX}'])
AC_MSG_NOTICE([  MPIF77:    '${MPIF77}'])
AC_MSG_NOTICE([  HPC_LT_LDFLAGS:        '${HPC_LT_LDFLAGS}'])
AC_MSG_NOTICE([  HPCPROFMPI_LT_LDFLAGS: '${HPCPROFMPI_LT_LDFLAGS}'])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  binutils:     ${GNUBINUTILS}])
AC_MSG_NOTICE([  boost:        ${BOOST}])
AC_MSG_NOTICE([  libdwarf:     ${LIBDWARF}])
AC_MSG_NOTICE([  libelf:       ${LIBELF}])
AC_MSG_NOTICE([  libmonitor:   ${LIBMONITOR}])
AC_MSG_NOTICE([  monitor/old:  ${MONITOR}])
AC_MSG_NOTICE([  libunwind:    ${OPT_LIBUNWIND}])
AC_MSG_NOTICE([  OpenAnalysis: ${OA}])
AC_MSG_NOTICE([  symtabAPI:    ${SYMTABAPI}])
AC_MSG_NOTICE([  xed2:         ${XED2}])
AC_MSG_NOTICE([  xerces:       ${XERCES}])
AC_MSG_NOTICE([  papi:         ${OPT_PAPI}])
AC_MSG_NOTICE([  cuda:         ${OPT_CUDA}])
AC_MSG_NOTICE([  papi-c-cupti: ${use_papi_c_cupti}])
AC_MSG_NOTICE([  upc (BG/P):   ${opt_upc_mesg}])

if test "$warn_config_site" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([You have CONFIG_SITE set in your environment.])
  AC_MSG_WARN([This may break some of hpctoolkit's install paths and])
  AC_MSG_WARN([launch scripts.  Set CONFIG_SITE=NONE or else unset it])
  AC_MSG_WARN([from your environment and rerun configure.])
fi

if test "$warn_bgq_backend" = yes ; then
    AC_MSG_NOTICE([])
    AC_MSG_WARN([On the Blue Gene/Q compute nodes, hpcrun needs a separate,])
    AC_MSG_WARN([back-end compiler.  Use --enable-bgq to build for the back end,])
    AC_MSG_WARN([or else --disable-bgq to build for the front end.])
fi

if test "$warn_no_externals" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Not using hpctoolkit-externals.])
  AC_MSG_WARN([Unless you're building hpctoolkit only for docs or make dist,])
  AC_MSG_WARN([this is probably an error.  First, checkout, configure and])
  AC_MSG_WARN([build externals and then reconfigure hpctoolkit with:])
  AC_MSG_WARN([--with-externals=/path/to/externals/install/directory])
fi

if test "$warn_non_gcc" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Not using the GNU C or C++ compiler.])
  AC_MSG_WARN([This is not necessarily an error, but some parts of hpctoolkit])
  AC_MSG_WARN([may not compile cleanly without gcc or g++.  If the build fails,])
  AC_MSG_WARN([try using GNU gcc and g++ before reporting an error.])
fi

if test "$warn_compiler_mismatch" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Mismatched C or C++ compiler.])
  AC_MSG_WARN([The C or C++ compiler does not match the one used to build])
  AC_MSG_WARN([hpctoolkit-externals.  Check the externals configure summary])
  AC_MSG_WARN([and try again with the same compilers.])
fi
