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
dnl Copyright ((c)) 2002-2018, Rice University
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

AC_INIT([hpctoolkit], [2017.11],
        [hpctoolkit-forum@rice.edu],
	[hpctoolkit],
	[http://hpctoolkit.org/])
AC_COPYRIGHT([Copyright (c) 2002-2018, Rice University.
See the file README.License for details.])

AC_CONFIG_SRCDIR([src/include/hpctoolkit-config.h.in])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

AC_MSG_NOTICE([*** INFO: from user: CXX='${CXX}' CXXFLAGS='${CXXFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: from user: CC='${CC}' CFLAGS='${CFLAGS}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPICC='${MPICC}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPICXX='${MPICXX}' ***])
AC_MSG_NOTICE([*** INFO: from user: MPIF77='${MPIF77}' ***])

# NOTE: please update config/README.version
AC_PREREQ(2.69)
AM_INIT_AUTOMAKE(1.15.1)
LT_PREREQ([2.4.6])

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
# Git version
#-------------------------------------------------

# If this is a git repo, then try to identify the branch name and the
# date and hash of the last commit.

AC_MSG_CHECKING([for git version])

git_version='unknown (not a git repo)'

mesg=`cd "$top_srcdir" && git log -n 1 --date=short 2>/dev/null`
if echo "$mesg" | grep -i commit >/dev/null ; then
  git_hash=`echo "$mesg" | grep -i commit | head -1 | awk '{print $2}'`
  git_hash=`expr "$git_hash" : '\(........\)'`
  git_date=`echo "$mesg" | grep -i date | head -1 | awk '{print $2}'`

  mesg=`cd "$top_srcdir" && git status 2>/dev/null | head -1`
  if echo "$mesg" | grep -i branch >/dev/null ; then
    git_branch=`echo "$mesg" | sed -e 's/^.*branch//i' | awk '{print $1}'`

  elif echo "$mesg" | grep -i detach >/dev/null ; then
    # detached commits don't have a branch name
    git_branch=detached

  else
    git_branch=unknown
  fi

  git_version="$git_branch at $git_date ($git_hash)"
fi

AC_MSG_RESULT([$git_version])


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
HOST_CPU_AARCH64=no

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

  # ARM 64-bit
  aarch64*linux* )
    HOST_OS="Linux"
    HOST_CPU_AARCH64=yes
    UNWINDER=libunw
    AC_DEFINE([HOST_OS_LINUX], [1], [HOST OS: Linux])
    AC_DEFINE([HOST_CPU_ARM64], [1], [HOST CPU: ARM 64 (aarch64])
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
AM_CONDITIONAL([HOST_CPU_AARCH64], [test "$HOST_CPU_AARCH64" = yes])

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
      HOST_CXXFLAGS="$HOST_CXXFLAGS -DBOOST_NO_CXX11_CONSTEXPR"
      HOST_CXXFLAGS="$HOST_CXXFLAGS -DBOOST_NO_CXX11_VARIADIC_TEMPLATES"
      ;;
esac

# The stdatomic.h file misreads the features of the 4.7.0 gnu k1om
# compiler.  It doesn't have the gnu __atomic_fetch_add() functions.
# We workaround the problem by resetting the minor number to 4.6 to
# more accurately reflect the features the compiler actually has.

case "$host" in
  *k1om* )
      HOST_CFLAGS="$HOST_CFLAGS -U__GNUC_MINOR__ -D__GNUC_MINOR__=6"
      HOST_CXXFLAGS="$HOST_CXXFLAGS -U__GNUC_MINOR__ -D__GNUC_MINOR__=6"
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
AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdint.h>
#include <byteswap.h>
int main(int argc, char **argv)
{
  uint64_t x = (uint64_t) argc;
  uint64_t y = bswap_64(x);
  return y > 2;
}
]])], [use_byteswap=yes], [use_byteswap=no])
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
  AC_LANG_SOURCE([[
#include <list>
int mylen(std::list <int> lst)
{
  int ans = 0;
  for (auto it = lst.begin(); it != lst.end(); it++) {
    ans += 1;
  }
  return ans;
}
]])], [ans=yes], [ans=no])
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


#-------------------------------------------------
# Option: --enable-bgq
#-------------------------------------------------

# Option to select blue gene front or back end.  Default is for back
# end if system is powerpc and looks like blue gene.
#
# Some things (POSIX timers and perf events) won't run on the blue
# gene back end, but this is hard to test for on the front end.

bgq_explicit=no

AC_ARG_ENABLE([bgq],
  [AS_HELP_STRING([--enable-bgq],
      [configure for blue gene/q back end, disable posix timers
       and perf events (default auto-detect)])],
  [bgq_explicit=yes],
  [enable_bgq=check])

# Treat this as blue gene if powerpc and /bgsys/drivers exists, or
# else if explicitly set.

AC_MSG_CHECKING([for blue gene/q back end])

case "$host_cpu" in
  *power* | *ppc* )
    case "$enable_bgq" in
      yes | no )
        ;;
      * )
        if test -d /bgsys/drivers ; then enable_bgq=yes
	else enable_bgq=no
        fi
        ;;
    esac
    ;;
  * )
    enable_bgq=no
    ;;
esac

AC_MSG_RESULT([$enable_bgq])

blue_gene_back_end=no
host_extra_mesg=
warn_bgq_backend=no

if test "$enable_bgq" = yes ; then
  blue_gene_back_end=yes
  host_extra_mesg='(bgq back end)'

  if test "$bgq_explicit" != yes ; then
    warn_bgq_backend=yes
  fi
fi

# Old --enable-bluegene option.  This is still used in itimer.c for
# how we treat WALLCLOCK.

if test "$blue_gene_back_end" = yes ; then
  AC_DEFINE([HOST_SYSTEM_IBM_BLUEGENE], [1], [IBM Blue Gene support])
fi

# Old --enable-bgq-cnk option.  This provides get_hw_tid() and
# Kernel_ProcessorID(), but nothing uses it now.  If you want to
# turn it back on, then fix the hard-coded paths in
# hpcrun/utilities/bgq-cnk and do a proper config test.

AM_CONDITIONAL([OPT_BGQ_BACKEND], false)

# Old --with-upc and --with-upc-lib options.  These only applied to
# blue gene/p which we no longer support.

AM_CONDITIONAL([OPT_ENABLE_UPC], false)

OPT_UPC_IFLAGS=
OPT_UPC_LDFLAGS=

AC_SUBST([OPT_UPC_IFLAGS])
AC_SUBST([OPT_UPC_LDFLAGS])


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
  AC_MSG_NOTICE([externals version: $ext_version])
  if test "x$ext_version" = x0 ; then
    ext_version=
  fi
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

# multilib path based on wordsize of host machine
multilib_path=lib
case "$host_wordsize" in
  32 ) multilib_path='lib32 lib' ;;
  64 ) multilib_path='lib64 lib' ;;
esac

AC_SUBST([wordsize_cflag])


#-------------------------------------------------
# Check for GNU's binutils (binary interface)
#-------------------------------------------------

