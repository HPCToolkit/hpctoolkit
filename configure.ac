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
dnl Copyright ((c)) 2002-2021, Rice University
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

#----------------------------------------------------------------------------
# Initialization:
#----------------------------------------------------------------------------

AC_INIT([hpctoolkit], [2021.03.01-master],
        [hpctoolkit-forum@rice.edu],
	[hpctoolkit],
	[http://hpctoolkit.org/])
AC_COPYRIGHT([Copyright (c) 2002-2021, Rice University.
See the file README.License for details.])

AC_CONFIG_SRCDIR([src/include/hpctoolkit-config.h.in])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

AC_MSG_NOTICE([INFO: from user: CC='${CC}'  CFLAGS='${CFLAGS}'])
AC_MSG_NOTICE([INFO: from user: CXX='${CXX}'  CXXFLAGS='${CXXFLAGS}'])
AC_MSG_NOTICE([INFO: from user: MPICC='${MPICC}'])
AC_MSG_NOTICE([INFO: from user: MPICXX='${MPICXX}'])
AC_MSG_NOTICE([INFO: from user: MPIF77='${MPIF77}'])

# NOTE: please update config/README.version
AC_PREREQ(2.69)
AM_INIT_AUTOMAKE(1.15.1)
LT_PREREQ([2.4.6])

AM_MAINTAINER_MODE()

AC_CANONICAL_BUILD()
AC_CANONICAL_HOST()

#----------------------------------------------------------------------------
# Setup
#----------------------------------------------------------------------------

# ${srcdir}, ${ac_top_srcdir}, ${ac_top_builddir}
hpctoolkit_top=`cd ${srcdir} && pwd`
hpctoolkit_build=`pwd`
HPCTOOLKIT_PLATFORM=`cd ${hpctoolkit_top}/config && ./hpcplatform`
AC_SUBST([HPCTOOLKIT_PLATFORM])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_INSTALL_PREFIX],
  ["$prefix"], [HPCToolkit install prefix])

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
# Git and Spack versions
#-------------------------------------------------

# If this is a git repo, then try to identify the branch name and the
# date and hash of the last commit.

AC_MSG_CHECKING([for git version])

git_version='unknown (not a git repo)'

if test -d "${srcdir}/.git" ; then
  mesg=`cd "$srcdir" && git log -n 1 --date=short 2>/dev/null`
  if echo "$mesg" | grep -i commit >/dev/null ; then
    git_hash=`echo "$mesg" | grep -i commit | head -1 | awk '{print $2}'`
    git_hash=`expr "$git_hash" : '\(................\)'`
    git_date=`echo "$mesg" | grep -i date | head -1 | awk '{print $2}'`

    mesg=`cd "$srcdir" && git status 2>/dev/null | head -1`
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
fi

AC_MSG_RESULT([$git_version])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_VERSION],
	  ["${PACKAGE_VERSION}"],
	  [HPCToolkit version])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_VERSION_STRING],
	  ["A member of HPCToolkit, version ${PACKAGE_VERSION}"],
	  [HPCToolkit version string])

# If this is a spack build, then try to identify the spack spec.

AC_MSG_CHECKING([for spack spec])

spack_spec='unknown (not a spack build)'

if test "x$SPACK_SHORT_SPEC" != x ; then
  spack_spec=`echo $SPACK_SHORT_SPEC | tr -d "'"`
fi

AC_MSG_RESULT([$spack_spec])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_GIT_VERSION],
    ["$git_version"], [Git branch and commit hash, if known.])

AC_DEFINE_UNQUOTED([HPCTOOLKIT_SPACK_SPEC],
    ["$spack_spec"], [Spack version and variants, if known.])


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
# enable-all-static
#-------------------------------------------------

# Add '-all-static' to LDFLAGS for hpcprof-mpi.  This is needed when
# using an MPI compiler that builds a static hpcprof-mpi for the
# compute nodes.
#
# The old way was to set HPCPROFMPI_LT_LDFLAGS='-all-static', the new
# way is to use --enable-all-static.

AC_ARG_VAR([HPCPROFMPI_LT_LDFLAGS],
  [old method to add ldflags to hpcprof-mpi, now use --enable-all-static
  to build a static binary for the compute nodes])

AC_ARG_ENABLE([all-static],
  [AS_HELP_STRING([--enable-all-static],
      [needed when building a static hpcprof-mpi for the compute nodes
      (default no)])],
  [],
  [enable_all_static=no])

if test "x$enable_all_static" = xyes ; then
  HPCPROFMPI_LT_LDFLAGS="$HPCPROFMPI_LT_LDFLAGS -all-static"
fi

AC_MSG_NOTICE([HPCPROFMPI_LT_LDFLAGS: $HPCPROFMPI_LT_LDFLAGS])


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
case "$host_cpu" in
  *aarch* | *arm* ) default=no ;;
esac

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

AM_PROG_AR

# Note: this will set CC and CFLAGS if not already set
LT_INIT()
AC_SUBST([LIBTOOL_DEPS])

AM_PROG_AS()
AC_PROG_AWK
AC_PROG_SED

AC_PROG_CXXCPP()

AC_PROG_INSTALL()

AC_MSG_NOTICE([INFO: post autoconf tests: CC='${CC}'  CFLAGS='${CFLAGS}'])
AC_MSG_NOTICE([INFO: post autoconf tests: CXX='${CXX}'  CXXFLAGS='${CXXFLAGS}'])
AC_MSG_NOTICE([INFO: post autoconf tests: MPICC='${MPICC}'])
AC_MSG_NOTICE([INFO: post autoconf tests: MPICXX='${MPICXX}'])
AC_MSG_NOTICE([INFO: post autoconf tests: MPIF77='${MPIF77}'])


#-------------------------------------------------
# Other build tools: ld, objcopy
#-------------------------------------------------

# Autotools searches for LD automatically.  The ARG-VAR's are only if
# autotools fails to find a good program.  We could change this to use
# the spack-built binutils.

AC_ARG_VAR([LD],
    [the ld program, only needed if autotools fails to find a suitable program])

AC_ARG_VAR([OBJCOPY],
    [the objcopy program, only needed if autotools fails to find a suitable program])

# Search path for objcopy
AC_PATH_PROG([path_objcopy], [objcopy], [objcopy])

if test "x$OBJCOPY" = x ; then
  OBJCOPY="$path_objcopy"
fi

AC_MSG_NOTICE([ar is $AR])
AC_MSG_NOTICE([ld is $LD])
AC_MSG_NOTICE([objcopy is $OBJCOPY])


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
AC_SUBST([HOST_CPU_AARCH64])

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
# Utilities for compiler versions and paths
#-------------------------------------------------

# These tests only use GNU syntax.  Intel, Clang, etc use a different
# format, but we don't really build with those.
# For example:  gcc (GCC) 7.3.1 20180130 (Red Hat 7.3.1-2)

find_version()
{
  ans='unknown non-gnu'

  # strip any compiler flags, 'gcc -m64'
  base=`echo $1 | awk '{ print $1 }'`
  version=`$base --version 2>/dev/null`
  echo "$version" | grep -i copyright >/dev/null
  if test $? -eq 0 ; then
    ans=`echo "$version" | head -1`
  fi

  echo "$ans"
}