GNUBINUTILS=no
GNUBINUTILS_IFLAGS=
GNUBINUTILS_LDFLAGS=
GNUBINUTILS_LDLIBS=

if test "$use_externals" = yes ; then
  GNUBINUTILS="$ext_binutils_prefix"
fi

AC_ARG_WITH([binutils],
  [AS_HELP_STRING([--with-binutils=PATH],
	[path to binutils install directory])],
  [GNUBINUTILS="$withval"],
  [])

AC_MSG_CHECKING([for binutils])

case "$GNUBINUTILS" in
  /* )
    if test -f "${GNUBINUTILS}/include/bfd.h" ; then
      GNUBINUTILS_IFLAGS="-I${GNUBINUTILS}/include"
      GNUBINUTILS_LDFLAGS="-L${GNUBINUTILS}/lib"
      for dir in lib32 lib64 ; do
        if test -d "${GNUBINUTILS}/$dir" ; then
	  GNUBINUTILS_LDFLAGS="$GNUBINUTILS_LDFLAGS -L${GNUBINUTILS}/$dir"
	fi
      done
    else
      AC_MSG_ERROR([invalid binutils directory: $GNUBINUTILS])
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([binutils directory must be absolute path])
    ;;
esac

AC_MSG_RESULT([$GNUBINUTILS])

# Test if binutils links with libs in GNUBINUTILS_LDLIBS.
test_binutils_libs()
{
  LIBS="$GNUBINUTILS_LDFLAGS $GNUBINUTILS_LDLIBS"

  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#include <iostream>
#include <bfd.h>
#include <bfdlink.h>
int main(int argc, char **argv)
{
  bfd_init();
  bfd *abfd = bfd_openr(argv[1], "default");
  long size = bfd_get_symtab_upper_bound(abfd);
  std::cout << size;
  return 0;
}
]])], [ans=0], [ans=1])

  return $ans
}

AC_LANG_PUSH([C++])

ORIG_CXXFLAGS="$CXXFLAGS"
ORIG_LIBS="$LIBS"
CXXFLAGS="$HOST_CXXFLAGS $ORIG_CXXFLAGS $GNUBINUTILS_IFLAGS"

# Test for binutils libs.  We always use bfd, opcodes and libiberty,
# and newer binutils also use libdl and libz.

case "$GNUBINUTILS" in
  /* )
    GNUBINUTILS_LDLIBS="-lopcodes -lbfd -liberty"
    if test_binutils_libs ; then :
    else
      GNUBINUTILS_LDLIBS="$GNUBINUTILS_LDLIBS -ldl -lz"
      if test_binutils_libs ; then :
      else
        AC_MSG_WARN([unable to find link flags for binutils])
      fi
    fi
    AC_MSG_NOTICE([binutils libraries: $GNUBINUTILS_LDLIBS])
    ;;
esac

AC_SUBST([GNUBINUTILS_IFLAGS])
AC_SUBST([GNUBINUTILS_LDFLAGS])
AC_SUBST([GNUBINUTILS_LDLIBS])

# Test for special, hpctoolkit-patched binutils.

hpc_binutils=no
LIBS="$GNUBINUTILS_LDFLAGS $GNUBINUTILS_LDLIBS"

case "$GNUBINUTILS" in
  /* )
    AC_MSG_CHECKING([for hpctoolkit patched binutils])

    AC_LINK_IFELSE([
    AC_LANG_SOURCE([[
#include <iostream>
#include <bfd.h>
#include <bfdlink.h>
int main(int argc, char **argv)
{
  bfd_init();
  bfd *abfd = bfd_openr(argv[1], "default");
  bfd_vma vma = bfd_get_first_addr(abfd);
  int ret = bfd_elf_forall_dbg_funcinfo(abfd, NULL, NULL, NULL);
  std::cout << vma << ret;
  return 0;
}
]])], [hpc_binutils=yes], [hpc_binutils=no])

    AC_MSG_RESULT([$hpc_binutils])
    ;;
esac

if test "$hpc_binutils" = yes ; then
  AC_DEFINE([HAVE_HPC_GNUBINUTILS], [1], [HPCToolkit patched binutils])
fi

# Test if binutils has support for IA64 (Itanium).

binutils_ia64=no

case "$GNUBINUTILS" in
  /* )
    AC_MSG_CHECKING([if binutils supports ia64])

    AC_LINK_IFELSE([
    AC_LANG_SOURCE([[
#include <iostream>
#include <bfd.h>
#include <bfdlink.h>
#include <dis-asm.h>
int main(int argc, char **argv)
{
  disassemble_info myinfo;
  bfd_init();
  int ret = print_insn_ia64(0x400400, &myinfo);
  std::cout << ret;
  return 0;
}
]])], [binutils_ia64=yes], [binutils_ia64=no])

    AC_MSG_RESULT([$binutils_ia64])
    ;;
esac

if test "$binutils_ia64" = yes ; then
  AC_DEFINE([ENABLE_BINUTILS_IA64], [1], [Binutils has support for ia64])
fi

AM_CONDITIONAL([OPT_HAVE_BINUTILS_IA64], [test "$binutils_ia64" = yes])

CXXFLAGS="$ORIG_CXXFLAGS"
LIBS="$ORIG_LIBS"

AC_LANG_POP


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

# Test for libboost_thread-mt.so and libboost_system-mt.so.
# If exist, then copy to ext-libs.  Recent dyninst uses these.

AC_MSG_CHECKING([whether to copy boost libraries])

BOOST_LIB=
BOOST_LIB_LIST=
BOOST_COPY=no

for lib in $multilib_path ; do
  boost_libdir="${BOOST}/$lib"
  for f in "$boost_libdir"/libboost_thread* ; do
    if test -f "$f" ; then
      BOOST_LIB="$boost_libdir"
      BOOST_COPY=yes
      break 2
    fi
  done
done

if test "$BOOST_COPY" = yes ; then
  BOOST_LIB_LIST='-lboost_atomic -lboost_chrono -lboost_date_time -lboost_system -lboost_thread'
fi

AC_MSG_RESULT([$BOOST_COPY])

AC_SUBST([BOOST_INC])
AC_SUBST([BOOST_LIB])
AC_SUBST([BOOST_LIB_LIST])
AC_SUBST([BOOST_COPY])

AM_CONDITIONAL([USE_BOOST_LIBS], [test "$BOOST_COPY" = yes])


#-------------------------------------------------
# Option: --with-bzip=PATH
#-------------------------------------------------

# Location of libbz2.so.  This is just for copy-libs as an elfutils
# prereq.  Toolkit doesn't directly call bzip, so we don't need an
# include path here.  Note: this package is not built in externals.

BZIP=no
BZIP_COPY=no
BZIP_LIB=no

AC_ARG_WITH([bzip],
  [AS_HELP_STRING([--with-bzip=PATH],
      [path to bzip install directory])],
  [BZIP="$withval"],
  [])

AC_MSG_CHECKING([for bzip])

case "$BZIP" in
  /* )
    for lib in $multilib_path ; do
      bzip_lib="${BZIP}/$lib"
      if test -f "${bzip_lib}/libbz2.so" ; then
        BZIP_LIB="$bzip_lib"
	break
      fi
    done
    if test "$BZIP_LIB" = no ; then
      AC_MSG_ERROR([unable to find libbz2.so in: $BZIP])
    fi
    BZIP_COPY=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([bzip directory must be absolute path: $BZIP])
    ;;
esac

AC_MSG_RESULT([$BZIP])

AC_SUBST([BZIP_COPY])
AC_SUBST([BZIP_LIB])


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

LIBELF_INC=no
LIBELF_LIB=no
LIBELF_COPY=no

case "$LIBELF" in
  /* )
    if test ! -f "${LIBELF}/include/libelf.h" \
        && test ! -f "${LIBELF}/include/libelf/libelf.h"
    then
      AC_MSG_ERROR([unable to find libelf.h in: $LIBELF])
    fi
    LIBELF_INC="${LIBELF}/include"
    for lib in $multilib_path ; do
      if test -f "${LIBELF}/${lib}/libelf.so" ; then
        LIBELF_LIB="${LIBELF}/${lib}"
	break
      fi
    done
    if test "$LIBELF_LIB" = no ; then
      AC_MSG_ERROR([unable to find libelf.so in: $LIBELF])
    fi
    LIBELF_COPY=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([libelf directory must be absolute path: $LIBELF])
    ;;
esac

AC_MSG_RESULT([$LIBELF])

AC_SUBST([LIBELF_INC])
AC_SUBST([LIBELF_LIB])
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
# Option: --with-libunwind=PATH, --enable-libunwind-primary
#-------------------------------------------------

# Libunwind can be the primary unwinder (any platform), the secondary
# unwinder or not used at all.  So, keep it optional and test static
# and dynamic versions separately.  But if libunwind is the primary
# unwinder, then can't build hpcrun or hpclink without it.

LIBUNWIND=no
LIBUNWIND_STAT=no

LIBUNWIND_IFLAGS=
LIBUNWIND_CPPFLAGS_DYN=
LIBUNWIND_CPPFLAGS_STAT=
LIBUNWIND_LDFLAGS_DYN=
LIBUNWIND_LDFLAGS_STAT=

LIBUNWIND_COPY=no
LIBUNWIND_LIB=

if test "$use_externals" = yes ; then
  LIBUNWIND="$ext_libunwind_prefix"
fi

AC_ARG_WITH([libunwind],
  [AS_HELP_STRING([--with-libunwind=PATH],
      [path to libunwind install directory])],
  [LIBUNWIND="$withval"],
  [])

AC_ARG_ENABLE([libunwind-primary],
  [AS_HELP_STRING([--enable-libunwind-primary],
      [use libunwind as primary unwinder for hpcrun (default no)])],
  [],
  [enable_libunwind_primary=no])

# If libunwind is available, require dynamic (.so) version, and make
# static (.a) optional.  On powerpc, libunwind installs libs into
# lib64, so test for multilib here.

AC_MSG_CHECKING([for libunwind])

case "$LIBUNWIND" in
  /* )
    if test ! -f "${LIBUNWIND}/include/libunwind.h" ; then
      AC_MSG_ERROR([invalid libunwind directory: $LIBUNWIND])
    fi
    LIBUNWIND_IFLAGS="-I${LIBUNWIND}/include"

    for lib in $multilib_path ; do
      unw_libdir="${LIBUNWIND}/$lib"
      if test -f "$unw_libdir/libunwind.so" ; then
        LIBUNWIND_LDFLAGS_DYN="-L${unw_libdir} -lunwind"
    	LIBUNWIND_COPY=yes
	LIBUNWIND_LIB="$unw_libdir"
	if test -f "${unw_libdir}/libunwind.a" ; then
	  LIBUNWIND_STAT=yes
      	  LIBUNWIND_LDFLAGS_STAT="${unw_libdir}/libunwind.a"
	fi
	break
      fi
    done
    if test "x$LIBUNWIND_LIB" = x ; then
      AC_MSG_ERROR([invalid libunwind directory: $LIBUNWIND])
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([libunwind directory must be absolute path: $LIBUNWIND])
    ;;
esac

AC_MSG_RESULT([$LIBUNWIND])
AC_MSG_NOTICE([libunwind static: $LIBUNWIND_STAT])

case "$enable_libunwind_primary" in
  yes | Yes | YES )
    if test "$LIBUNWIND" = no ; then
      AC_MSG_ERROR([enable libunwind primary requires libunwind])
    fi
    UNWINDER=libunw
    ;;
esac

no_unwind_for_hpcrun=no
no_unwind_for_hpclink=no
unwinder_mesg="$UNWINDER"

if test "$UNWINDER" = libunw ; then
  #
  # libunwind is the primary unwinder.  must have libunwind or else
  # can't build hpcrun or hpclink.  don't need CPPFLAGS in this case.
  #
  if test "$LIBUNWIND" = no ; then
    no_unwind_for_hpcrun=yes
  fi
  if test "$LIBUNWIND_STAT" = no ; then
    no_unwind_for_hpclink=yes
  fi
else
  #
  # libunwind is the secondary unwinder or not available.  can build
  # hpcrun without libunwind, but set CPPFLAGS when available.
  #
  if test "$LIBUNWIND" != no ; then
    LIBUNWIND_CPPFLAGS_DYN='-DUSE_LIBUNWIND'
    unwinder_mesg="$UNWINDER + libunw"
  fi
  if test "$LIBUNWIND_STAT" = yes ; then
    LIBUNWIND_CPPFLAGS_STAT='-DUSE_LIBUNWIND'
  fi
fi

AC_MSG_NOTICE([hpcrun unwinder: $unwinder_mesg])

# fixme: only used in hpcrun/main.c because we don't have a libunwind
# version of hpcrun_dump_intervals().
if test "$UNWINDER" = "libunw"; then
   AC_DEFINE([USE_LIBUNW], [1], [libunwind is the specified unwinder])
fi

AC_SUBST(LIBUNWIND_IFLAGS)
AC_SUBST(LIBUNWIND_CPPFLAGS_DYN)
AC_SUBST(LIBUNWIND_CPPFLAGS_STAT)
AC_SUBST(LIBUNWIND_LDFLAGS_DYN)
AC_SUBST(LIBUNWIND_LDFLAGS_STAT)

AC_SUBST(LIBUNWIND_COPY)
AC_SUBST(LIBUNWIND_LIB)

#
# set up automake choice of unwinder
#
AM_CONDITIONAL([UNW_X86], [test "$UNWINDER" = x86-family])
AM_CONDITIONAL([UNW_PPC64], [test "$UNWINDER" = ppc64])
AM_CONDITIONAL([UNW_MIPS], [test "$UNWINDER" = mips])
AM_CONDITIONAL([UNW_LIBUNW], [test "$UNWINDER" = libunw])


#-------------------------------------------------
# Option: --with-lzma=PATH
#-------------------------------------------------

# Test for lzma libraries and add to the build system if available.
# New libunwind (aug 2017) uses liblzma to read compressed sections.
# But allow for older externals that doesn't build them.
#
# Dynamically, add -llzma to libhpcrun.so and copy libs.
# Statically, link liblzma.a into libhpcrun.o at build time.

LZMA=no
LZMA_STATIC=no
LZMA_LDFLAGS_DYN=
LZMA_LDFLAGS_STAT=
LZMA_INC=no
LZMA_COPY=no
LZMA_LIB=
LZMA_PROF_MPI_LIBS=

# allow for older externals which doesn't define ext_lzma_prefix
if test "$use_externals" = yes && test "x$ext_lzma_prefix" != x ; then
  LZMA="$ext_lzma_prefix"
fi

AC_ARG_WITH([lzma],
  [AS_HELP_STRING([--with-lzma=PATH],
      [path to lzma (xz) install directory])],
  [LZMA="$withval"],
  [])

AC_MSG_CHECKING([for lzma])

case "$LZMA" in
  /* )
    for lib in $multilib_path ; do
      lzma_libdir="${LZMA}/$lib"
      if test -f "${lzma_libdir}/liblzma.so" ; then
        LZMA_LDFLAGS_DYN="-L${lzma_libdir} -llzma"
	LZMA_COPY=yes
	LZMA_LIB="$lzma_libdir"
        LZMA_INC="${LZMA}/include"
	if test -f "${lzma_libdir}/liblzma.a" ; then
	  LZMA_STATIC=yes
	  LZMA_LDFLAGS_STAT="${lzma_libdir}/liblzma.a"
	fi
	break
      fi
    done
    if test "x$LZMA_LIB" = x ; then
      AC_MSG_ERROR([invalid lzma directory: $LZMA])
    else
      #
      # hpcprof-mpi: same as hpclink if fully static, else default
      #
      echo "x$HPC_LT_LDFLAGS" | grep -e all-static >/dev/null 2>&1
      if test $? = 0 ; then
        LZMA_PROF_MPI_LIBS="$LZMA_LDFLAGS_DYN"
      else
        LZMA_PROF_MPI_LIBS="$LZMA_LDFLAGS_STAT"
      fi
    
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([lzma directory must be absolute path: $LZMA])
    ;;
esac

AC_MSG_RESULT([$LZMA])
AC_MSG_NOTICE([lzma static: $LZMA_STATIC])

AC_SUBST([LZMA_LDFLAGS_DYN])
AC_SUBST([LZMA_LDFLAGS_STAT])
AC_SUBST([LZMA_PROF_MPI_LIBS])
AC_SUBST([LZMA_COPY])
AC_SUBST([LZMA_INC])
AC_SUBST([LZMA_LIB])


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

# Open Analysis is no longer used.  Now, it's ParseAPI or nothing.
# For the transition, keep the option and issue a warning if used.

OA=no

AC_ARG_WITH([open-analysis],
  [AS_HELP_STRING([--with-open-analysis=PATH],
      [deprecated option, no longer used])],
  [OA="$withval"],
  [])

if test "x$OA" != xno ; then
  AC_MSG_WARN([open analysis is deprecated and no longer used])
fi


#-------------------------------------------------
# Option: --with-papi=PATH
#-------------------------------------------------

OPT_PAPI=no
OPT_PAPI_DYNAMIC=no
OPT_PAPI_STATIC=no
OPT_PAPI_IFLAGS=
OPT_PAPI_LDFLAGS=
OPT_PAPI_LIBPATH=

AC_ARG_WITH([papi],
  [AS_HELP_STRING([--with-papi=PATH],
      [path to papi install directory])],
  [OPT_PAPI="$withval"],
  [])

AC_ARG_ENABLE([force-papi],
  [AS_HELP_STRING([--enable-force-papi],
      [force using both papi and perf events, if configure can't verify
       that papi and perfmon match])],
  [force_papi="$enableval"],
  [force_papi=no])

AC_MSG_CHECKING([for papi])

case "$OPT_PAPI" in
  /* )
    if test ! -f "${OPT_PAPI}/include/papi.h" ; then
      AC_MSG_ERROR([unable to find papi.h in: $OPT_PAPI])
    fi
    OPT_PAPI_IFLAGS="-I${OPT_PAPI}/include"

    for lib in $multilib_path ; do
      lib_dir="${OPT_PAPI}/$lib"
      if test -f "${lib_dir}/libpapi.so" || test -f "${lib_dir}/libpapi.a"
      then
        OPT_PAPI_LIBPATH="$lib_dir"
        OPT_PAPI_LDFLAGS="-L${lib_dir} -lpapi"
	if test -f "${lib_dir}/libpapi.so" ; then
          OPT_PAPI_DYNAMIC=yes
        fi
        if test -f "${lib_dir}/libpapi.a" ; then
          OPT_PAPI_STATIC=yes
	fi
	break
      fi
    done

    if test "x$OPT_PAPI_LIBPATH" = x ; then
      AC_MSG_ERROR([unable to find libpapi in: $OPT_PAPI])
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([papi directory must be absolute path: $OPT_PAPI])
    ;;
esac

AC_MSG_RESULT([$OPT_PAPI])

if test "$OPT_PAPI" != no ; then
  AC_MSG_NOTICE([papi shared libpapi.so: $OPT_PAPI_DYNAMIC])
  AC_MSG_NOTICE([papi static libpapi.a: $OPT_PAPI_STATIC])
fi

# The dynamic case also needs libpapi.so's prerequisite libraries
# (-lpfm).  In the static case, libpapi.a already includes these
# functions.  At least, that's what we've seen so far.

papi_extra_libs=
so_file="${OPT_PAPI_LIBPATH}/libpapi.so"
if test -f "$so_file" ; then
  ldd "$so_file" | grep libpfm >/dev/null 2>&1
  if test $? = 0 ; then
    papi_extra_libs="${papi_extra_libs} -lpfm"
  fi
fi

AC_SUBST(OPT_PAPI)
AC_SUBST(OPT_PAPI_IFLAGS)
AC_SUBST(OPT_PAPI_LDFLAGS)
AC_SUBST(OPT_PAPI_LIBPATH)
AC_SUBST([papi_extra_libs])


#-------------------------------------------------
# Option: --enable-papi-c
#-------------------------------------------------

# Option to use the new papi-c.c sample source file.  This is mostly
# temporary until the new file integrates both component and
# non-component PAPI.  Requires PAPI 5.1 or later.

use_papi_c=yes

AC_ARG_ENABLE([papi-c],
  [AS_HELP_STRING([--enable-papi-c],
      [use component papi, if available (default yes)])],
  [use_papi_c="$enableval"],
  [])

if test "$OPT_PAPI" = no || test "$use_papi_c" != yes ; then
  use_papi_c=no
else
  AC_MSG_CHECKING([for component papi])

  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="$CFLAGS $OPT_PAPI_IFLAGS"
  LIBS="$OPT_PAPI_LDFLAGS $papi_extra_libs"
  AC_LANG_PUSH([C])

  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#include "papi.h"
int main()
{
  return PAPI_get_eventset_component(0);
}
]])], [use_papi_c=yes], [use_papi_c=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"

  AC_MSG_RESULT([$use_papi_c])
fi


#-------------------------------------------------
# Option: --enable-papi-c-cupti
#-------------------------------------------------

use_papi_c_cupti=no

AC_ARG_ENABLE([papi-c-cupti],
  [AS_HELP_STRING([--enable-papi-c-cupti],
      [use papi CUPTI support, if available (default no), requires
       papi cuda component])],
  [use_papi_c_cupti="$enableval"],
  [])

if test "$use_papi_c" = no || test "$use_papi_c_cupti" != yes ; then
  use_papi_c_cupti=no
else
  AC_MSG_CHECKING([for papi cuda component])

  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="$CFLAGS $OPT_PAPI_IFLAGS"
  LIBS="$OPT_PAPI_LDFLAGS $papi_extra_libs"
  AC_LANG_PUSH([C])

  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#include "papi.h"
extern void CUDA_init_component(void);
int main()
{
  CUDA_init_component();
}
]])], [use_papi_c_cupti=yes], [use_papi_c_cupti=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"

  AC_MSG_RESULT([$use_papi_c_cupti])
fi


#-------------------------------------------------
# Option: --with-perfmon=PATH
#-------------------------------------------------

PERFMON=no
PERFMON_STATIC=no
PERFMON_COPY=no
OPT_PERFMON=no

PERFMON_CFLAGS=
PERFMON_LDFLAGS_DYN=
PERFMON_LDFLAGS_STAT=
PERFMON_LIB=

AC_ARG_WITH([perfmon],
  [AS_HELP_STRING([--with-perfmon=PATH],
      [path to perfmon2 (libpfm4) install directory])],
  [perfmon_withval="$withval"],
  [perfmon_withval= ])

# Returns: success (0) if $1 is a valid perfmon install directory.
# Needs header file and at least one library.
valid_perfmon_dir()
{
  dir="$1"

  if test "x$dir" = x  \
     || test ! -f "${dir}/include/perfmon/pfmlib.h" ; then
    return 1
  fi

  for lib in $multilib_path ; do
    if test -f "${dir}/${lib}/libpfm.a"  \
       || test -f "${dir}/${lib}/libpfm.so" ; then
      return 0
    fi
  done

  return 1
}

AC_MSG_CHECKING([for perfmon])

perfmon_mesg=
perfmon_is_from_papi=no

# First, find which perfmon directory, if any.

if test "x$perfmon_withval" = xno
then
  # if explicitly set to no, then leave disabled
  PERFMON=no
  perfmon_mesg='no (disabled)'

elif test "x$OPT_PAPI" != xno && valid_perfmon_dir "$OPT_PAPI"
then
  # if --with-papi exists and includes perfmon, use that to keep papi
  # and perfmon consistent.
  PERFMON="$OPT_PAPI"
  perfmon_mesg="(from papi) $OPT_PAPI"
  perfmon_is_from_papi=yes

elif test "$blue_gene_back_end" = yes ; then
  # disable separate perfmon for blue gene
  PERFMON=no
  perfmon_mesg='no (disabled for blue gene)'

elif test "xperfmon_withval" != x && valid_perfmon_dir "$perfmon_withval"
then
  # if --with-perfmon is set and works, use that
  PERFMON="$perfmon_withval"
  perfmon_mesg="$perfmon_withval"

elif test "$use_externals" = yes && test "x$ext_perfmon_prefix" != x  \
     && valid_perfmon_dir "$ext_perfmon_prefix"
then
  # if value from externals works, use that
  PERFMON="$ext_perfmon_prefix"
  perfmon_mesg="$ext_perfmon_prefix"

else
  # give up, nothing works
  PERFMON=no
  perfmon_mesg=no
fi

AC_MSG_RESULT([$perfmon_mesg])

# Second, set the flags variables, etc.

case "$PERFMON" in
  /* )
    PERFMON_CFLAGS="-I${PERFMON}/include -DENABLE_PERFMON"
    OPT_PERFMON=yes

    for lib in $multilib_path ; do
      lib_dir="${PERFMON}/$lib"
      if test -f "${lib_dir}/libpfm.so" || test -f "${lib_dir}/libpfm.a"
      then
        PERFMON_LIB="$lib_dir"
	PERFMON_COPY=yes
	if test -f "${lib_dir}/libpfm.so" ; then
	  PERFMON_LDFLAGS_DYN="-L${lib_dir} -lpfm"
        fi
        if test -f "${lib_dir}/libpfm.a" ; then
	  PERFMON_LDFLAGS_STAT="${lib_dir}/libpfm.a"
          PERFMON_STATIC=yes
	fi
        break
      fi
    done

    if test "x$PERFMON_LIB" = x ; then
      AC_MSG_ERROR([unable to find libpfm in: $PERFMON])
    fi
    ;;
esac

AC_SUBST([PERFMON_CFLAGS])
AC_SUBST([PERFMON_LIB])
AC_SUBST([PERFMON_LDFLAGS_DYN])
AC_SUBST([PERFMON_LDFLAGS_STAT])
AC_SUBST([PERFMON_COPY])

AM_CONDITIONAL(OPT_PERFMON, [test "$OPT_PERFMON" = yes])


#-------------------------------------------------
# Option: --enable-perf-events
#-------------------------------------------------

# This option is mostly for compute nodes where it's hard to run a
# config test.

AC_ARG_ENABLE([perf-events],
  [AS_HELP_STRING([--enable-perf-events],
      [force enable or disable perf events in hpcrun (normally 2.6.32 or later),
       only needed if fails to auto-detect correctly])],
  [perf_event="$enableval"],
  [perf_event=check])

PERF_EVENT_PARANOID_PATH="/proc/sys/kernel/perf_event_paranoid"
PERF_EVENT_PARANOID="-1"

if test -f "$PERF_EVENT_PARANOID_PATH" ; then
  PERF_EVENT_PARANOID=`cat "$PERF_EVENT_PARANOID_PATH"`
fi

AC_MSG_CHECKING([for perf events])

case "$perf_event" in
  yes ) perf_event_mesg="yes (forced)" ;;
  no )  perf_event_mesg="no (disabled)" ;;
  * )
    AC_COMPILE_IFELSE([
    AC_LANG_SOURCE([[
#include <linux/perf_event.h>
static struct perf_event_attr attr;
void check_kernel()
{
  attr.sample_id_all = 1;
}
]])], [perf_event=yes], [perf_event=no])
    perf_event_mesg="$perf_event"
    ;;
esac

AC_MSG_RESULT([$perf_event_mesg])

AC_MSG_NOTICE([perf paranoid in ${PERF_EVENT_PARANOID_PATH} : ${PERF_EVENT_PARANOID}])

# Perf events won't run and throws an illegal instruction on blue gene
# back end.
if test "$blue_gene_back_end" = yes ; then
  AC_MSG_NOTICE([disable perf events for blue gene back end])
  perf_event=no
  perf_event_mesg='no (disabled for blue gene)'
fi

# Perf events doesn't exactly require perfmon, but if not available,
# then mnemonic names won't work.

warn_no_perfmon=no

if test "$perf_event" = yes && test "$PERFMON" = no ; then
  AC_MSG_WARN([perf events is enabled but perfmon is not available])
  warn_no_perfmon=yes
fi

AC_SUBST([PERF_EVENT_PARANOID])

AM_CONDITIONAL(OPT_ENABLE_PERF_EVENT, [test "$perf_event" = yes])


#-------------------------------------------------
# Option: --enable-kernel-blocking
#-------------------------------------------------

# Again, the option is mostly for compute nodes.

AC_ARG_ENABLE([kernel-blocking],
  [AS_HELP_STRING([--enable-kernel-blocking],
      [force enable or disable kernel blocking support in perf events
       (normally 4.3 or later), only needed if fails to auto-detect correctly])],
  [kernel_blocking="$enableval"],
  [kernel_blocking=check])

if test "$perf_event" = no ; then
  kernel_blocking=no
  kernel_blocking_mesg="no (perf-events is off)"
else
  AC_MSG_CHECKING([for kernel blocking in perf events])

  case "$kernel_blocking" in
    yes ) kernel_blocking_mesg="yes (forced)" ;;
    no )  kernel_blocking_mesg="no (disabled)" ;;
    * )
      AC_COMPILE_IFELSE([
      AC_LANG_SOURCE([[
#include <linux/perf_event.h>
static struct perf_event_attr attr;
void check_kernel()
{
  attr.context_switch = 1;
}
]])], [kernel_blocking=yes], [kernel_blocking=no])
    kernel_blocking_mesg="$kernel_blocking"
    ;;
  esac

  AC_MSG_RESULT([$kernel_blocking])
fi

AM_CONDITIONAL([OPT_ENABLE_KERNEL_4_3], [test "$kernel_blocking" = yes])


#-------------------------------------------------
# Test consistency of papi and perfmon
#-------------------------------------------------

warn_papi_mismatch=no

papi_mesg="$OPT_PAPI"

if test "$OPT_PAPI" != no && test "$perf_event" = yes  \
   && test "$PERFMON" != no && test "$perfmon_is_from_papi" = no
then
  AC_MSG_WARN([unable to verify that papi and perfmon are consistent])

  if test "$force_papi" = yes ; then
    AC_MSG_WARN([force using papi])
    papi_mesg="(force enable) $OPT_PAPI"

  else
    AC_MSG_WARN([disable papi due to possible conflict with perfmon])
    papi_mesg='no (disabled due to conflict with perfmon)'
    warn_papi_mismatch=yes

    OPT_PAPI=no
    OPT_PAPI_DYNAMIC=no
    OPT_PAPI_STATIC=no
    OPT_PAPI_IFLAGS=
    OPT_PAPI_LDFLAGS=
    OPT_PAPI_LIBPATH=
    use_papi_c=no
    use_papi_c_cupti=no
  fi
fi

# AC-SUBST can be anywhere, but AM-CONDITIONAL reads the value now.

AM_CONDITIONAL(OPT_PAPI_DYNAMIC, [test "$OPT_PAPI_DYNAMIC" = yes])
AM_CONDITIONAL(OPT_PAPI_STATIC,  [test "$OPT_PAPI_STATIC" = yes])
AM_CONDITIONAL(OPT_PAPI_COMPONENT, [test "$use_papi_c" = yes])
AM_CONDITIONAL(OPT_PAPI_CUPTI, [test "$use_papi_c_cupti" = yes])


#-------------------------------------------------
# Option: --with-symtabAPI=PATH
#-------------------------------------------------

# Now require full ParseAPI with CFG loop support or nothing.

SYMTABAPI=no
if test "$use_externals" = yes ; then
  SYMTABAPI="$ext_symtabAPI_prefix"
fi

AC_ARG_WITH([symtabAPI],
  [AS_HELP_STRING([--with-symtabAPI=PATH],
      [path to symtabAPI (dyninst) install directory])],
  [SYMTABAPI="$withval"],
  [])

#
# Basic test that symtab and parseapi files exist.
#
AC_MSG_CHECKING([for symtabAPI])

SYMTABAPI_COPY=no
SYMTABAPI_LIB_LIST=
hpcstruct_mesg=no

case "${SYMTABAPI}" in
  /* )
    if test -f "${SYMTABAPI}/include/Symtab.h"  \
       && test -f "${SYMTABAPI}/include/CFG.h"  \
       && test -f "${SYMTABAPI}/lib/libsymtabAPI.so"  \
       && test -f "${SYMTABAPI}/lib/libparseAPI.so"
    then :
    else
       AC_MSG_ERROR([invalid symtabAPI directory: $SYMTABAPI])
    fi
    SYMTABAPI_COPY=yes
    SYMTABAPI_LIB_LIST='-lparseAPI -linstructionAPI -lsymtabAPI -lcommon'
    SYMTABAPI_LIB_LIST="$SYMTABAPI_LIB_LIST -ldynDwarf -ldynElf"
    hpcstruct_mesg=parseapi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([symtabAPI directory must be absolute path: $SYMTABAPI])
    ;;
esac

AC_MSG_RESULT([$SYMTABAPI])

SYMTABAPI_INC="${SYMTABAPI}/include"
SYMTABAPI_LIB="${SYMTABAPI}/lib"

AC_SUBST([SYMTABAPI_COPY])
AC_SUBST([SYMTABAPI_LIB_LIST])
AC_SUBST([SYMTABAPI_INC])
AC_SUBST([SYMTABAPI_LIB])

#-------------------------------------------------

#
# Chcek that ParseAPI includes CFG loop support.
#
ORIG_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="${HOST_CXXFLAGS} ${CXXFLAGS} -I${BOOST_INC} -I${SYMTABAPI_INC}"
AC_LANG_PUSH([C++])

if test "$SYMTABAPI" != no ; then
  AC_MSG_CHECKING([for parseAPI CFG loop support])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
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
]])], [cfg_loop_support=yes], [cfg_loop_support=no])

  AC_MSG_RESULT([$cfg_loop_support])

  if test "$cfg_loop_support" != yes ; then
    AC_MSG_ERROR([parseAPI does not include CFG loop support])
  fi
fi

#
# Test if dyninst was built with libdw or libdwarf.
#
# There's not a good compile or link test for this, so we fall back on
# ldd.  But note: ldd only works for dynamic libraries and doesn't
# work cross-platform.
#
use_libdw=no

if test "$SYMTABAPI" != no ; then
  AC_MSG_CHECKING([if dyninst built with libdw.so])

  dyn_dwarf="${SYMTABAPI_LIB}/libdynDwarf.so"

  ORIG_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
  LD_LIBRARY_PATH=

  if test -f "$dyn_dwarf" ; then
    ldd "$dyn_dwarf" 2>/dev/null | $AWK '{print $1}' | grep -e 'libdw.so' >/dev/null
    if test $? -eq 0 ; then
       use_libdw=yes
    fi
  fi

  LD_LIBRARY_PATH="$ORIG_LD_LIBRARY_PATH"

  AC_MSG_RESULT([$use_libdw])
fi

#
# Test if dyninst supports cuda.
#
use_cuda=no

if test "$SYMTABAPI" != no ; then
  AC_MSG_CHECKING([if dyninst supports cuda])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <Symtab.h>
using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
long cuda_type()
{
  return (long) Dyninst::Arch_cuda;
}
]])], [use_cuda=yes], [use_cuda=no])

  AC_MSG_RESULT([$use_cuda])
fi

#
# Test if ParseAPI uses Instruction or Instruction::Ptr.
#
use_instn_ptr=yes

if test "$SYMTABAPI" != no ; then
  AC_MSG_CHECKING([if parseapi uses Instruction::Ptr])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <map>
#include <Symtab.h>
#include <Function.h>
#include <Instruction.h>
#include <CFG.h>
using namespace Dyninst;
using namespace InstructionAPI;
using namespace ParseAPI;
using namespace std;
long num_instructions(Block * block)
{
  map <Offset, Instruction::Ptr> imap;
  block->getInsns(imap);
  return (long) imap.size();
}
]])], [use_instn_ptr=yes], [use_instn_ptr=no])

  AC_MSG_RESULT([$use_instn_ptr])
fi

if test "$use_instn_ptr" = yes ; then
  AC_DEFINE(DYNINST_INSTRUCTION_PTR, 1, [dyninst uses Instruction::Ptr])
fi

AC_LANG_POP
CXXFLAGS="$ORIG_CXXFLAGS"

#
# Temporary option to identify version of dyninst, only necessary if
# the above tests produce the wrong answer.
#
#  old = orig version from externals
#  libdw = uses libdw instead of libdwarf, but not cuda
#  cuda = uses libdw and supports cuda
#
dyninst_type=unset

AC_ARG_WITH([dyninst-type],
  [AS_HELP_STRING([--with-dyninst-type=TYPE],
      [temp option for dyninst version (old, libdw, cuda),
       only needed if autoconf fails to detect correctly])],
  [dyninst_type="$withval"],
  [])

if test "$dyninst_type" != unset ; then
  case "$dyninst_type" in
    old )   use_libdw=no ;  use_cuda=no ;;
    libdw ) use_libdw=yes ; use_cuda=no ;;
    cuda )  use_libdw=yes ; use_cuda=yes ;;
    * ) AC_MSG_ERROR([bad value for --with-dyninst-type: $dyninst_type]) ;;
  esac

  AC_MSG_NOTICE([override dyninst-type ($dyninst_type): libdw: $use_libdw, cuda: $use_cuda])
fi

AM_CONDITIONAL(OPT_DYNINST_LIBDWARF, [test "$use_libdw" = no])
AM_CONDITIONAL(OPT_DYNINST_LIBDW, [test "$use_libdw" = yes])

if test "$use_libdw" = yes ; then
  AC_DEFINE(DYNINST_USE_LIBDW, 1, [dyninst built with libdw])
fi

if test "$use_cuda" = yes ; then
  AC_DEFINE(DYNINST_USE_CUDA, 1, [dyninst supports cuda])
fi

LIBDW_COPY="$use_libdw"
AC_SUBST([LIBDW_COPY])


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
    AC_LANG_SOURCE([[
#include <stdio.h>
#include <xed-interface.h>
int my_xed_init(void)
{
  xed_tables_init();
  return 0;
}
]])], [xed2_static_fpic=yes], [xed2_static_fpic=no])
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


#-------------------------------------------------
# Option: --with-zlib=PATH
#-------------------------------------------------

ZLIB=no
if test "$use_externals" = yes ; then
  ZLIB="$ext_zlib_prefix"
fi

AC_ARG_WITH([zlib],
  [AS_HELP_STRING([--with-zlib=PATH],
      [path to zlib install directory])],
  [ZLIB="$withval"],
  [])

AC_MSG_CHECKING([zlib])

ZLIB_INC=no
ZLIB_LIB=no
ZLIB_HPCLINK_LIB=no
ZLIB_COPY=no

case "$ZLIB" in
  /* )
    if test ! -f "${ZLIB}/include/zlib.h" ; then
      AC_MSG_ERROR([unable to find zlib.h in: $ZLIB])
    fi
    ZLIB_INC="${ZLIB}/include"
    for lib in $multilib_path ; do
      if test -f "${ZLIB}/${lib}/libz.so" && test -f "${ZLIB}/${lib}/libz.a" ; then
        ZLIB_LIB="${ZLIB}/${lib}"
	ZLIB_HPCLINK_LIB="${ZLIB}/${lib}/libz.a"
	break
      fi
    done
    if test "$ZLIB_LIB" = no ; then
      AC_MSG_ERROR([unable to find zlib.so or zlib.a in: $ZLIB])
    fi
    ZLIB_COPY=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([zlib directory must be absolute path: $ZLIB])
    ;;
esac

AC_MSG_RESULT([$ZLIB])

AM_CONDITIONAL([OPT_USE_ZLIB], [test "$ZLIB" != no])

AC_SUBST([ZLIB_INC])
AC_SUBST([ZLIB_LIB])
AC_SUBST([ZLIB_HPCLINK_LIB])
AC_SUBST([ZLIB_COPY])


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
     test "$no_xed_for_hpcrun" = yes || test "$no_unwind_for_hpcrun" = yes
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

if test "$OPT_LIBMONITOR_STATIC" = no || test "$no_xed_for_hpclink" = yes \
   || test "$no_unwind_for_hpclink" = yes
then
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

AC_MSG_CHECKING([whether to build MPI wrapper support])

AC_ARG_ENABLE([mpi-wrapper],
  [AS_HELP_STRING([--enable-mpi-wrapper],
      [enable MPI wrapper support, requires MPICC, MPIF77 and MPI_INC
       (default no)])],
  [],
  [enable_mpi_wrapper=no])

case "$enable_mpi_wrapper" in
  yes | Yes | YES ) enable_mpi_wrapper=yes ;;
  * ) enable_mpi_wrapper=no ;;
esac

AC_MSG_RESULT([$enable_mpi_wrapper])

AC_ARG_VAR([MPICC], [MPI C compiler for MPI wrapper support])
AC_ARG_VAR([MPIF77], [MPI Fortran compiler for MPI wrapper support])
AC_ARG_VAR([MPI_INC], [MPI include directory for MPI wrapper support])

mpi_const_type=no

if test "$enable_mpi_wrapper" = yes ; then
  #
  # check that MPICC works
  #
  if test "x$MPICC" = x ; then
    MPICC=mpicc
  fi

  AC_LANG_PUSH([C])
  ORIG_CC="$CC"
  CC="$MPICC"

  AC_MSG_CHECKING([MPI wrapper C compiler: $MPICC ])
  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <mpi.h>
int my_size(void)
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}
]])], [ans=yes], [ans=no])
  AC_MSG_RESULT([$ans])

  if test "$ans" = no ; then
    AC_MSG_WARN([invalid MPICC compiler for MPI wrappers: $MPICC ])
    AC_MSG_ERROR([reconfigure with MPICC set to valid MPI compiler])
  fi
  #
  # check MPI_INC include path
  #
  AC_MSG_CHECKING([MPI wrapper include path: $MPI_INC ])
  ans=no
  if test -f "${MPI_INC}/mpi.h" ; then
    ans=yes
  fi
  AC_MSG_RESULT([$ans])

  if test "$ans" = no ; then
    AC_MSG_WARN([no mpi.h in MPI_INC include path: $MPI_INC])
    AC_MSG_ERROR([reconfigure with MPI_INC set to MPI include directory])
  fi
  #
  # check if MPI uses 'void *buf' or 'const void *buf'
  #
  AC_MSG_CHECKING([if MPI types use 'const void *buf'])
  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <mpi.h>
int MPI_Send(void *buf, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm)
{
  return 0;
}
]])], [mpi_const_type=no], [mpi_const_type=yes])
  AC_MSG_RESULT([$mpi_const_type])

  CC="$ORIG_CC"
  AC_LANG_POP
fi

MPI_PROTO_FILE=mpi.protos
if test "$mpi_const_type" = yes ; then
  MPI_PROTO_FILE=mpi.protos.const
fi

AC_SUBST([MPI_PROTO_FILE])
AM_CONDITIONAL(OPT_ENABLE_MPI_WRAP, [test "$enable_mpi_wrapper" = yes])


#-------------------------------------------------
# enable-mpi-wrapper: check F77 symbols
#-------------------------------------------------

if test "$enable_mpi_wrapper" = yes ; then

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
AC_LANG_SOURCE([[
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
]])], [ans=yes], [ans=no])

AC_MSG_RESULT([$ans])

# The POSIX timers pass this test on blue gene, but they don't work on
# the back end.
if test "$blue_gene_back_end" = yes ; then
  AC_MSG_NOTICE([disable POSIX timers for blue gene back end])
  ans=no
fi

# If CLOCK_REALTIME exists, then also try CLOCK_THREAD_CPUTIME_ID.

if test "$ans" = yes ; then
  AC_DEFINE([ENABLE_CLOCK_REALTIME], [1],
            [Support for CLOCK_REALTIME and SIGEV_THREAD_ID])

  AC_MSG_CHECKING([for CLOCK_THREAD_CPUTIME_ID])
  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#include <sys/time.h>
#include <time.h>
int main()
{
  clockid_t clock = CLOCK_THREAD_CPUTIME_ID;
  return 0;
}
]])], [ans=yes], [ans=no])
  AC_MSG_RESULT([$ans])

  if test "$ans" = yes ; then
    AC_DEFINE([ENABLE_CLOCK_CPUTIME], [1], [Support for CLOCK_THREAD_CPUTIME_ID])
  fi
fi

LIBS="$ORIG_LIBS"
AC_LANG_POP


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
  src/tool/hpctracedump/Makefile \
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
AC_MSG_NOTICE([  git version: $git_version])
if test "x$ext_version" != x ; then
  AC_MSG_NOTICE([  externals:   $ext_version])
fi
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  prefix: $prefix])
AC_MSG_NOTICE([  build:  $build])
AC_MSG_NOTICE([  host:   ${host} ${host_extra_mesg}])
AC_MSG_NOTICE([  host endian: $host_endian])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  hpcrun?:      ${enable_hpcrun_mesg}])
AC_MSG_NOTICE([  hpclink?:     ${OPT_ENABLE_HPCRUN_STATIC}])
AC_MSG_NOTICE([  hpcrun-flat?: ${OPT_ENABLE_HPCRUN_FLAT}])
AC_MSG_NOTICE([  unwinder:     ${unwinder_mesg}])
AC_MSG_NOTICE([  mpi support?: ${OPT_ENABLE_MPI}])
AC_MSG_NOTICE([  mpi wrapper?: ${enable_mpi_wrapper}])
AC_MSG_NOTICE([  hpcrun/lush?: ${opt_enable_lush}])
AC_MSG_NOTICE([  datacentric?: ${OPT_ENABLE_DATACENTRIC_TRACE}])
AC_MSG_NOTICE([  hpcserver?:   ${enable_hpcserver}])
AC_MSG_NOTICE([  hpcstruct:    $hpcstruct_mesg])
AC_MSG_NOTICE([  perf-events:  ${perf_event_mesg}])
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
AC_MSG_NOTICE([  bzip:         ${BZIP}])
AC_MSG_NOTICE([  libdwarf:     ${LIBDWARF}])
AC_MSG_NOTICE([  libelf:       ${LIBELF}])
AC_MSG_NOTICE([  libmonitor:   ${LIBMONITOR}])
AC_MSG_NOTICE([  monitor/old:  ${MONITOR}])
AC_MSG_NOTICE([  libunwind:    ${LIBUNWIND}])
AC_MSG_NOTICE([  lzma:         ${LZMA}])
AC_MSG_NOTICE([  perfmon:      ${perfmon_mesg}])
AC_MSG_NOTICE([  symtabAPI:    ${SYMTABAPI}])
AC_MSG_NOTICE([  xed2:         ${XED2}])
AC_MSG_NOTICE([  xerces:       ${XERCES}])
AC_MSG_NOTICE([  zlib:         ${ZLIB}])
AC_MSG_NOTICE([  papi:         ${papi_mesg}])
AC_MSG_NOTICE([  cuda:         ${OPT_CUDA}])
AC_MSG_NOTICE([  papi-c-cupti: ${use_papi_c_cupti}])

if test "x$OA" != xno ; then
  AC_MSG_WARN([open analysis is deprecated and no longer used])
fi

if test "$warn_config_site" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([You have CONFIG_SITE set in your environment.])
  AC_MSG_WARN([This may break some of hpctoolkit's install paths and])
  AC_MSG_WARN([launch scripts.  Set CONFIG_SITE=NONE or else unset it])
  AC_MSG_WARN([from your environment and rerun configure.])
fi

if test "$warn_bgq_backend" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_NOTICE([Note: HPCToolkit is configured for Blue Gene/Q back end.])
  AC_MSG_NOTICE([If you wanted to build for the front end (regular powerpc),])
  AC_MSG_NOTICE([then re-run configure with '--disable-bgq'.])
fi

if test "$warn_papi_mismatch" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([PAPI is disabled due to possible conflict between perf events])
  AC_MSG_WARN([and papi versions of perfmon (normally pfmlib.h is missing])
  AC_MSG_WARN([from papi include dir).  Possible solutions:])
  AC_MSG_WARN([ 1. disable papi with:  --without-papi])
  AC_MSG_WARN([ 2. disable perf events with:  --disable-perf-events])
  AC_MSG_WARN([ 3. force enable both with:  --enable-force-papi])
  AC_MSG_WARN([   if you're sure that papi was built with the same version])
  AC_MSG_WARN([   of perfmon (but may deadlock or segfault if mismatched).])
fi

if test "$warn_no_perfmon" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Perf events is enabled, but perfmon is not available.])
  AC_MSG_WARN([Mnemonic names will not be available for perf events.])
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