find_path()
{
  ans=
  base=`echo $1 | awk '{ print $1 }'`
  case "$base" in
    /* ) ans="$base" ;;
    * )  AC_PATH_PROG([ans], [$base], [no]) ;;
  esac
  if test "x$ans" = xno ; then
    ans=unknown
  fi

  echo "$ans"
}

test_is_gnu()
{
  base=`echo $1 | awk '{ print $1 }'`
  version=`$base --version 2>/dev/null`
  echo $version | grep -E -i -e 'copy.*free.*soft.*found' >/dev/null

  return $?
}


#-------------------------------------------------
# C, C++ compiler versions and paths
#-------------------------------------------------

# Find path and version for CC/CXX, test if GNU, and check if recent
# enough version >= 5.x.

cc_is_gnu=yes

CC_VERSION=`find_version "$CC"`
CC_PATH=`find_path "$CC"`

AC_MSG_NOTICE([C compiler: $CC_VERSION])
AC_MSG_NOTICE([C path: $CC_PATH])

if test_is_gnu "$CC" ; then : ; else
  AC_MSG_WARN([Not using the GNU C compiler.])
  cc_is_gnu=no
fi

CXX_VERSION=`find_version "$CXX"`
CXX_PATH=`find_path "$CXX"`

AC_MSG_NOTICE([C++ compiler: $CXX_VERSION])
AC_MSG_NOTICE([C++ path: $CXX_PATH])

if test_is_gnu "$CXX" ; then : ; else
  AC_MSG_WARN([Not using the GNU C++ compiler.])
  cc_is_gnu=no
fi

warn_non_gnu=no
warn_non_gnu_mesg=

warn_old_gnu=no

warn_identify_compiler=no
warn_identify_compiler_mesg=
have_cc_version=no

# parse CC version: gcc (GCC) 7.3.1 20180130 (Red Hat 7.3.1-2)
if test "$cc_is_gnu" = yes ; then
  ver=`echo $CC_VERSION | sed -e '[s/^[^)]*[)]//]' | awk '{print $1}'`
  cc_major=`echo $ver | awk -F. '{print $1}'`
  cc_minor=`echo $ver | awk -F. '{print $2}'`

  if test "x$cc_minor" != x ; then
    have_cc_version=yes
    if test "$cc_major" -lt 5 ; then
      AC_MSG_WARN([CC/CXX compiler too old for hpctoolkit])
      AC_MSG_WARN([Use GNU version 5.x or later])
      warn_old_gnu=yes
    fi
  else
    AC_MSG_WARN([unable to identify CC/CXX compiler version: $CC_VERSION])
    warn_identify_compiler=yes
    warn_identify_compiler_mesg='CC/CXX'
  fi
else
  warn_non_gnu=yes
  warn_non_gnu_mesg='CC/CXX'
fi


#-------------------------------------------------
# Find base CC compiler, outside of spack
#-------------------------------------------------

# Hpclink needs the CC compiler for the fnbounds array, but the spack
# wrapper won't run outside of spack.  So, if we're being built by
# spack, then find the base compiler for hpclink.

HPCLINK_CC="$CC_PATH"

echo "$CC_PATH" | grep -E -i -e 'lib.*spack.*env' >/dev/null
if test $? -eq 0 ; then
  AC_MSG_NOTICE([found spack compiler wrapper])
  AC_MSG_CHECKING([if base compiler works: $SPACK_CC])

  ORIG_CC="$CC"
  CC="$SPACK_CC"
  AC_LANG_PUSH([C])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
unsigned long addrs[] = {
  0x400400, 0x400410
};
unsigned long len = sizeof(addrs) / sizeof(addrs[0]);
]])], [ans=yes], [ans=no])

  AC_MSG_RESULT([$ans])

  AC_LANG_POP()
  CC="$ORIG_CC"

  if test "$ans" = yes ; then HPCLINK_CC="$SPACK_CC"
  else HPCLINK_CC=gcc
  fi
fi

AC_MSG_NOTICE([hpclink compiler: $HPCLINK_CC])

AC_SUBST([HPCLINK_CC])


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

AC_MSG_CHECKING([for CXX flag for C++11])

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

# Find location of libstdc++.  This is used to set LD_LIBRARY_PATH in
# hpcfnbounds.  It is not used by hpcfnbounds2.
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
# CXXFLAGS for extended C++ alignment: -faligned-new
#-------------------------------------------------

# Test if CXX supports '-faligned-new' and add it to CXXFLAGS if
# supported and not already supplied.  This suppresses warnings from
# boost and dyninst:
#   warning: 'new' of type 'SymtabCodeSource' with extended alignment 128

flag='-faligned-new'

AC_MSG_CHECKING([if CXX supports $flag])

AC_LANG_PUSH([C++])
ORIG_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$HOST_CXXFLAGS $ORIG_CXXFLAGS $flag"

AC_COMPILE_IFELSE([
AC_LANG_SOURCE([[
#include <list>
long mylen(std::list <int> lst)
{
  return (long) lst.size();
}
]])], [ans=yes], [ans=no])

AC_MSG_RESULT([$ans])

AC_LANG_POP

CXXFLAGS="$ORIG_CXXFLAGS"

# Add to CXXFLAGS if supported and not already supplied.
if test "$ans" = yes ; then
  case "x$CXXFLAGS" in
    *${flag}* ) ;;
    * ) HOST_CXXFLAGS="$HOST_CXXFLAGS $flag" ;;
  esac
fi


#-------------------------------------------------
# CXXFLAGS for OpenMP
#-------------------------------------------------

# Find a CC/CXX flag for openmp, normally -fopenmp for gcc/g++.
# We could add an ARG_VAR variable for this, but that's probably not
# necessary.

openmp_flag=no

flag_list='-fopenmp -openmp -qopenmp -qsmp=omp'

ORIG_CXXFLAGS="$CXXFLAGS"
AC_LANG_PUSH([C++])

AC_MSG_CHECKING([for CXX flag for openmp])

for flag in $flag_list
do
  CXXFLAGS="$HOST_CXXFLAGS $ORIG_CXXFLAGS $flag"
  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <iostream>
#include <omp.h>
void myprint(int len)
{
  int i;
#pragma omp parallel for
  for (i = 0; i < len; i++) {
    std::cout << i << "\n";
  }
}
]])], [ans=yes], [ans=no])
  if test "$ans" = yes ; then
    openmp_flag="$flag"
    break
  fi
done

AC_MSG_RESULT([$openmp_flag])

AC_LANG_POP

CXXFLAGS="$ORIG_CXXFLAGS"

# If unable to verify an openmp flag, then -fopenmp probably works.
if test "x$openmp_flag" = xno ; then
  AC_MSG_WARN([unable to find flag for openmp, using -fopenmp])
  openmp_flag='-fopenmp'
fi

OPENMP_FLAG="$openmp_flag"

AC_SUBST([OPENMP_FLAG])

#-------------------------------------------------

# Find the openmp library and its directory.  We add the directory to
# the launch script for hpcstruct when using openmp.

openmp_lib_list='libgomp libiomp libomp libxlsmp'

src="conftest.$$.cpp"
obj="conftest.$$.bin"

rm -f "$src" "$obj"

cat <<EOF >"$src"
#include <iostream>
#include <omp.h>
int main(int argc, char **argv)
{
  int i;
#pragma omp parallel for
  for (i = 0; i < argc; i++) {
    std::cout << i << "\n";
  }
}
EOF

openmp_lib_name=no

AC_MSG_CHECKING([for openmp library])

$CXX -g $CXXFLAGS $openmp_flag "$src" -o "$obj" >/dev/null 2>&1

if test $? -eq 0 && test -f "$obj" ; then
  for lib in $openmp_lib_list ; do
    base=`ldd "$obj" 2>/dev/null | $AWK '{print $1}' | grep -e "$lib"`
    if test "x$base" != x ; then
      openmp_lib_name="$base"
      break
    fi
  done
else
  AC_MSG_WARN([unable to compile openmp with $openmp_flag])
fi

AC_MSG_RESULT([$openmp_lib_name])

if test "$openmp_lib_name" = no ; then
  AC_MSG_WARN([unable to find openmp library])
fi

openmp_lib_dir=

if test "$openmp_lib_name" != no ; then
  AC_MSG_CHECKING([for openmp library directory])

  name=`$CXX -print-file-name="$openmp_lib_name" 2>/dev/null`
  if test ! -f "$name" ; then
    name=`ldd "$obj" 2>/dev/null | grep -e "$openmp_lib_name" | $AWK '{print $3}'`
  fi

  if test -f "$name" ; then
    dir=`dirname "$name"`
    if test -d "$dir" ; then
      openmp_lib_dir="$dir"
    fi
  fi

  AC_MSG_RESULT([$openmp_lib_dir])

  if test ! -d "$openmp_lib_dir" ; then
    AC_MSG_WARN([unable to find openmp library directory])
  fi
fi

rm -f "$src" "$obj"


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
# MPICXX compiler for hpcprof-mpi
#-------------------------------------------------

# Search for MPI C++ compiler that is on PATH but not from a module
# and is not a spack wrapper.  This is mainly for a spack build on
# Cray or Blue Gene where mpicxx is not from a module that we can put
# in packages.yaml.
#
# On Cray, we must avoid the spack wrapper.  The Cray MPI C++ compiler
# is CC, but the spack CC wrapper is only a C++ compiler, not MPI.
#
#  $1, $2, ... is a list of compiler names (per platform).
#
# Returns: full path of compiler, or else 'no'.
#
search_for_mpi()
{
  ans=no

  for name in "$@" ; do
    for item in `type -a $name 2>/dev/null` ; do
      case "$item" in
        /* )
          # spack wrapper: /path/to/spack/lib/spack/env/case-insensitive/CC
          echo "$item" | grep -E -i -e 'lib.*spack.*env' >/dev/null
          if test $? -ne 0 ; then
            ans="$item"
            break 2
          fi
          ;;
      esac
    done
  done

  echo "$ans"
}

#-------------------------------------------------

# Find MPICXX, the MPI C++ compiler for hpcprof-mpi.
#
# --enable-mpi-search=(cray, bgq) is mainly for a spack build on Cray
# or Blue Gene that doesn't have a module for MPI.  Outside of spack,
# set MPICXX and use --enable-all-static for Cray and Blue Gene that
# build a static hpcprof-mpi.

AC_ARG_VAR([MPICXX], [MPI C++ compiler for hpcprof-mpi])

AC_MSG_NOTICE([supplied MPICXX compiler: $MPICXX])

AC_ARG_ENABLE([mpi-search],
  [AS_HELP_STRING([--enable-mpi-search=(cray,bgq)],
      [search for MPI C++ compiler on given system (for spack build)])],
  [],
  [enable_mpi_search=no])

case "$enable_mpi_search" in
  cray )
    MPICXX=`search_for_mpi CC`
    case "$MPICXX" in
      /* )
        AC_MSG_NOTICE([Cray MPI wrapper: $MPICXX])
	;;
      * )
        AC_MSG_WARN([unable to find the Cray MPI C++ wrapper (CC) on your PATH])
        AC_MSG_ERROR([check that this system is Cray and CC is the MPI wrapper])
        ;;
    esac
    ;;

  bgq )
    MPICXX=`search_for_mpi mpicxx mpic++`
    case "$MPICXX" in
      /* )
        AC_MSG_NOTICE([Blue Gene MPI wrapper: $MPICXX])
	;;
      * )
        AC_MSG_WARN([unable to find the Blue Gene MPI C++ wrapper (mpicxx) on your PATH])
        AC_MSG_ERROR([check that this system is Blue Gene and mpicxx is the MPI wrapper])
        ;;
    esac
    ;;

  no )
    ;;

  * )
    AC_MSG_WARN([--enable-mpi-search is for Spack builds on Cray and Blue Gene])
    AC_MSG_ERROR([unknown platform: $enable_mpi_search])
    ;;
esac

#-------------------------------------------------

# Test if MPICXX is a valid MPI C++ compiler and report version.

if test "x$MPICXX" != x ; then
  MPICXX_VERSION=`find_version "$MPICXX"`
  MPICXX_PATH=`find_path "$MPICXX"`
  MPICXX="$MPICXX_PATH"

  AC_MSG_NOTICE([MPICXX compiler: $MPICXX_VERSION])
  AC_MSG_NOTICE([MPICXX path: $MPICXX_PATH])

  if test_is_gnu "$MPICXX" ; then : ; else
    AC_MSG_WARN([Not using the GNU MPICXX compiler.])
    warn_non_gnu=yes
    warn_non_gnu_mesg="$warn_non_gnu_mesg MPICXX"
  fi

  AC_MSG_CHECKING([if MPICXX is a valid MPI C++ compiler])

  ORIG_CXX="$CXX"
  CXX="$MPICXX"
  AC_LANG_PUSH([C++])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <mpi.h>
#include <iostream>
int myRank(void)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  std::cout << "yes";
  return rank;
}
]])], [mpicxx_avail=yes], [mpicxx_avail=no])

  AC_MSG_RESULT([$mpicxx_avail])

  if test "$mpicxx_avail" = no ; then
    AC_MSG_ERROR([MPICXX is not a valid compiler: $MPICXX])
  fi

  AC_LANG_POP
  CXX="$ORIG_CXX"
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

prof_mpi_avail=no

if test -n "$MPICXX" ; then
  AC_MSG_CHECKING([if CXX and MPICXX are compatible])

  rm -f "$hello_src" "$hello_o" "$hello_bin" "$hello_out"
  cat <<EOF >"$hello_src"
#include <iostream>
#include <string>
#include <vector>
using namespace std;
int main(int argc, char **argv)
{
  vector <string> foo;
  for (int i = 0; i < argc; i++) {
    foo.push_back(argv[[i]]);
  }
  cout << foo.size();
  return 0;
}
EOF
  $CXX -c $ACTUAL_CXXFLAGS -o "$hello_o" "$hello_src" >/dev/null 2>&1
  $MPICXX -o "$hello_bin" "$hello_o" >"$hello_out" 2>&1

  if test $? -eq 0 && test -f "$hello_bin" ; then
    AC_MSG_RESULT([yes])
    prof_mpi_avail=yes
  else
    AC_MSG_RESULT([no])
    cat <<EOF >&AS_MESSAGE_LOG_FD
------------------------------------------------------------
CXX:     $CXX -c $ACTUAL_CXXFLAGS -o $hello_o $hello_src
MPICXX:  $MPICXX -o $hello_bin $hello_o

EOF
    cat "$hello_src" >&AS_MESSAGE_LOG_FD
    echo >&AS_MESSAGE_LOG_FD
    cat "$hello_out" >&AS_MESSAGE_LOG_FD
    cat <<EOF >&AS_MESSAGE_LOG_FD
------------------------------------------------------------
EOF
    AC_MSG_WARN([CXX:     $CXX_VERSION])
    AC_MSG_WARN([MPICXX:  $MPICXX_VERSION])
    AC_MSG_WARN([MPICXX does not link with CXX (see config.log for details)])
    AC_MSG_ERROR([probably MPICXX does match CXX for compiler family or version])
  fi
  rm -f "$hello_src" "$hello_o" "$hello_bin" "$hello_out"
fi

AC_MSG_NOTICE([build hpcprof-mpi: $prof_mpi_avail])

AM_CONDITIONAL(OPT_ENABLE_MPI, [test "$prof_mpi_avail" = yes])


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

AC_MSG_NOTICE([INFO: post hpc tests: CC='${CC}'  CFLAGS='${CFLAGS}'  HOST_CFLAGS='${HOST_CFLAGS}'])
AC_MSG_NOTICE([INFO: post hpc tests: CXX='${CXX}'  CXXFLAGS='${CXXFLAGS}'  HOST_CXXFLAGS='${HOST_CXXFLAGS}'])
AC_MSG_NOTICE([INFO: post hpc tests: MPICC='${MPICC}'])
AC_MSG_NOTICE([INFO: post hpc tests: MPICXX='${MPICXX}'])
AC_MSG_NOTICE([INFO: post hpc tests: MPIF77='${MPIF77}'])


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


#-------------------------------------------------
# Hash value for launch scripts
#-------------------------------------------------

# Make a hash value to verify that a launch script has the correct
# install directory.  It doesn't have to be secure, just unique.

AC_PATH_PROGS([hash_fcn], [md5sum sha256sum sha384sum sha512sum], [no])

plain_text=`echo $$ ; date ; pwd`

hash_value=`echo $plain_text | $hash_fcn | $AWK '{ print $1 }'`

if test "x$hash_value" = x ; then
  hash_value=`echo $plain_text | tr -d ' '`
fi

hash_value=`echo $hash_value | head -c 30`

AC_MSG_NOTICE([launch script hash: $hash_value])

AC_SUBST([hash_value])


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

AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([], [])])
AC_ARG_WITH([externals],
  [AS_HELP_STRING([--with-externals=PATH],
	  [path to hpctoolkit-externals build or install directory])],
  [],
  [with_externals=no])

if test "${with_externals}" != "no"; then
  ext_file="${with_externals}/externs.conf"
  if test ! -f "$ext_file" ; then
    AC_MSG_ERROR([missing externals file: $ext_file])
  fi
  externals_root=`( cd "$with_externals" && pwd )`
  . "$ext_file"
  use_externals=yes
  AC_MSG_NOTICE([externals version: $ext_version])
  if test "x$ext_version" = x0 ; then
    ext_version=
  fi
else
  use_externals=no
fi

AC_MSG_NOTICE([externals directory: $with_externals])


#-------------------------------------------------
# Option: --with-spack=PATH
#-------------------------------------------------

# If specified, then use PATH as the directory containing the spack
# packages.  This is not spack's install_tree prefix, but rather the
# arch/compiler subdir that contains the spack packages, something
# like:  .../linux-fedora26-x86_64/gcc-7.3.1/
#
# May be absolute or relative path.

AC_ARG_WITH([spack],
  [AS_HELP_STRING([--with-spack=PATH],
      [path to spack arch/compiler install directory])],
  [],
  [with_spack=no])

if test "x$with_spack" != xno ; then
  dir=`(cd "$with_spack" 2>/dev/null && /bin/pwd)`
  if test ! -d "$dir" ; then
    AC_MSG_ERROR([bad spack install directory: $with_spack])
  fi
  with_spack="$dir"
fi

AC_MSG_NOTICE([spack directory: $with_spack])


#-------------------------------------------------
# Default package prefixes from externals and spack.
#-------------------------------------------------

# The order of precedence is:
#  1. --with-package  (highest)
#  2. --with-spack
#  3. --with-externals

default_binutils_prefix=no
default_boost_prefix=no
default_bzip_prefix=no
default_cuda_prefix=no
default_dyninst_prefix=no
default_elfutils_prefix=no
default_gotcha_prefix=no
default_libdwarf_prefix=no
default_libiberty_prefix=no
default_libmonitor_prefix=no
default_libunwind_prefix=no
default_lzma_prefix=no
default_mbedtls_prefix=no
default_papi_prefix=no
default_perfmon_prefix=no
default_tbb_prefix=no
default_xed_prefix=no
default_xerces_prefix=no
default_zlib_prefix=no
default_valgrind_prefix=no

if test "$use_externals" = yes ; then
  default_binutils_prefix="$ext_binutils_prefix"
  default_boost_prefix="$ext_boost_prefix"
  default_dyninst_prefix="$ext_symtabAPI_prefix"
  default_elfutils_prefix="$ext_libelf_prefix"
  default_libdwarf_prefix="$ext_libdwarf_prefix"
  default_libmonitor_prefix="$ext_libmonitor_prefix"
  default_libunwind_prefix="$ext_libunwind_prefix"
  if test "x$ext_lzma_prefix" != x ; then
    default_lzma_prefix="$ext_lzma_prefix"
  fi
  if test "x$ext_perfmon_prefix" != x ; then
    default_perfmon_prefix="$ext_perfmon_prefix"
  fi
  default_xed_prefix="$ext_xed2_prefix"
  default_xerces_prefix="$ext_xerces_prefix"
  default_zlib_prefix="$ext_zlib_prefix"
fi

# Return: success (0) and set 'spack_ans' to install prefix for given
# package name ($1).  Warn if there are multiple dirs with same pkg
# name and return the most recent one.  (Be careful not to spawn a
# subshell.)

mult_spack_pkgs=

find_spack_pkg()
{
  base="$1"
  spack_ans=no
  mult=no

  # allow symlink, if set
  if test -d "${with_spack}/$base" ; then
    spack_ans="${with_spack}/$base"
    return 0
  fi

  for dir in "${with_spack}/${base}"-* ; do
    if test -d "$dir" ; then
      if test "$spack_ans" = no ; then
        spack_ans="$dir"
      else
        mult=yes
        if test "$dir" -nt "$spack_ans" ; then
          spack_ans="$dir"
        fi
      fi
    fi
  done

  if test "$mult" = yes ; then
    AC_MSG_WARN([multiple spack packages for: $base])
    mult_spack_pkgs="$mult_spack_pkgs $base"
  fi

  if test "$spack_ans" != no ; then
    return 0
  fi
  return 1
}

if test "$with_spack" != no
then
  AC_MSG_NOTICE([checking spack packages ...])

  if find_spack_pkg binutils ; then
    default_binutils_prefix="$spack_ans"
  fi
  if find_spack_pkg boost ; then
    default_boost_prefix="$spack_ans"
  fi
  if find_spack_pkg bzip2 ; then
    default_bzip_prefix="$spack_ans"
  fi
  if find_spack_pkg cuda ; then
    default_cuda_prefix="$spack_ans"
  fi
  if find_spack_pkg dyninst ; then
    default_dyninst_prefix="$spack_ans"
  fi
  if find_spack_pkg elfutils ; then
    default_elfutils_prefix="$spack_ans"
  fi
  if find_spack_pkg gotcha ; then
    default_gotcha_prefix="$spack_ans"
  fi
  if find_spack_pkg libdwarf ; then
    default_libdwarf_prefix="$spack_ans"
  fi
  if find_spack_pkg libiberty ; then
    default_libiberty_prefix="$spack_ans"
  fi
  if find_spack_pkg libmonitor ; then
    default_libmonitor_prefix="$spack_ans"
  fi
  if find_spack_pkg libunwind ; then
    default_libunwind_prefix="$spack_ans"
  fi
  if find_spack_pkg xz ; then
    default_lzma_prefix="$spack_ans"
  fi
  if find_spack_pkg mbedtls ; then
    default_mbedtls_prefix="$spack_ans"
  fi
  if find_spack_pkg papi ; then
    default_papi_prefix="$spack_ans"
  fi
  if find_spack_pkg libpfm4 ; then
    default_perfmon_prefix="$spack_ans"
  fi
  if find_spack_pkg intel-tbb ; then
    default_tbb_prefix="$spack_ans"
  fi
  if find_spack_pkg intel-xed ; then
    default_xed_prefix="$spack_ans"
  fi
  if find_spack_pkg xerces-c ; then
    default_xerces_prefix="$spack_ans"
  fi
  if find_spack_pkg zlib ; then
    default_zlib_prefix="$spack_ans"
  fi
  if find_spack_pkg valgrind ; then
    default_valgrind_prefix="$spack_ans"
  fi
fi


#-------------------------------------------------
# Check consistency of C, C++ and wordsize
#-------------------------------------------------

# If using --with-spack, try to test if the spack compiler and the
# toolkit compiler are both GNU and the same rev.  This is mostly
# heuristic, we may throw false positives.  For example:
#
# spack path:  /home/krentel/Spack/install/linux-fedora26-x86_64/gcc-7.3.1
# CC version:  gcc (GCC) 7.3.1 20180130 (Red Hat 7.3.1-2)

have_spack_version=no

# parse spack version from path: .../gcc-7.3.1
case "$with_spack" in
  /* )
    base=`basename "$with_spack"`
    AC_MSG_NOTICE([checking that CC matches the spack compiler: $base])

    case "$base" in
      gcc-* )
        ver=`echo $base | sed -e 's/gcc-//'`
        spack_major=`echo $ver | awk -F. '{print $1}'`
        spack_minor=`echo $ver | awk -F. '{print $2}'`

        if test "x$spack_minor" != x ; then
	  have_spack_version=yes
	else
          AC_MSG_WARN([unable to identify spack compiler version: $with_spack])
	  warn_identify_compiler=yes
	  warn_identify_compiler_mesg="$warn_identify_compiler_mesg spack"
        fi
	;;
      * )
        AC_MSG_WARN([not using the GNU compiler for spack prereqs: $with_spack])
        warn_non_gnu=yes
        warn_non_gnu_mesg="$warn_non_gnu_mesg spack"
	;;
    esac
    ;;
esac

warn_compiler_mismatch=no

if test "$have_spack_version" = yes && test "$have_cc_version" = yes  \
   && test "x${spack_major}-${spack_minor}" != "x${cc_major}-${cc_minor}"
then
  AC_MSG_WARN([the CC and spack compilers are not the same version])
  warn_compiler_mismatch=yes
fi

#-------------------------------------------------

# Continue to set multilib_path for package tests below, but we really
# only support 64-bit now.

host_wordsize=unknown
wordsize_cflag=
case "$host_sizeof_voidp" in
  4 ) host_wordsize=32 ; wordsize_cflag='-m32' ;;
  8 ) host_wordsize=64 ; wordsize_cflag='-m64' ;;
  * ) AC_MSG_WARN([unable to determine host wordsize]) ;;
esac

multilib_path=lib
case "$host_wordsize" in
  32 ) multilib_path='lib32 lib' ;;
  64 ) multilib_path='lib64 lib' ;;
esac


#-------------------------------------------------
# Option: --with-binutils=PATH
#-------------------------------------------------

num_pkgs=0

# Binutils uses four main libraries: libbfd.a, libopcodes.a,
# libiberty.a and libz.a (plus -ldl if bfd is built with plugins).
# The first two must be in the binutils package, but the last two may
# be in binutils or separate (libiberty and zlib).
#
# Note: we no longer use binutils to crack instructions, so we don't
# need libopcodes anymore, not on any platform.

BINUTILS="$default_binutils_prefix"

AC_ARG_WITH([binutils],
  [AS_HELP_STRING([--with-binutils=PATH],
      [path to binutils install directory])],
  [BINUTILS="$withval"],
  [])

BINUTILS_IFLAGS=
BINUTILS_LIBS=
binutils_has_libiberty=no
binutils_has_libz=no

AC_MSG_CHECKING([for binutils])

case "$BINUTILS" in
  /* )
    # bfd.h must exist
    if test -f "${BINUTILS}/include/bfd.h" ; then
      BINUTILS_IFLAGS="-I${BINUTILS}/include"
    else
      AC_MSG_ERROR([unable to find bfd.h in: $BINUTILS])
    fi

    # libbfd.a must be in binutils
    for lib in $multilib_path fail ; do
      file="${BINUTILS}/${lib}/libbfd.a"
      if test -f "$file" ; then
        BINUTILS_LIBS="${BINUTILS_LIBS} $file"
	break
      elif test "$lib" = fail ; then
        AC_MSG_ERROR([unable to find libbfd.a in: $BINUTILS])
      fi
    done

    # libiberty.a may be here or may be in libiberty
    for lib in $multilib_path ; do
      file="${BINUTILS}/${lib}/libiberty.a"
      if test -f "$file" ; then
        BINUTILS_LIBS="${BINUTILS_LIBS} $file"
	binutils_has_libiberty=yes
	break
      fi
    done

    # libz.a may be here or may be in zlib
    for lib in $multilib_path ; do
      file="${BINUTILS}/${lib}/libz.a"
      if test -f "$file" ; then
        BINUTILS_LIBS="${BINUTILS_LIBS} $file"
	binutils_has_libz=yes
	break
      fi
    done
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([binutils directory must be absolute path])
    ;;
esac

AC_MSG_RESULT([$BINUTILS])

binlibs=
for lib in $BINUTILS_LIBS ; do
  binlibs="$binlibs `basename $lib`"
done

AC_MSG_NOTICE([binutils libs: $binlibs])

# FIXME: libbfd requires -ldl unless plugins are disabled (default is
# enabled).  But we could test for this.

BINUTILS_LIBS="${BINUTILS_LIBS} -ldl"

AC_SUBST([BINUTILS_IFLAGS])
AC_SUBST([BINUTILS_LIBS])

#-------------------------------------------------

# Test for binutils 2.34 or later.  The signature of
# bfd_section_vma(), etc dropped the first argument.
# up to 2.33.1:  #define bfd_section_vma(bfd, ptr) ((ptr)->vma)
# after 2.34:    bfd_section_vma (const asection *sec)

ORIG_CFLAGS="$CFLAGS"
CFLAGS="$HOST_CFLAGS  $BINUTILS_IFLAGS"

AC_LANG_PUSH([C])

AC_MSG_CHECKING([if binutils version 2.34 or later])

AC_COMPILE_IFELSE([
AC_LANG_SOURCE([[
#define PACKAGE "hpctoolkit"
#include <bfd.h>
unsigned long
get_vma(asection * asec)
{
    return bfd_section_vma(asec);
}
]])], [binutils_234=yes], [binutils_234=no])

AC_MSG_RESULT([$binutils_234])

AC_LANG_POP

CFLAGS="$ORIG_CFLAGS"

if test "$binutils_234" = yes ; then
  AC_DEFINE(BINUTILS_234, 1, [binutils version 2.34 or later])
fi


#-------------------------------------------------
# Option: --with-boost=PATH
#-------------------------------------------------

BOOST="$default_boost_prefix"

AC_ARG_WITH([boost],
  [AS_HELP_STRING([--with-boost=PATH],
      [path to boost install directory])],
  [BOOST="$withval"],
  [])

BOOST_IFLAGS=
BOOST_LFLAGS=
BOOST_LIB_DIR=
BOOST_COPY=no

# The copy list should include all libraries that any version of
# dyninst would use (but no more).  But copy-libs won't fail on a
# missing library, so it doesn't have to be exact.

BOOST_COPY_LIST='atomic chrono date_time filesystem graph regex system thread timer'
BOOST_NEEDED_LIST='graph regex system'

AC_MSG_CHECKING([for boost])

case "$BOOST" in
  /* )
    if test ! -f "${BOOST}/include/boost/shared_ptr.hpp" ; then
      AC_MSG_ERROR([invalid boost directory: $BOOST])
    fi
    BOOST_IFLAGS="-I${BOOST}/include"

    for lib in $multilib_path ; do
      dir="${BOOST}/$lib"
      for f in "$dir"/libboost*system*.so* ; do
        if test -f "$f" ; then
	  BOOST_LIB_DIR="$dir"
	  BOOST_COPY=yes
	  break 2
	fi
      done
    done
    if test "$BOOST_COPY" != yes ; then
      AC_MSG_ERROR([invalid boost directory: $BOOST])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([boost directory must be absolute path: $BOOST])
    ;;
esac

AC_MSG_RESULT([$BOOST])

if test "x$BOOST_LIB_DIR" != x ; then
  boost_lib_list=
  boost_lflag_list=

  for lib in $BOOST_NEEDED_LIST ; do
    if test -f "${BOOST_LIB_DIR}/libboost_${lib}.so" ; then
      boost_lflag_list="${boost_lflag_list} -lboost_$lib"
    else
      AC_MSG_ERROR([boost ${lib} is required])
    fi
  done

  for lib in $BOOST_COPY_LIST ; do
    if test -f "${BOOST_LIB_DIR}/libboost_${lib}.so" ; then
      boost_lib_list="${boost_lib_list} $lib"
    fi
  done

  BOOST_LFLAGS="-L${BOOST_LIB_DIR} $boost_lflag_list"

  AC_MSG_NOTICE([boost libs: $boost_lib_list])
fi

AC_SUBST([BOOST_IFLAGS])
AC_SUBST([BOOST_LFLAGS])
AC_SUBST([BOOST_LIB_DIR])
AC_SUBST([BOOST_COPY])
AC_SUBST([BOOST_COPY_LIST])


#-------------------------------------------------
# Option: --with-bzip=PATH
#-------------------------------------------------

# Location of libbz2.so.  This is just for copy-libs as an elfutils
# prereq.  Toolkit doesn't directly call bzip, so we don't need an
# include path here.  Note: this package is not built in externals.

BZIP_COPY=no
BZIP_LIB=no

BZIP="$default_bzip_prefix"

AC_ARG_WITH([bzip],
  [AS_HELP_STRING([--with-bzip=PATH],
      [path to bzip2 install directory])],
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
    num_pkgs=`expr 1 + $num_pkgs`
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
# Option: --with-dyninst=PATH
#-------------------------------------------------

# Now require full ParseAPI with CFG loop support or nothing.

DYNINST="$default_dyninst_prefix"

AC_ARG_WITH([dyninst],
  [AS_HELP_STRING([--with-dyninst=PATH],
      [path to dyninst install directory])],
  [DYNINST="$withval"],
  [])

DYNINST_IFLAGS=
DYNINST_LFLAGS=
DYNINST_LIB_DIR=
DYNINST_COPY=no
dyninst_features=

AC_MSG_CHECKING([for dyninst])

case "${DYNINST}" in
  /* )
    if test -f "${DYNINST}/include/Symtab.h"  \
       && test -f "${DYNINST}/include/CFG.h"
    then : ; else
      AC_MSG_ERROR([invalid dyninst directory: $DYNINST])
    fi
    DYNINST_IFLAGS="-I${DYNINST}/include"

    for lib in $multilib_path ; do
      dir="${DYNINST}/$lib"
      if test -f "${dir}/libparseAPI.so" ; then
        DYNINST_LIB_DIR="$dir"
	DYNINST_COPY=yes
	break
      fi
    done
    if test "$DYNINST_COPY" != yes ; then
      AC_MSG_ERROR([invalid dyninst directory: $DYNINST])
    fi
    DYNINST_LFLAGS="-L${DYNINST_LIB_DIR} -lparseAPI -linstructionAPI -lsymtabAPI"
    DYNINST_LFLAGS="${DYNINST_LFLAGS} -ldynDwarf -ldynElf -lcommon"
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([dyninst directory must be absolute path: $DYNINST])
    ;;
esac

AC_MSG_RESULT([$DYNINST])

AC_SUBST([DYNINST_IFLAGS])
AC_SUBST([DYNINST_LFLAGS])
AC_SUBST([DYNINST_LIB_DIR])
AC_SUBST([DYNINST_COPY])


#-------------------------------------------------
# Option: --with-elfutils=PATH
#-------------------------------------------------

LIBELF="$default_elfutils_prefix"

AC_ARG_WITH([elfutils],
  [AS_HELP_STRING([--with-elfutils=PATH],
      [path to elfutils install directory])],
  [LIBELF="$withval"],
  [])

AC_MSG_CHECKING([for elfutils])

LIBELF_INC=no
LIBELF_LIB=no
LIBELF_COPY=no
elf_has_libdw=no

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

    if test -f "${LIBELF_LIB}/libdw.so" ; then
      elf_has_libdw=yes
    fi
    num_pkgs=`expr 1 + $num_pkgs`
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
# Option: --with-gotcha=PATH
#-------------------------------------------------

# NOTE: currently not used, but there's a non-trivial chance that it
# may come back.

GOTCHA="$default_gotcha_prefix"

GOTCHA_IFLAGS=
GOTCHA_LDFLAGS=
GOTCHA_LIBDIR=no

AC_ARG_WITH([gotcha],
  [AS_HELP_STRING([--with-gotcha=PATH],
      [path to gotcha install directory (ignored)])],
  [GOTCHA="$withval"],
  [])

AC_MSG_NOTICE([skipping gotcha ...])

if false ; then

AC_MSG_CHECKING([for gotcha])

case "$GOTCHA" in
  /* )
    if test ! -f "${GOTCHA}/include/gotcha/gotcha.h" ; then
      AC_MSG_ERROR([unable to find gotcha.h in: $GOTCHA])
    fi
    GOTCHA_IFLAGS="-I${GOTCHA}/include"
    for lib in $multilib_path ; do
      if test -f "${GOTCHA}/${lib}/libgotcha.so" ; then
        GOTCHA_LIBDIR="${GOTCHA}/${lib}"
	break
      fi
    done
    if test "$GOTCHA_LIBDIR" = no ; then
      AC_MSG_ERROR([unable to find libgotcha.so in: $GOTCHA])
    fi
    GOTCHA_LDFLAGS="-L${GOTCHA_LIBDIR} -lgotcha"
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([gotcha directory must be absolute path: $GOTCHA])
    ;;
esac

AC_MSG_RESULT([$GOTCHA])

#
# Test for gotcha_filter_libraries_by_name(), in gotcha >= 1.0.3.
#
case "$GOTCHA" in
  /* )
    AC_MSG_CHECKING([if gotcha supports gotcha_filter_libraries_by_name])

    ORIG_CFLAGS="$CFLAGS"
    ORIG_LIBS="$LIBS"
    CFLAGS="$CFLAGS $GOTCHA_IFLAGS"
    LIBS="$GOTCHA_LDFLAGS"
    AC_LANG_PUSH([C])

    AC_LINK_IFELSE([
    AC_LANG_SOURCE([[
#include <gotcha/gotcha.h>
int main(int argc, char **argv)
{
  gotcha_filter_libraries_by_name(argv[0]);
  return 0;
}
]])], [gotcha_has_filter=yes], [gotcha_has_filter=no])

    AC_LANG_POP
    CFLAGS="$ORIG_CFLAGS"
    LIBS="$ORIG_LIBS"

    AC_MSG_RESULT([$gotcha_has_filter])

    if test "x$gotcha_has_filter" != xyes ; then
      AC_MSG_ERROR([hpctoolkit needs gotcha 1.0.3 or later])
    fi
    ;;
esac

fi  # skip test for gotcha

AC_SUBST([GOTCHA_IFLAGS])
AC_SUBST([GOTCHA_LDFLAGS])
AC_SUBST([GOTCHA_LIBDIR])


#-------------------------------------------------
# Option: --with-libdwarf=PATH
#-------------------------------------------------

LIBDWARF="$default_libdwarf_prefix"

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
    num_pkgs=`expr 1 + $num_pkgs`
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
# Option: --with-libiberty=PATH
#-------------------------------------------------

# This is only used if binutils doesn't have libiberty.a.

LIBIBERTY="$default_libiberty_prefix"

AC_ARG_WITH([libiberty],
  [AS_HELP_STRING([--with-libiberty=PATH],
      [path to libiberty install directory (only needed if binutils
       does not contain libiberty.a)])],
  [LIBIBERTY="$withval"],
  [])

if test "$BINUTILS" = no || test "$binutils_has_libiberty" = yes
then
  AC_MSG_NOTICE([skipping libiberty for binutils])
  LIBIBERTY=no
else
  AC_MSG_CHECKING([for libiberty.a for binutils])

  found_libiberty=no
  case "$LIBIBERTY" in
    /* )
      for lib in $multilib_path ; do
        file="${LIBIBERTY}/${lib}/libiberty.a"
	if test -f "$file" ; then
          BINUTILS_LIBS="${BINUTILS_LIBS} $file"
	  found_libiberty=yes
	  break
	fi
      done
      num_pkgs=`expr 1 + $num_pkgs`
      ;;
  esac

  AC_MSG_RESULT([$found_libiberty])

  if test "$found_libiberty" != yes ; then
    AC_MSG_ERROR([unable to find libiberty.a in binutils or libiberty])
  fi
fi


#-------------------------------------------------
# Option: --with-libmonitor=PATH
#-------------------------------------------------

OPT_LIBMONITOR_DYNAMIC=no
OPT_LIBMONITOR_STATIC=no

LIBMONITOR="$default_libmonitor_prefix"

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
    num_pkgs=`expr 1 + $num_pkgs`
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
# Option: --with-libunwind=PATH
#-------------------------------------------------
#
# Now required on all platforms for hpcrun.
# Test for three things:
#
#  1. libunwind exists (headers + .so) for dynamic case.
#  2. static archive libunwind.a exists for static case.
#  3. archive compiled with -fPIC for link into libhpcrun.so.
#
# Note: we only use local (first-party) unwinding, so we only need
# libunwind.so, not libunwind-PLATFORM.so, but code must be compiled
# with UNW_LOCAL_ONLY.
#

LIBUNWIND="$default_libunwind_prefix"

LIBUNWIND_IFLAGS=
LIBUNWIND_LDFLAGS_DYN=
LIBUNWIND_LDFLAGS_STAT=
LIBUNWIND_LIB_DIR=

libunwind_exists=no
libunwind_dynamic=no
libunwind_static=no
libunwind_fpic=no

AC_ARG_WITH([libunwind],
  [AS_HELP_STRING([--with-libunwind=PATH],
      [path to libunwind install directory])],
  [LIBUNWIND="$withval"],
  [])

AC_MSG_CHECKING([for libunwind])

#
# Test if libunwind.h, libunwind.so and libunwind.a exist.
#
case "$LIBUNWIND" in
  /* )
    if test ! -f "${LIBUNWIND}/include/libunwind.h" ; then
      AC_MSG_ERROR([invalid libunwind directory: $LIBUNWIND])
    fi
    LIBUNWIND_IFLAGS="-I${LIBUNWIND}/include"

    for lib in $multilib_path ; do
      ldir="${LIBUNWIND}/$lib"
      if test -f "${ldir}/libunwind.so" ; then
        LIBUNWIND_LDFLAGS_DYN="-L${ldir} -lunwind"
	LIBUNWIND_LIB_DIR="$ldir"
	libunwind_exists=yes
	libunwind_dynamic=yes
      fi
      if test -f "${ldir}/libunwind.a" ; then
        LIBUNWIND_LDFLAGS_STAT="${ldir}/libunwind.a"
	LIBUNWIND_LIB_DIR="$ldir"
	libunwind_exists=yes
	libunwind_static=yes
      fi
      if test "$libunwind_exists" = yes ; then
        break
      fi
    done
    if test "$libunwind_exists" != yes ; then
      AC_MSG_ERROR([invalid libunwind directory: $LIBUNWIND])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([libunwind directory must be absolute path: $LIBUNWIND])
    ;;
esac

AC_MSG_RESULT([$LIBUNWIND])
AC_MSG_NOTICE([libunwind.a static archive: $libunwind_static])

#
# Test if libunwind.a was compiled with -fPIC.
# If yes, then use for both static and dynamic.
#
if test "x$libunwind_static" = xyes
then
  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="-shared -fPIC $LIBUNWIND_IFLAGS"
  LIBS="$LIBUNWIND_LDFLAGS_STAT"

  AC_LANG_PUSH([C])
  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#define UNW_LOCAL_ONLY
#include <sys/types.h>
#include <libunwind.h>
unw_word_t
my_get_ip_addr(unw_context_t *context)
{
  unw_cursor_t cursor;
  unw_word_t ip;
  unw_init_local(&cursor, context);
  unw_step(&cursor);
  unw_get_reg(&cursor, UNW_REG_IP, &ip);
  return ip;
}
]])], [libunwind_fpic=yes], [libunwind_fpic=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"
fi

AC_MSG_NOTICE([libunwind.a compiled with -fPIC: $libunwind_fpic])

#
# Now require libunwind static archive with fpic for both static and
# dynamic cases, or else not at all, same as mbedtls.  (The nothing
# case is for just building documentation.)  And never need to copy.
#
if test "$libunwind_exists" = yes && test "$libunwind_fpic" = no ; then
  AC_MSG_ERROR([libunwind.a must be compiled with -fPIC])
fi

unwinder_mesg="$UNWINDER"
if test "$UNWINDER" != libunw && test "$libunwind_exists" = yes ; then
  unwinder_mesg="$unwinder_mesg + libunw"
fi

# fixme: only used in hpcrun/main.c because we don't have a libunwind
# version of hpcrun_dump_intervals().
if test "$UNWINDER" != "libunw"; then
  AC_DEFINE([HPCRUN_HAVE_CUSTOM_UNWINDER], 1,
            [have custom unwinder for this platform])
fi

AC_SUBST(LIBUNWIND_IFLAGS)
AC_SUBST(LIBUNWIND_LDFLAGS_DYN)
AC_SUBST(LIBUNWIND_LDFLAGS_STAT)
AC_SUBST(LIBUNWIND_LIB_DIR)

#
# set up automake choice of unwinder
#
AM_CONDITIONAL([UNW_X86], [test "$UNWINDER" = x86-family])
AM_CONDITIONAL([UNW_PPC64], [test "$UNWINDER" = ppc64])
AM_CONDITIONAL([UNW_LIBUNW], [test "$UNWINDER" = libunw])

AM_CONDITIONAL(OPT_LIBUNWIND_STATIC, [test "$libunwind_static" = yes])


#-------------------------------------------------
# Option: --with-lzma=PATH
#-------------------------------------------------
#
# Test for three things:
#
#  1. liblzma exists (headers + .so) for dynamic case.
#  2. static archive liblzma.a exists for static case.
#  3. archive compiled with -fPIC for link into libhpcrun.so.
#
# Note: lzma is also used in prof-mpi (via support-lean), not just in
# hpcrun, so we also need "normal" usage, not just fPIC archive for
# hiding.
#

LZMA="$default_lzma_prefix"

LZMA_IFLAGS=
LZMA_LDFLAGS_DYN=
LZMA_LDFLAGS_STAT=
LZMA_PROF_MPI_LIBS=

LZMA_COPY=no
LZMA_LIB_DIR=

lzma_exists=no
lzma_dynamic=no
lzma_static=no
lzma_fpic=no

AC_ARG_WITH([lzma],
  [AS_HELP_STRING([--with-lzma=PATH],
      [path to lzma (xz utils) install directory])],
  [LZMA="$withval"],
  [])

AC_MSG_CHECKING([for lzma])

#
# Test if lzma.h, liblzma.so and/or liblzma.a exist.
#
case "$LZMA" in
  /* )
    if test ! -f "${LZMA}/include/lzma.h" ; then
      AC_MSG_ERROR([invalid lzma directory: $LZMA])
    fi
    LZMA_IFLAGS="-I${LZMA}/include"

    for lib in $multilib_path ; do
      ldir="${LZMA}/$lib"
      if test -f "${ldir}/liblzma.so" ; then
        LZMA_LDFLAGS_DYN="-L${ldir} -llzma"
	LZMA_COPY=yes
	LZMA_LIB_DIR="$ldir"
        lzma_exists=yes
	lzma_dynamic=yes
      fi
      if test -f "${ldir}/liblzma.a" ; then
        LZMA_LDFLAGS_STAT="${ldir}/liblzma.a"
	LZMA_LIB_DIR="$ldir"
        lzma_exists=yes
	lzma_static=yes
      fi
      if test "$lzma_exists" = yes ; then
        break
      fi
    done
    if test "$lzma_exists" != yes ; then
      AC_MSG_ERROR([invalid lzma directory: $LZMA])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([lzma directory must be absolute path: $LZMA])
    ;;
esac

AC_MSG_RESULT([$LZMA])
AC_MSG_NOTICE([lzma.a static archive: $lzma_static])

#
# Test if liblzma.a was compiled with -fPIC.
# If yes, then use for both static and dynamic.
#
if test "$lzma_static" = yes
then
  ORIG_CFLAGS="$CFLAGS"
  ORIG_LIBS="$LIBS"
  CFLAGS="-shared -fPIC $LZMA_IFLAGS"
  LIBS="$LZMA_LDFLAGS_STAT"

  AC_LANG_PUSH([C])
  AC_LINK_IFELSE([
  AC_LANG_SOURCE([[
#include <sys/types.h>
#include <inttypes.h>
#include <lzma.h>
long
myencode(uint8_t *in_buf, uint8_t *out_buf)
{
  lzma_stream stream = LZMA_STREAM_INIT;
  stream.next_in = in_buf;
  stream.next_out = out_buf;
  lzma_code(&stream, LZMA_FINISH);
  return (long) stream.avail_out;
}
]])], [lzma_fpic=yes], [lzma_fpic=no])

  AC_LANG_POP
  CFLAGS="$ORIG_CFLAGS"
  LIBS="$ORIG_LIBS"
fi

AC_MSG_NOTICE([lzma.a compiled with -fPIC: $lzma_fpic])

#
# Now require lzma static archive with fpic, or else nothing at all
# (for docs), same as libunwind and mbedtls.
#
if test "$lzma_exists" = yes && test "$lzma_fpic" = no ; then
  AC_MSG_ERROR([liblzma.a must be compiled with -fPIC])
fi

#
# hpcprof-mpi: same as hpclink if fully static, else default
#
echo "x$HPCPROFMPI_LT_LDFLAGS" | grep -e all-static >/dev/null 2>&1
if test $? = 0 ; then
  LZMA_PROF_MPI_LIBS="$LZMA_LDFLAGS_DYN"
else
  LZMA_PROF_MPI_LIBS="$LZMA_LDFLAGS_STAT"
fi

AC_SUBST([LZMA_IFLAGS])
AC_SUBST([LZMA_LDFLAGS_DYN])
AC_SUBST([LZMA_LDFLAGS_STAT])
AC_SUBST([LZMA_PROF_MPI_LIBS])
AC_SUBST([LZMA_COPY])
AC_SUBST([LZMA_LIB_DIR])

AM_CONDITIONAL(OPT_LZMA_STATIC, [test "$lzma_static" = yes])


#-------------------------------------------------
# Option: --with-mbedtls=PATH
#-------------------------------------------------

# mbedtls is a lightweight library that includes libmbedcrypto.a for
# computing md5 and SHA sums for buffers in memory.
#
# Note: we require libmbedcrypto.a to be compiled with -fPIC.

MBEDTLS="$default_mbedtls_prefix"
MBEDTLS_IFLAGS=
MBEDTLS_LIBS=
MBEDTLS_LIB=

AC_ARG_WITH([mbedtls],
  [AS_HELP_STRING([--with-mbedtls=PATH],
      [path to mbedtls install directory])],
  [MBEDTLS="$withval"],
  [])

AC_MSG_CHECKING([for mbedtls])

case "$MBEDTLS" in
  /* )
    if test ! -f "${MBEDTLS}/include/mbedtls/md5.h" ; then
      AC_MSG_ERROR([unable to find md5.h in: $MBEDTLS])
    fi
    MBEDTLS_IFLAGS="-I ${MBEDTLS}/include"
    for lib in $multilib_path ; do
      if test -f "${MBEDTLS}/lib/libmbedcrypto.a" ; then
	MBEDTLS_LIB="${MBEDTLS}/lib"
        MBEDTLS_LIBS="${MBEDTLS}/lib/libmbedcrypto.a"
	break
      fi
    done
    if test "x$MBEDTLS_LIBS" = x ; then
      AC_MSG_ERROR([unable to find libmbedcrypto.a in: $MBEDTLS])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([mbedtls directory must be absolute path: $MBEDTLS])
    ;;
esac

AC_MSG_RESULT([$MBEDTLS])

#
# Test if libmbedcrypto.a was compiled with -fPIC
#
case "$MBEDTLS" in
  /* )
    AC_MSG_CHECKING([if libmbedcrypto.a was compiled with -fPIC])

    ORIG_CFLAGS="$CFLAGS"
    ORIG_LIBS="$LIBS"
    CFLAGS="-shared -fPIC $MBEDTLS_IFLAGS"
    LIBS="$MBEDTLS_LIBS"

    AC_LANG_PUSH([C])
    AC_LINK_IFELSE([
    AC_LANG_SOURCE([[
#include <sys/types.h>
#include "mbedtls/md5.h"
void
my_md5_hash(char *buf, size_t len)
{
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_update(&ctx, buf, len);
}
]])], [mbed_fpic=yes], [mbed_fpic=no])

    AC_MSG_RESULT([$mbed_fpic])
    AC_LANG_POP
    CFLAGS="$ORIG_CFLAGS"
    LIBS="$ORIG_LIBS"

    if test "$mbed_fpic" != yes ; then
      AC_MSG_ERROR([libmbedcrypto.a must be compiled with -fPIC])
    fi
    ;;
esac

AC_SUBST([MBEDTLS_LIB])
AC_SUBST([MBEDTLS_IFLAGS])
AC_SUBST([MBEDTLS_LIBS])


#-------------------------------------------------
# Option: --with-papi=PATH
#-------------------------------------------------

OPT_PAPI_DYNAMIC=no
OPT_PAPI_STATIC=no
OPT_PAPI_IFLAGS=
OPT_PAPI_LDFLAGS=
OPT_PAPI_LIBS_STAT=
OPT_PAPI_LIBPATH=

OPT_PAPI="$default_papi_prefix"

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
	if test -f "${lib_dir}/libpapi.so" ; then
          OPT_PAPI_DYNAMIC=yes
	  OPT_PAPI_LDFLAGS="-L${lib_dir} -lpapi"
	  if test -f "${lib_dir}/libpfm.so" ; then
	    OPT_PAPI_LDFLAGS="${OPT_PAPI_LDFLAGS} -lpfm"
	  fi
        fi
        if test -f "${lib_dir}/libpapi.a" ; then
          OPT_PAPI_STATIC=yes
	  OPT_PAPI_LIBS_STAT="${lib_dir}/libpapi.a"
	  if test -f "${lib_dir}/libpfm.a" ; then
	    OPT_PAPI_LIBS_STAT="${OPT_PAPI_LIBS_STAT} ${lib_dir}/libpfm.a"
	  fi
	fi
	break
      fi
    done

    if test "x$OPT_PAPI_LIBPATH" = x ; then
      AC_MSG_ERROR([unable to find libpapi in: $OPT_PAPI])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
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

AC_SUBST(OPT_PAPI)
AC_SUBST(OPT_PAPI_IFLAGS)
AC_SUBST(OPT_PAPI_LDFLAGS)
AC_SUBST(OPT_PAPI_LIBS_STAT)
AC_SUBST(OPT_PAPI_LIBPATH)


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

elif test "$default_perfmon_prefix" != no  \
     && valid_perfmon_dir "$default_perfmon_prefix"
then
  # if value from externals/spack works, use that
  PERFMON="$default_perfmon_prefix"
  perfmon_mesg="$default_perfmon_prefix"

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
    num_pkgs=`expr 1 + $num_pkgs`
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
#include <linux/version.h>

static struct perf_event_attr attr;
void check_kernel()
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
  attr.context_switch = 1;
#else
#error "kernel is older than 4.3"
#endif
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
# Option: --with-tbb=PATH
#-------------------------------------------------

# Dyninst 10.x with parallel parsing uses Intel TBB, but older
# versions do not, so this must be optional.  Also, we selectively add
# proxy malloc to hpcstruct, so keep that separate.

TBB="$default_tbb_prefix"

AC_ARG_WITH([tbb],
  [AS_HELP_STRING([--with-tbb=PATH],
      [path to intel tbb install directory])],
  [TBB="$withval"],
  [])

TBB_IFLAGS=
TBB_LFLAGS=
TBB_PROXY_LIB=
TBB_LIB_DIR=
TBB_COPY=no

AC_MSG_CHECKING([for tbb])

case "$TBB" in
  /* )
    if test ! -f "${TBB}/include/tbb/concurrent_hash_map.h" ; then
      AC_MSG_ERROR([invalid tbb directory: $TBB])
    fi
    TBB_IFLAGS="-I${TBB}/include"

    for lib in $multilib_path ; do
      dir="${TBB}/$lib"
      for f in "$dir"/libtbb*.so* ; do
        if test -f "$f" ; then
	  TBB_LIB_DIR="$dir"
	  TBB_COPY=yes
	  break 2
	fi
      done
    done
    if test "$TBB_COPY" != yes ; then
      AC_MSG_ERROR([invalid tbb directory: $TBB])
    fi
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([tbb directory must be absolute path: $TBB])
    ;;
esac

AC_MSG_RESULT([$TBB])

if test "x$TBB_LIB_DIR" != x ; then
  tbb_lib_list=

  for lib in tbb tbbmalloc ; do
    if test -f "${TBB_LIB_DIR}/lib${lib}.so" ; then
      tbb_lib_list="${tbb_lib_list} -l$lib"
    fi
  done

  TBB_LFLAGS="-L${TBB_LIB_DIR} $tbb_lib_list"

  # Keep proxy malloc separate.
  if test -f "${TBB_LIB_DIR}/libtbbmalloc_proxy.so" ; then
    TBB_PROXY_LIB='-ltbbmalloc_proxy'
  fi

  AC_MSG_NOTICE([tbb libs: $tbb_lib_list $TBB_PROXY_LIB])
fi

AC_SUBST([TBB_IFLAGS])
AC_SUBST([TBB_LFLAGS])
AC_SUBST([TBB_PROXY_LIB])
AC_SUBST([TBB_LIB_DIR])
AC_SUBST([TBB_COPY])


#-------------------------------------------------
# Option: --with-xed=PATH
#-------------------------------------------------

# Now use static and shared libxed.  hpcrun requires .so or .a + fPIC,
# hpclink requires .a, and fnbounds, etc can use either.  hpcprof-mpi
# requires .a, but only if fully static (Cray).
#
# We support both old XED (2013) with single .a + fPIC and new (2015)
# XED with separate .so and .a without fPIC, so we test if .a can link
# into a shared library.

XED2_INC=
XED2_LIB_DIR=no
XED2_LIB_FLAGS=
XED2_HPCRUN_LIBS=
XED2_HPCLINK_LIBS=
XED2_PROF_MPI_LIBS=
XED2_COPY=no
no_xed_for_hpcrun=no
no_xed_for_hpclink=no

XED2="$default_xed_prefix"

AC_ARG_WITH([xed],
  [AS_HELP_STRING([--with-xed=PATH],
  	    [path to intel xed install directory])],
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
    num_pkgs=`expr 1 + $num_pkgs`
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
  echo "x$HPCPROFMPI_LT_LDFLAGS" | grep -e all-static >/dev/null 2>&1
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

XERCES="$default_xerces_prefix"

AC_MSG_CHECKING([for XercesC])

AC_ARG_WITH([xerces],
  [AS_HELP_STRING([--with-xerces=PATH],
                  [path to xerces-c install directory])],
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
  num_pkgs=`expr 1 + $num_pkgs`
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

# Zlib is used both independently and for binutils.

ZLIB="$default_zlib_prefix"

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
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([zlib directory must be absolute path: $ZLIB])
    ;;
esac

AC_MSG_RESULT([$ZLIB])

# If binutils doesn't include libz.a, then try here.
if test "$BINUTILS" = no || test "$binutils_has_libz" = yes
then
  AC_MSG_NOTICE([skipping libz for binutils])
else
  AC_MSG_CHECKING([for libz.a for binutils])

  found_libz=no
  case "$ZLIB" in
    /* )
      for lib in $multilib_path ; do
        file="${ZLIB}/${lib}/libz.a"
	if test -f "$file" ; then
          BINUTILS_LIBS="${BINUTILS_LIBS} $file"
	  found_libz=yes
	  break
	fi
      done
      ;;
  esac

  AC_MSG_RESULT([$found_libz])

  if test "$found_libz" != yes ; then
    AC_MSG_ERROR([unable to find libz.a in binutils or zlib])
  fi
fi

AM_CONDITIONAL([OPT_USE_ZLIB], [test "$ZLIB" != no])

AC_SUBST([ZLIB_INC])
AC_SUBST([ZLIB_LIB])
AC_SUBST([ZLIB_HPCLINK_LIB])
AC_SUBST([ZLIB_COPY])


#----------------------------------------------------------------------------
# Extra Dyninst tests
#----------------------------------------------------------------------------

# The dyninst compile if/else tests require TBB, so this comes after
# all the package paths are known.

# Check that ParseAPI includes CFG loop support.

ORIG_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="${HOST_CXXFLAGS} ${CXXFLAGS} ${BOOST_IFLAGS} ${DYNINST_IFLAGS}"
CXXFLAGS="${CXXFLAGS} -I${LIBELF_INC} ${TBB_IFLAGS}"

AC_LANG_PUSH([C++])

if test "$DYNINST" != no ; then
  AC_MSG_CHECKING([for parseapi CFG loop support])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <vector>
#include <Symtab.h>
#include <Function.h>
#include <CFG.h>
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace std;
long num_blocks(LoopTreeNode * node)
{
  Loop * loop = node->loop;
  vector <Block *> bvec;
  loop->getLoopBasicBlocks(bvec);
  return (long) bvec.size();
}
]])],
  [cfg_loop_support=yes ; dyninst_features="$dyninst_features cfg"],
  [cfg_loop_support=no])

  AC_MSG_RESULT([$cfg_loop_support])

  if test "$cfg_loop_support" != yes ; then
    AC_MSG_ERROR([parseAPI does not include CFG loop support])
  fi
fi

#-------------------------------------------------

# Test if Block::getInsns() uses Instruction::Ptr or Instruction.
# 9.3 uses Instruction::Ptr, new-par-par and 10.x use Instruction.

use_instn_ptr=yes

if test "$DYNINST" != no ; then
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
long instruction_size(Block * block)
{
  map <Offset, Instruction::Ptr> imap;
  block->getInsns(imap);
  return (long) imap.begin()->second->size();
}
]])], [use_instn_ptr=yes], [use_instn_ptr=no])

  AC_MSG_RESULT([$use_instn_ptr])
fi

if test "$use_instn_ptr" = yes ; then
  AC_DEFINE(DYNINST_INSTRUCTION_PTR, 1, [dyninst uses Instruction::Ptr])
fi

#-------------------------------------------------

# Test if dyninst was built with libdw or libdwarf.
#
# There is not a good compile or link test for this, so we fall back
# on ldd.  But note: ldd only works for dynamic libraries and doesn't
# work cross-platform.

use_libdw=no

if test "$DYNINST" != no ; then
  AC_MSG_CHECKING([if dyninst built with libdw.so])

  dyn_dwarf="${DYNINST_LIB_DIR}/libdynDwarf.so"

  ORIG_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
  LD_LIBRARY_PATH=

  if test -f "$dyn_dwarf" ; then
    ldd "$dyn_dwarf" 2>/dev/null | $AWK '{print $1}' | grep -e 'libdw.so' >/dev/null
    if test $? -eq 0 ; then
       use_libdw=yes
    fi
  fi

  AC_MSG_RESULT([$use_libdw])

  LD_LIBRARY_PATH="$ORIG_LD_LIBRARY_PATH"

  if test "$use_libdw" = yes
  then dyninst_features="$dyninst_features libdw"
  else dyninst_features="$dyninst_features libdwarf"
  fi
fi

#-------------------------------------------------

# Test if dyninst supports cuda.

use_cuda=no

if test "$DYNINST" != no ; then
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
]])],
  [use_cuda=yes ; dyninst_features="$dyninst_features cuda"],
  [use_cuda=no])

  AC_MSG_RESULT([$use_cuda])
fi

#-------------------------------------------------

# Test if symtab and parseapi were built with openmp support.
# Again, not really a good test.

parse_openmp=no
symtab_openmp=no

if test "$DYNINST" != no ; then
  ORIG_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
  LD_LIBRARY_PATH=

  AC_MSG_CHECKING([if parseapi built with openmp])

  parseapi="${DYNINST_LIB_DIR}/libparseAPI.so"

  if test -f "$parseapi" ; then
    for lib in $openmp_lib_list ; do
      ldd "$parseapi" 2>/dev/null | $AWK '{print $1}' | grep -e "$lib" >/dev/null
      if test $? -eq 0 ; then
        parse_openmp=yes
	dyninst_features="$dyninst_features openmp"
	break
      fi
    done
  fi

  AC_MSG_RESULT([$parse_openmp])

  AC_MSG_CHECKING([if symtab built with openmp])

  symtabapi="${DYNINST_LIB_DIR}/libsymtabAPI.so"

  if test -f "$symtabapi" ; then
    for lib in $openmp_lib_list ; do
      ldd "$symtabapi" 2>/dev/null | $AWK '{print $1}' | grep -e "$lib" >/dev/null
      if test $? -eq 0 ; then
        symtab_openmp=yes
	dyninst_features="$dyninst_features par-symtab"
	break
      fi
    done
  fi

  AC_MSG_RESULT([$symtab_openmp])

  LD_LIBRARY_PATH="$ORIG_LD_LIBRARY_PATH"
fi

#-------------------------------------------------

# Test if dyninst has Intel GPU support.

dyninst_intel_gpu=no

if test "$DYNINST" != no ; then
  AC_MSG_CHECKING([if dyninst supports intel gpus])

  AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
#include <entryIDs.h>
unsigned int has_intel_enum(void)
{
  return intel_gpu_op_general;
}
]])],
  [dyninst_intel_gpu=yes ; dyninst_features="$dyninst_features intel-gpu"],
  [dyninst_intel_gpu=no])

  AC_MSG_RESULT([$dyninst_intel_gpu])
fi

if test "$dyninst_intel_gpu" = yes ; then
  AC_DEFINE(DYNINST_SUPPORTS_INTEL_GPU, 1, [dyninst supports Intel GPUs])
fi

#-------------------------------------------------

# Restore C++ flags from dyninst tests

AC_LANG_POP
CXXFLAGS="$ORIG_CXXFLAGS"

#-------------------------------------------------

# Temporary option to identify version of dyninst, only necessary if
# the above tests produce the wrong answer.
#
#  old = orig version from externals
#  libdw = uses libdw instead of libdwarf, but not cuda
#  cuda = uses libdw and supports cuda

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

#-------------------------------------------------

if test "$use_libdw" = yes && test "$elf_has_libdw" != yes ; then
  AC_MSG_WARN([dyninst uses libdw but elfutils is missing libdw])
  AC_MSG_WARN([dyninst:  $DYNINST])
  AC_MSG_WARN([elfutils: $LIBELF])
fi


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
  XERCES_COPY=no
fi

AC_SUBST([BACK_END_LABEL])

AM_CONDITIONAL(OPT_BUILD_BACK_END,  [test $enable_back_end = yes])
AM_CONDITIONAL(OPT_BUILD_FRONT_END, [test $enable_back_end = no])

AM_CONDITIONAL(OPT_BUILD_LIB_ALL,  [test $enable_back_end = no])
AM_CONDITIONAL(OPT_BUILD_TOOL_ALL, [test $enable_back_end = no])


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
     test "$LIBMONITOR" = no || test "$DYNINST" = no || \
     test "$no_xed_for_hpcrun" = yes || test "$libunwind_exists" = no
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
   || test "$libunwind_static" = no
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
# enable-openmp
#-------------------------------------------------

# Enable openmp support in hpcstruct and fnbounds.  Requires support
# from ParseAPI and Symtab, so all this really does is disable it, if
# desired.
#
# Note: this only controls openmp pragmas in hpctoolkit.  If dyninst
# is compiled with openmp, then those pragmas still exist.

AC_ARG_ENABLE([openmp],
  [AS_HELP_STRING([--enable-openmp],
      [enable openmp support in hpcstruct, requires support from dyninst])],
  [],
  [enable_openmp="$parse_openmp"])

if test "x$enable_openmp" != xyes ; then
  AC_MSG_NOTICE([disable openmp support in hpcstruct and hpcfnbounds])
  parse_openmp=no
  symtab_openmp=no
fi

hpcstruct_mesg=serial
ans=no

if test "$parse_openmp" = yes ; then
  AC_DEFINE([ENABLE_OPENMP], [1], [ParseAPI supports openmp (for hpcstruct)])
  hpcstruct_mesg=openmp
  ans=hpcstruct
else
  OPENMP_FLAG=
  symtab_openmp=no
fi

if test "$symtab_openmp" = yes ; then
  AC_DEFINE([ENABLE_OPENMP_SYMTAB], [1], [Symtab supports openmp (for fnbounds)])
  hpcstruct_mesg="$hpcstruct_mesg + fnbounds"
  ans="$ans + fnbounds"
fi

AC_MSG_NOTICE([openmp support from dyninst: $ans])

AM_CONDITIONAL([OPT_ENABLE_OPENMP], [test "$parse_openmp" = yes])
AM_CONDITIONAL([OPT_ENABLE_OPENMP_SYMTAB], [test "$symtab_openmp" = yes])


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

# CUDA support for CPU-GPU-Blame-shifting.  Also requires CUPTI,
# in CUDA, CUDA/extras/CUPTI, or in a separate install 
# directory specified using the --with-cupti option.

CUDA="$default_cuda_prefix"

OPT_HAVE_CUDA=no
OPT_CUDA_IFLAGS=
OPT_CUDA_LDFLAGS=

AC_ARG_WITH([cuda],
  [AS_HELP_STRING([--with-cuda=PATH],
      [path to cuda install directory])],
  [CUDA="$withval"],
  [])

AC_MSG_CHECKING([for cuda])

case "$CUDA" in
  /* )
    if test ! -f "${CUDA}/include/cuda.h" ; then
      AC_MSG_ERROR([unable to find cuda.h in: $CUDA])
    fi
    OPT_CUDA_IFLAGS="-I${CUDA}/include"

    for lib in $multilib_path ; do
      if test -f "${CUDA}/${lib}/libcudart.so" ; then
        OPT_CUDA_LDFLAGS="-L${CUDA}/${lib} -lcudart"
        break
      fi
    done
    if test "x$OPT_CUDA_LDFLAGS" = x ; then
      AC_MSG_ERROR([unable to find libcudart.so in: $CUDA])
    fi
    OPT_HAVE_CUDA=yes
    num_pkgs=`expr 1 + $num_pkgs`
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([cuda directory must be absolute path: $CUDA])
    ;;
esac

AC_MSG_RESULT([$CUDA])

AM_CONDITIONAL([OPT_ENABLE_CUDA], [test "$OPT_HAVE_CUDA" = yes])

if test "$OPT_HAVE_CUDA" = yes ; then
  AC_DEFINE([OPT_HAVE_CUDA], [1], [Have support for cuda])
fi

AC_DEFINE_UNQUOTED([CUDA_INSTALL_PREFIX],
  ["$CUDA"], ["Cuda install prefix"])

AC_SUBST([OPT_CUDA_IFLAGS])
AC_SUBST([OPT_CUDA_LDFLAGS])

#-------------------------------------------------
# Option: --with-cupti=PATH
#-------------------------------------------------

# CUPTI is either in CUDA, CUDA/extras/CUPTI, or in a directory
#  specified separately here.

CUPTI=no
case "$CUDA" in
  /* ) if test -d "${CUDA}/extras/CUPTI/include" ; then CUPTI="${CUDA}/extras/CUPTI" ; else CUPTI=${CUDA} ; fi
       ;;
esac

OPT_HAVE_CUPTI=no
OPT_CUPTI_IFLAGS=
OPT_CUPTI_LDFLAGS=

AC_ARG_WITH([cupti],
  [AS_HELP_STRING([--with-cupti=PATH],
      [path to cupti install directory, default is from cuda])],
  [CUPTI="$withval"],
  [])

AC_MSG_CHECKING([for cupti])

case "$CUPTI" in
  /* )
    if test ! -f "${CUPTI}/include/cupti.h" ; then
      AC_MSG_ERROR([unable to find cupti.h in: $CUPTI])
    fi
    OPT_CUPTI_IFLAGS="-I${CUPTI}/include"

    for lib in $multilib_path ; do
      if test -f "${CUPTI}/${lib}/libcupti.so" ; then
        OPT_CUPTI_LDFLAGS="-L${CUPTI}/$lib -Wl,-rpath=${CUPTI}/$lib -lcupti"
        break
      fi
    done
    if test "x$OPT_CUPTI_LDFLAGS" = x ; then
      AC_MSG_ERROR([unable to find libcupti.so in: $CUPTI])
    fi
    OPT_HAVE_CUPTI=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([cupti directory must be absolute path: $CUPTI])
    ;;
esac

AC_MSG_RESULT([$CUPTI])

# CUDA doesn't work without CUPTI.
if test "$OPT_HAVE_CUDA" = yes && test "$OPT_HAVE_CUPTI" = no ; then
  AC_MSG_WARN([cuda is configured without cupti])
  AC_MSG_ERROR([use --with-cupti to provide location of cupti])
fi

if test "$OPT_HAVE_CUPTI" = yes ; then
  AC_DEFINE([HAVE_CUPTI_H], [1], [The <cupti.h> header file is available.])
fi

AM_CONDITIONAL([OPT_ENABLE_CUPTI], [test "$OPT_HAVE_CUPTI" = yes])

OPT_CUPTI="$CUPTI"

AC_DEFINE_UNQUOTED([CUPTI_INSTALL_PREFIX],
  ["$CUPTI"], ["CUPTI install prefix"])

AC_SUBST([OPT_CUPTI])
AC_SUBST([OPT_CUPTI_IFLAGS])
AC_SUBST([OPT_CUPTI_LDFLAGS])



#-------------------------------------------------
# Option: --with-opencl=PATH
#-------------------------------------------------

OPENCL=no
OPT_HAVE_OPENCL=no
OPT_OPENCL_IFLAGS=

AC_ARG_WITH([opencl],
  [AS_HELP_STRING([--with-opencl=PATH],
      [path to opencl headers])],
  [OPENCL="$withval"],
  [])

AC_MSG_CHECKING([for opencl])

case "$OPENCL" in
  /* )
    if test ! -f "${OPENCL}/include/CL/cl.h" ; then
      AC_MSG_ERROR([unable to find CL/cl.h in: $OPENCL])
    else
      OPT_OPENCL_IFLAGS="-I${OPENCL}/include"
      OPT_HAVE_OPENCL=yes
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([opencl header path must be absolute: $OPENCL])
    ;;
esac

if test "$OPENCL" = yes && test "$OPT_HAVE_OPENCL" = no ; then
  AC_MSG_ERROR([use --with-opencl to provide path/location of opencl headers])
fi

if test "$OPT_HAVE_OPENCL" = yes ; then
  AC_MSG_NOTICE([The <CL/cl.h> header file is available.])
fi

AM_CONDITIONAL([OPT_ENABLE_OPENCL], [test "$OPT_HAVE_OPENCL" = yes])

AC_MSG_RESULT([$OPENCL])

AC_SUBST([OPT_OPENCL_IFLAGS])



#-------------------------------------------------
# Option: --with-igc=PATH
#-------------------------------------------------

IGC=no
OPT_HAVE_IGC=no
OPT_IGC_IFLAGS=
OPT_IGC_LDFLAGS=

AC_ARG_WITH([igc],
  [AS_HELP_STRING([--with-igc=PATH],
      [path to igc install directory])],
  [IGC="$withval"],
  [])

AC_MSG_CHECKING([for igc])

case "$IGC" in
  /* )
    if test ! -f "${IGC}/include/igc/igc.opencl.h" ; then
      AC_MSG_ERROR([unable to find igc.opencl.h in: $IGC])
    fi
    OPT_IGC_IFLAGS="-DENABLE_IGC -I${IGC}/include"

    IGC_LDFLAGS =
    IGA_LDFLAGS =

    for lib in $multilib_path ; do
      if test -f "${IGC}/${lib}/libigc.so" ; then
        IGC_LDFLAGS="-L${IGC}/$lib -Wl,-rpath=${IGC}/$lib -ligc"
        break
      fi
    done
    if test "x$IGC_LDFLAGS" = x ; then
      AC_MSG_ERROR([unable to find libigc.so in: $IGC])
    fi

    for lib in $multilib_path ; do
      if test -f "${IGC}/${lib}/libiga64.so" ; then
        IGA_LDFLAGS="-L${IGC}/$lib -Wl,-rpath=${IGC}/$lib -liga64"
        break
      fi
    done
    if test "x$IGA_LDFLAGS" = x ; then
      AC_MSG_ERROR([unable to find libiga.so in: $IGC])
    fi

    OPT_IGC_LDFLAGS="${IGC_LDFLAGS} ${IGA_LDFLAGS}"

    OPT_HAVE_IGC=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([igc directory must be absolute path: $IGC])
    ;;
esac

AM_CONDITIONAL([OPT_ENABLE_IGC], [test "$OPT_HAVE_IGC" = yes])

AC_MSG_RESULT([$IGC])

AC_SUBST([OPT_IGC])
AC_SUBST([OPT_IGC_IFLAGS])
AC_SUBST([OPT_IGC_LDFLAGS])

#-------------------------------------------------
# Option: --with-metrics-discovery=PATH
#-------------------------------------------------

METRICS_DISCOVERY=no
OPT_HAVE_METRICS_DISCOVERY=no
OPT_METRICS_DISCOVERY_IFLAGS=
OPT_METRICS_DISCOVERY_LDFLAGS=

AC_ARG_WITH([metrics-discovery],
  [AS_HELP_STRING([--with-metrics-discovery=PATH],
      [path to metrics-discovery install directory])],
  [METRICS_DISCOVERY="$withval"],
  [])

AC_MSG_CHECKING([for metrics-discovery])

case "$METRICS_DISCOVERY" in
  /* )
    if test ! -f "${METRICS_DISCOVERY}/include/metrics_discovery_api.h" ; then
      AC_MSG_ERROR([unable to find metrics_discovery_api.h in: $METRICS_DISCOVERY])
    fi
    OPT_METRICS_DISCOVERY_IFLAGS="-I${METRICS_DISCOVERY}/include"

    for lib in $multilib_path ; do
      if test -f "${METRICS_DISCOVERY}/${lib}/libmd.so" ; then
        OPT_METRICS_DISCOVERY_LDFLAGS="-L${METRICS_DISCOVERY}/$lib -Wl,-rpath=${METRICS_DISCOVERY}/$lib -lmd"
        break
      fi
    done
    if test "x$OPT_METRICS_DISCOVERY_LDFLAGS" = x ; then
      AC_MSG_ERROR([unable to find libmd.so in: $METRICS_DISCOVERY])
    fi
    OPT_HAVE_METRICS_DISCOVERY=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([metrics-discovery directory must be absolute path: $METRICS_DISCOVERY])
    ;;
esac

AM_CONDITIONAL([OPT_ENABLE_METRICS_DISCOVERY], [test "$OPT_HAVE_METRICS_DISCOVERY" = yes])

AC_MSG_RESULT([$METRICS_DISCOVERY])

AC_SUBST([OPT_METRICS_DISCOVERY])
AC_SUBST([OPT_METRICS_DISCOVERY_IFLAGS])
AC_SUBST([OPT_METRICS_DISCOVERY_LDFLAGS])

#-------------------------------------------------
# Option: --with-gtpin=PATH
#-------------------------------------------------

GTPIN=no
OPT_HAVE_GTPIN=no
OPT_GTPIN_IFLAGS=
OPT_GTPIN_LIBDIR=

AC_ARG_WITH([gtpin],
  [AS_HELP_STRING([--with-gtpin=PATH],
      [path to gtpin install directory])],
  [GTPIN="$withval"],
  [])

AC_MSG_CHECKING([for gtpin])

case "$GTPIN" in
  /* )
    if test ! -f "${GTPIN}/Profilers/Include/gtpin.h" ; then
      AC_MSG_ERROR([unable to find gtpin.h in: $GTPIN])
    fi
    OPT_GTPIN_IFLAGS="-I${GTPIN}/Profilers/Include -I${GTPIN}/Profilers/Include/ged/intel64/"

    if test -f "${GTPIN}/Profilers/Lib/intel64/libgtpin.so" ; then
      OPT_GTPIN_LIBDIR="${GTPIN}/Profilers/Lib/intel64"
    fi

    if test "x$OPT_GTPIN_LIBDIR" = x ; then
      AC_MSG_ERROR([unable to find libgtpin.so in: $GTPIN])
    fi
    OPT_HAVE_GTPIN=yes
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([gtpin directory must be absolute path: $GTPIN])
    ;;
esac

AM_CONDITIONAL([OPT_ENABLE_GTPIN], [test "$OPT_HAVE_GTPIN" = yes])

AC_MSG_RESULT([$GTPIN])

AC_SUBST([OPT_GTPIN])
AC_SUBST([OPT_GTPIN_IFLAGS])
AC_SUBST([OPT_GTPIN_LIBDIR])


#-------------------------------------------------
# Option: --with-rocm=PATH
#-------------------------------------------------

# Can specify ROCM with all-in-one ROCM + HIP directory (normally
# /opt/rocm), or with spack packages: hip, rocm-dbgapi, roctracer-dev,
# or partially with /opt/rocm plus a subset of spack packages.
#
# Note: may use both --with-rocm plus specific rocm package(s).
# In this case, the specific overrides the general.

ROCM=
ROCM_HIP=
ROCM_DBGAPI=
ROCM_TRACER=

AC_ARG_WITH([rocm],
  AS_HELP_STRING([--with-rocm=PATH],
      [path to full ROCM + HIP install directory]),
  [ROCM="$withval"], [])

AC_ARG_WITH([rocm-hip],
  AS_HELP_STRING([--with-rocm-hip=PATH],
      [path to hip install directory]),
  [ROCM_HIP="$withval"], [])

AC_ARG_WITH([rocm-dbgapi],
  AS_HELP_STRING([--with-rocm-dbgapi=PATH],
      [path to rocm-dbgapi install directory]),
  [ROCM_DBGAPI="$withval"], [])

AC_ARG_WITH([rocm-tracer],
  AS_HELP_STRING([--with-rocm-tracer=PATH],
      [path to roctracer-dev install directory]),
  [ROCM_TRACER="$withval"], [])

ROCM_HIP_IFLAGS=
ROCM_DBGAPI_IFLAGS=
ROCM_TRACER_IFLAGS=

ROCM_HIP_LD_DIR=
ROCM_DBGAPI_LD_DIR=
ROCM_TRACER_LD_DIR=

ROCM_HIP_MESG=
ROCM_DBGAPI_MESG=
ROCM_TRACER_MESG=

require_rocm=no

AC_MSG_NOTICE([checking for rocm ....])

#
# The all-in-one system rocm, commonly /opt/rocm.
#
case "$ROCM" in
  /* )
    require_rocm=yes
    found=no

    # HIP
    if test -f "$ROCM/hip/include/hip/hip_runtime.h" ; then
      AC_MSG_NOTICE([found $ROCM/hip/include/hip/hip_runtime.h])
      ROCM_HIP_IFLAGS="-I$ROCM/hip/include"
      ROCM_HIP_MESG="$ROCM/hip"
      found=yes
    fi
    if test -f "$ROCM/hip/lib/libamdhip64.so" ; then
      AC_MSG_NOTICE([found $ROCM/hip/lib/libamdhip64.so])
      ROCM_HIP_LD_DIR="$ROCM/hip/lib"
      ROCM_HIP_MESG="$ROCM/hip"
      found=yes
    fi

    # AMD-DBGAPI
    if test -f "$ROCM/include/amd-dbgapi.h" ; then
      AC_MSG_NOTICE([found $ROCM/include/amd-dbgapi.h])
      ROCM_DBGAPI_IFLAGS="-I$ROCM/include"
      ROCM_DBGAPI_MESG="$ROCM"
      found=yes
    fi
    if test -f "$ROCM/lib/librocm-dbgapi.so" ; then
      AC_MSG_NOTICE([found $ROCM/lib/librocm-dbgapi.so])
      ROCM_DBGAPI_LD_DIR="$ROCM/lib"
      ROCM_DBGAPI_MESG="$ROCM"
      found=yes
    fi

    # ROCTRACER
    if test -f "$ROCM/roctracer/include/roctracer_hip.h" ; then
      AC_MSG_NOTICE([found $ROCM/roctracer/include/roctracer_hip.h])
      ROCM_TRACER_IFLAGS="-I$ROCM/roctracer/include"
      ROCM_TRACER_MESG="$ROCM/roctracer"
      found=yes
    fi
    if test -f "$ROCM/roctracer/lib/libroctracer64.so" ; then
      AC_MSG_NOTICE([found $ROCM/roctracer/lib/libroctracer64.so])
      ROCM_TRACER_LD_DIR="$ROCM/roctracer/lib"
      ROCM_TRACER_MESG="$ROCM/roctracer"
      found=yes
    fi

    # warn if given dir has nothing useful
    if test "$found" = no ; then
      AC_MSG_WARN([found nothing useful in $ROCM])
    fi
    ;;
  * )
    ROCM=no
    ;;
esac

#
# Individual packages, the specific overrides the general.
#
case "$ROCM_HIP" in
  /* )
    require_rocm=yes
    found=no

    if test -f "$ROCM_HIP/include/hip/hip_runtime.h" ; then
      AC_MSG_NOTICE([found $ROCM_HIP/include/hip/hip_runtime.h])
      ROCM_HIP_IFLAGS="-I$ROCM_HIP/include"
      ROCM_HIP_MESG="$ROCM_HIP"
      found=yes
    fi
    if test -f "$ROCM_HIP/lib/libamdhip64.so" ; then
      AC_MSG_NOTICE([found $ROCM_HIP/lib/libamdhip64.so])
      ROCM_HIP_LD_DIR="$ROCM_HIP/lib"
      ROCM_HIP_MESG="$ROCM_HIP"
      found=yes
    fi
    if test "$found" = no ; then
      AC_MSG_WARN([found nothing useful in $ROCM_HIP])
    fi
    ;;
  * )
    ROCM_HIP=no
    ;;
esac

case "$ROCM_DBGAPI" in
  /* )
    require_rocm=yes
    found=no

    if test -f "$ROCM_DBGAPI/include/amd-dbgapi.h" ; then
      AC_MSG_NOTICE([found $ROCM_DBGAPI/include/amd-dbgapi.h])
      ROCM_DBGAPI_IFLAGS="-I$ROCM_DBGAPI/include"
      ROCM_DBGAPI_MESG="$ROCM_DBGAPI"
      found=yes
    fi
    if test -f "$ROCM_DBGAPI/lib/librocm-dbgapi.so" ; then
      AC_MSG_NOTICE([found $ROCM_DBGAPI/lib/librocm-dbgapi.so])
      ROCM_DBGAPI_LD_DIR="$ROCM_DBGAPI/lib"
      ROCM_DBGAPI_MESG="$ROCM_DBGAPI"
      found=yes
    fi
    if test "$found" = no ; then
      AC_MSG_WARN([found nothing useful in $ROCM_DBGAPI])
    fi
    ;;
  * )
    ROCM_DBGAPI=no
    ;;
esac

case "$ROCM_TRACER" in
  /* )
    require_rocm=yes
    found=no

    if test -f "$ROCM_TRACER/roctracer/include/roctracer_hip.h" ; then
      AC_MSG_NOTICE([found $ROCM_TRACER/roctracer/include/roctracer_hip.h])
      ROCM_TRACER_IFLAGS="-I$ROCM_TRACER/roctracer/include"
      ROCM_TRACER_MESG="$ROCM_TRACER/roctracer"
      found=yes
    fi
    if test -f "$ROCM_TRACER/roctracer/lib/libroctracer64.so" ; then
      AC_MSG_NOTICE([found $ROCM_TRACER/roctracer/lib/libroctracer64.so])
      ROCM_TRACER_LD_DIR="$ROCM_TRACER/roctracer/lib"
      ROCM_TRACER_MESG="$ROCM_TRACER/roctracer"
      found=yes
    fi
    if test "$found" = no ; then
      AC_MSG_WARN([found nothing useful in $ROCM_TRACER])
    fi
    ;;
  * )
    ROCM_TRACER=no
    ;;
esac

#
# Check that we found all the pieces.
#
OPT_HAVE_ROCM=no

if test "$require_rocm" = yes
then
  if test "x$ROCM_HIP_IFLAGS" = x ; then
    AC_MSG_ERROR([unable to find hip_runtime.h])
  fi
  if test "x$ROCM_DBGAPI_IFLAGS" = x ; then
    AC_MSG_ERROR([unable to find amd-dbgapi.h])
  fi
  if test "x$ROCM_TRACER_IFLAGS" = x ; then
    AC_MSG_ERROR([unable to find roctracer_hip.h])
  fi

  if test "x$ROCM_HIP_LD_DIR" = x ; then
    AC_MSG_ERROR([unable to find libamdhip64.so])
  fi
  if test "x$ROCM_DBGAPI_LD_DIR" = x ; then
    AC_MSG_ERROR([unable to find librocm-dbgapi.so])
  fi
  if test "x$ROCM_TRACER_LD_DIR" = x ; then
    AC_MSG_ERROR([unable to find libroctracer64.so])
  fi

  OPT_HAVE_ROCM=yes
  OPT_ROCM_IFLAGS="$ROCM_HIP_IFLAGS  $ROCM_DBGAPI_IFLAGS  $ROCM_TRACER_IFLAGS"
  OPT_ROCM_LD_LIB_PATH="${ROCM_HIP_LD_DIR}:${ROCM_DBGAPI_LD_DIR}:${ROCM_TRACER_LD_DIR}"
fi

#
# Check for ROCM version
#
rocm_mesg=no

if test "$OPT_HAVE_ROCM" = yes ; then
  version_file="${ROCM_HIP_MESG}/include/hip/hip_version.h"

  rocm_major=`grep VERSION_MAJOR $version_file | head -1 | awk '{ print $3 }'`
  rocm_minor=`grep VERSION_MINOR $version_file | head -1 | awk '{ print $3 }'`

  if test "x$rocm_major" = x || test "x$rocm_minor" = x ; then
    AC_MSG_WARN([unable to find rocm version])
    rocm_version=unknown
  else
    rocm_version="${rocm_major}.${rocm_minor}"

    # Require 3.10 or 4.0 or greater.
    if ( test $rocm_major -eq 3 && test $rocm_minor -ge 10 )  \
       || test $rocm_major -ge 4
    then :
    else
      AC_MSG_ERROR([rocm version $rocm_version is too old, use 3.10.0 or later])
    fi
  fi
  rocm_mesg="yes, version: $rocm_version"
fi

AC_MSG_NOTICE([rocm: $rocm_mesg])

if test "$OPT_HAVE_ROCM" = "yes" ; then
  AC_DEFINE(USE_ROCM, 1, [use ROCM])
fi

AM_CONDITIONAL([OPT_ENABLE_ROCM], [test "$OPT_HAVE_ROCM" = yes])

AC_SUBST([OPT_ROCM_IFLAGS])
AC_SUBST([OPT_ROCM_LD_LIB_PATH])


#-------------------------------------------------
# Option: --with-level0=PATH
#-------------------------------------------------

OPT_LEVEL0=
OPT_LEVEL0_INCPATH=
OPT_LEVEL0_LDFLAGS=
OPT_LEVEL0_LIBPATH=

AC_ARG_WITH([level0],
  AS_HELP_STRING([--with-level0=PATH],
                 [use given Level Zero installation (absolute path) with hpcrun (default is NO)]),
  [if test $withval != no; then
     OPT_HAVE_LEVEL0=yes
     if ( echo "${withval}" | grep -v "^/" >/dev/null 2>&1 ); then
       AC_MSG_ERROR([--with-level0 requires absolute path as argument; given '${withval}'])
     fi
     OPT_LEVEL0=${withval}
   fi])

if test "$OPT_HAVE_LEVEL0" = "yes" ; then
  AC_DEFINE(USE_LEVEL0, 1, [use Level Zero])
  OPT_LEVEL0_IFLAGS="-I${OPT_LEVEL0}/include"
fi

AM_CONDITIONAL([OPT_ENABLE_LEVEL0], [test "$OPT_HAVE_LEVEL0" = yes])

AC_SUBST([OPT_LEVEL0])
AC_SUBST([OPT_LEVEL0_IFLAGS])
AC_SUBST([OPT_LEVEL0_LDFLAGS])

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
# Option: --with-valgrind=PATH
#-------------------------------------------------

VALGRIND="$default_valgrind_prefix"

AC_ARG_WITH([valgrind],
  AS_HELP_STRING([--with-valgrind=PATH],
                 [path to Valgrind install directory]),
  [VALGRIND="$withval"],
  [])

VALGRIND_IFLAGS=

AC_MSG_CHECKING([for Valgrind headers])

case "$VALGRIND" in
  /* )
    # valgrind/valgrind.h must exist on the include path
    if test -f "$VALGRIND"/include/valgrind/valgrind.h; then
      VALGRIND_IFLAGS=-I"$VALGRIND"/include
    else
      AC_MSG_ERROR([unable to find valgrind.h in: $VALGRIND])
    fi
    ;;
  no )
    ;;
  * )
    AC_MSG_ERROR([Valgrind directory must be an absolute path])
    ;;
esac

AC_MSG_RESULT([$VALGRIND])

AC_SUBST([VALGRIND_IFLAGS])


#-------------------------------------------------
# Option: --enable-valgrind-annotations
#-------------------------------------------------

OPT_ENABLE_VG_ANNOTATIONS="no"

AC_ARG_ENABLE([valgrind-annotations],
  AS_HELP_STRING([--enable-valgrind-annotations],
                 [Add extra annotations for Valgrind analysis (aids debugging)]),
  [case "${enableval}" in
     yes) OPT_ENABLE_VG_ANNOTATIONS="yes" ;;
     no)  OPT_ENABLE_VG_ANNOTATIONS="no" ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-valgrind-annotations]) ;;
   esac],
  [OPT_ENABLE_VG_ANNOTATIONS=no])

if test x"$OPT_ENABLE_VG_ANNOTATIONS" = xyes; then
  if test x"$VALGRIND" = xno; then
    AC_MSG_ERROR([Valgrind annotations requested but valgrind headers not available!])
  fi
  AC_DEFINE([ENABLE_VG_ANNOTATIONS], [1],
            [Add extra annotations for debugging with Valgrind])
fi

AM_CONDITIONAL(OPT_ENABLE_VG_ANNOTATIONS, test "${OPT_ENABLE_VG_ANNOTATIONS}" = "yes")


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
# Launch script variables
#----------------------------------------------------------------------------

# Variables to insert into every launch script.  We write this
# manually to guarantee that it exists before configure runs file
# substitution from AC-OUTPUT.

launch_script_file='config/launch-script-vars'

mkdir -p config
rm -f "$launch_script_file"

cat <<EOF >"$launch_script_file"
# If this script can't find its install directory, then set 'prefix'
# here.  This should only be necessary if you've moved both the script
# and the install directory.
#
# Set 'hash_value' to empty to disable matching the hash with the
# install directory.  Note: re-running configure creates a new hash.
# If you copy this script out of its install directory and rerun
# configure, then the hashes won't match.

prefix='$prefix'
hash_value='$hash_value'

VERSION='$PACKAGE_VERSION'
git_version='$git_version'
spack_spec='$spack_spec'
EOF

AC_SUBST_FILE([launch_script_vars])
launch_script_vars="$launch_script_file"

AC_SUBST_FILE([launch_early_options])
launch_early_options="${srcdir}/config/launch-options"


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
  src/extern/Makefile \
  src/extern/libunwind/Makefile \
  src/extern/lzma/Makefile \
  src/tool/Makefile \
  src/tool/hpcfnbounds/Makefile \
  src/tool/hpcfnbounds2/Makefile \
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
AC_CONFIG_FILES([src/tool/hpcstruct/dotgraph],
  [chmod +x src/tool/hpcstruct/dotgraph])

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

dyninst_features=`echo $dyninst_features`

AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([HPCToolkit summary])
AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([  HPCToolkit version: ${PACKAGE_VERSION}])
AC_MSG_NOTICE([  git version: $git_version])
AC_MSG_NOTICE([  spack spec:  $spack_spec])
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
AC_MSG_NOTICE([  hpcprof-mpi:  ${prof_mpi_avail}])
AC_MSG_NOTICE([  unwinder:     ${unwinder_mesg}])
AC_MSG_NOTICE([  mpi wrapper?: ${enable_mpi_wrapper}])
AC_MSG_NOTICE([  hpcrun/lush?: ${opt_enable_lush}])
AC_MSG_NOTICE([  datacentric?: ${OPT_ENABLE_DATACENTRIC_TRACE}])
AC_MSG_NOTICE([  hpcserver?:   ${enable_hpcserver}])
AC_MSG_NOTICE([  hpcstruct:    ${hpcstruct_mesg}])
AC_MSG_NOTICE([  perf-events:  ${perf_event_mesg}])
AC_MSG_NOTICE([  rtld-audit:   ${rtld_audit_mesg}])
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
AC_MSG_NOTICE([  OpenMP:    '${OPENMP_FLAG}'])
AC_MSG_NOTICE([  CCAS:      '${CCAS}'])
AC_MSG_NOTICE([  CCASFLAGS: '${CCASFLAGS}'])
AC_MSG_NOTICE([  LDFLAGS:   '${LDFLAGS}'])
AC_MSG_NOTICE([  MPICC:     '${MPICC}'])
AC_MSG_NOTICE([  MPICXX:    '${MPICXX}'])
AC_MSG_NOTICE([  MPIF77:    '${MPIF77}'])
AC_MSG_NOTICE([  HPCPROFMPI_LT_LDFLAGS: '${HPCPROFMPI_LT_LDFLAGS}'])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  binutils:     ${BINUTILS}])
AC_MSG_NOTICE([  boost:        ${BOOST}])
AC_MSG_NOTICE([  bzip:         ${BZIP}])
AC_MSG_NOTICE([  dyninst:      ${DYNINST}])
AC_MSG_NOTICE([  dyninst:      ${dyninst_features}])
AC_MSG_NOTICE([  elfutils:     ${LIBELF}])
AC_MSG_NOTICE([  libdwarf:     ${LIBDWARF}])
AC_MSG_NOTICE([  libiberty:    ${LIBIBERTY}])
AC_MSG_NOTICE([  libmonitor:   ${LIBMONITOR}])
AC_MSG_NOTICE([  libunwind:    ${LIBUNWIND}])
AC_MSG_NOTICE([  lzma:         ${LZMA}])
AC_MSG_NOTICE([  mbedtls:      ${MBEDTLS}])
AC_MSG_NOTICE([  papi:         ${papi_mesg}])
AC_MSG_NOTICE([  perfmon:      ${perfmon_mesg}])
AC_MSG_NOTICE([  tbb:          ${TBB}])
AC_MSG_NOTICE([  xed:          ${XED2}])
AC_MSG_NOTICE([  xerces:       ${XERCES}])
AC_MSG_NOTICE([  zlib:         ${ZLIB}])
AC_MSG_NOTICE([  cuda:         ${CUDA}])
AC_MSG_NOTICE([  cupti:        ${CUPTI}])
AC_MSG_NOTICE([  opencl:        ${OPENCL}])
AC_MSG_NOTICE([  igc:          ${IGC}])
AC_MSG_NOTICE([  gtpin:        ${GTPIN}])
AC_MSG_NOTICE([  metrics-discovery: ${METRICS_DISCOVERY}])
AC_MSG_NOTICE([  papi-c-cupti: ${use_papi_c_cupti}])
AC_MSG_NOTICE([  rocm:         ${rocm_mesg}])
if test "$OPT_HAVE_ROCM" = yes ; then
  AC_MSG_NOTICE([  rocm hip:     $ROCM_HIP_MESG])
  AC_MSG_NOTICE([  rocm dbgapi:  $ROCM_DBGAPI_MESG])
  AC_MSG_NOTICE([  rocm tracer:  $ROCM_TRACER_MESG])
fi
AC_MSG_NOTICE([  valgrind:     ${VALGRIND}])
AC_MSG_NOTICE([  valgrind:     annotated: ${OPT_ENABLE_VG_ANNOTATIONS}])

if test "x$mult_spack_pkgs" != x ; then
  mult_spack_pkgs=`echo $mult_spack_pkgs`
  AC_MSG_NOTICE([])
  AC_MSG_WARN([multiple spack packages for: $mult_spack_pkgs])
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

if test "x$with_spack" = xno && test "$num_pkgs" -lt 8 ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Not using --with-spack.])
  AC_MSG_WARN([Unless you're building hpctoolkit only for docs or make dist,])
  AC_MSG_WARN([this is probably an error.  Rerun configure with:])
  AC_MSG_WARN([  --with-spack=/path/to/spack/install/gcc-x.y.z])
fi

if test "$warn_non_gnu" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Not using the GNU compiler for: $warn_non_gnu_mesg])
  AC_MSG_WARN([This is not necessarily an error, but some parts of hpctoolkit])
  AC_MSG_WARN([may not compile cleanly without gcc or g++.  If the build fails,])
  AC_MSG_WARN([try using GNU gcc and g++ before reporting an error.])

elif test "$warn_identify_compiler" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([Unable to identify the compiler version for: $warn_identify_compiler_mesg])
  AC_MSG_WARN([Check that CC, CXX and the compiler for the spack prereqs are])
  AC_MSG_WARN([all GNU and the same version.])

elif test "$warn_old_gnu" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([The CC/CXX compiler is too old: $CC_VERSION])
  AC_MSG_WARN([Please use GNU gcc/g++ version 5.x or later.])
fi

if test "$warn_compiler_mismatch" = yes ; then
  AC_MSG_NOTICE([])
  AC_MSG_WARN([The CC/CXX and spack compilers are not the same version.])
  AC_MSG_WARN([Check that CC, CXX and the compiler for the spack prereqs are])
  AC_MSG_WARN([all GNU and the same version.])
  AC_MSG_WARN([CC/CXX:  $CC_VERSION])
  AC_MSG_WARN([Spack:   $with_spack])
fi
