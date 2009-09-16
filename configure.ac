dnl -*-Mode: autoconf;-*-

dnl * BeginRiceCopyright *****************************************************
dnl
dnl $HeadURL$
dnl $Id$
dnl
dnl -----------------------------------
dnl Part of HPCToolkit (hpctoolkit.org)
dnl -----------------------------------
dnl 
dnl Copyright ((c)) 2002-2009, Rice University 
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

m4_include(config/hpc-cxxutils.m4)
m4_include(config/hpc-mpiutils.m4)

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

AC_INIT([hpctoolkit], [4.9.9], 
        [hpc@cs.rice.edu], [hpctoolkit])
AC_CONFIG_SRCDIR([src/include/HPCToolkitVersionInfo.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

# NOTE: please update config/README.version
AC_PREREQ(2.63)
AM_INIT_AUTOMAKE(1.10.2)
LT_PREREQ([2.2.6])
AM_MAINTAINER_MODE()

AC_CANONICAL_BUILD()
AC_CANONICAL_HOST()

AC_MSG_NOTICE([INFO: on entry: CXX=${CXX} CXXFLAGS=${CXXFLAGS}])
AC_MSG_NOTICE([INFO: on entry: CC=${CC} CFLAGS=${CFLAGS}])
AC_MSG_NOTICE([INFO: on entry: MPICXX=${MPICXX}])

MPICXX_ORIG=${MPICXX}

#----------------------------------------------------------------------------
# Setup
#----------------------------------------------------------------------------

# ${srcdir}, ${ac_top_srcdir}, ${ac_top_builddir}
hpctoolkit_top=`cd ${srcdir} && pwd`
hpctoolkit_build=`pwd`
HPCTOOLKIT_PLATFORM=`cd ${hpctoolkit_top}/config && ./hpcplatform`
AC_SUBST([HPCTOOLKIT_PLATFORM])


#-------------------------------------------------
# SVN version
#-------------------------------------------------

# Append svn revision to PACKAGE_VERSION.
#
# If this directory is an svn sandbox, then use svn info (and store
# the result in a file), else expect the answer to be in the file.
# Note: svn update changes the revision number, so svn info must take
# precedence over the file.
#
# *** N.B.: since PACKAGE_VERSION is also used in the 'dist' tarball,
# it is better not to include spaces or special characters within. ***

version_file="${srcdir}/.svn-revision"

AC_MSG_CHECKING([for svn revision])

svn_revision=`svn info $srcdir 2>/dev/null | grep -e '^Revision' | awk '{ print $2 }'`
if test "x$svn_revision" != x ; then
    PACKAGE_VERSION="${PACKAGE_VERSION}-r${svn_revision}"
    rm -f "$version_file"
    echo "$svn_revision" > "$version_file"
elif test -f "$version_file" ; then
    svn_revision=`cat $version_file`
    PACKAGE_VERSION="${PACKAGE_VERSION}-r${svn_revision}"
else
    svn_revision=0
fi

AC_MSG_RESULT([$svn_revision])

if test "$svn_revision" = 0 ; then
    AC_MSG_WARN([unable to find svn revision])
fi

# NOTE (tallent): Setting PACKAGE_VERSION in this way does not affect
# the corresponding -D that is automatically placed on the compilation
# line!  We add our own because 
AC_DEFINE_UNQUOTED([HPCTOOLKIT_VERSION], "${PACKAGE_VERSION}", [])
dnl AC_SUBST([HPCTOOLKIT_VERSION])


#-------------------------------------------------
# is_cross_compile: non-empty string if cross-compiling
#-------------------------------------------------

is_cross_compile=""
if test "x$build" != "x$host" ; then
   is_cross_compile=${host}
fi


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
AC_PROG_CC_C99()
if test "${ac_cv_prog_cc_c99}" = "no" ; then
  AC_MSG_ERROR([${CC} ${CFLAGS} does not support C99 mode!])
fi
AM_PROG_CC_C_O()
AM_PROG_AS()

AC_PROG_CXXCPP()


AC_PROG_INSTALL()

HPC_PROG_MPICXX(HPC_MPICXX_LIST)


AC_MSG_NOTICE([INFO: post ac_prog: CXX=${CXX} CXXFLAGS=${CXXFLAGS}])
AC_MSG_NOTICE([INFO: post ac_prog: CC=${CC} CFLAGS=${CFLAGS}])
AC_MSG_NOTICE([INFO: post ac_prog: MPICXX=${MPICXX} ])


# Use C++ compiler for all checks
AC_LANG([C++])

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
  x86_64*-*-linux* | mips64*-*-linux* | powerpc64*-*-linux*)
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
#
# FIXME: This should probably be split into separate CPU and OS
# sections.

HOST_OS=
HOST_CPU_MIPS=no
HOST_CPU_PPC=no
HOST_CPU_X86=no

case ${host} in
  # Linux
  i386*-*-linux* | i686*-*-linux* | x86_64*-*-linux*)
    # __i386 / __x86_64 && __linux
    HOST_OS="Linux"
    HOST_CPU_X86=yes
    AC_DEFINE([HOST_CPU_x86_64], [], []) # bogus to include 386
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  ia64*-*-linux*)
    # __ia64 && __linux
    HOST_OS="Linux"
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  mips64*-*-linux*)
    # __mips64 && __linux / _MIPS_SIM == _ABIN32 / _MIPS_SIM == _ABI64
    HOST_OS="Linux"
    HOST_CPU_MIPS=yes
    AC_DEFINE([HOST_PLATFORM_MIPS64LE_LINUX], [], [])
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  powerpc64*-*-linux*)
    # []
    HOST_OS="Linux"
    HOST_CPU_PPC=yes
    AC_DEFINE([HOST_OS_LINUX], [], [])
    ;;

  # MacOS
  powerpc*-*-darwin*)
    # __ppc__ / __MACH__
    HOST_OS="MacOS"
    HOST_CPU_PPC=yes
    AC_DEFINE([HOST_OS_MACOS], [], [])
    ;;

  i386*-*-darwin* | i686*-*-darwin*)
    HOST_OS="MacOS"
    HOST_CPU_X86=yes
    AC_DEFINE([HOST_CPU_x86_64], [], []) # bogus to include 386
    AC_DEFINE([HOST_OS_MACOS], [], [])
    ;;

  # IRIX
  mips*-*-irix*)
    # __mips && __sgi && __unix
    # _MIPS_ISA == _MIPS_ISA_MIPS1 | _MIPS_ISA == _MIPS_ISA_MIPS2
    # _MIPS_ISA == _MIPS_ISA_MIPS3 | _MIPS_ISA == _MIPS_ISA_MIPS4
    HOST_OS="IRIX"
    HOST_CPU_MIPS=yes
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


AM_CONDITIONAL([HOST_OS_LINUX], [test "$HOST_OS" = Linux])
AM_CONDITIONAL([HOST_CPU_MIPS], [test "$HOST_CPU_MIPS" = yes])
AM_CONDITIONAL([HOST_CPU_PPC],  [test "$HOST_CPU_PPC" = yes])
AM_CONDITIONAL([HOST_CPU_X86],  [test "$HOST_CPU_X86" = yes])


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
  my_opt_cflags="-g -O0 -Wall"
  my_opt_cxxflags="-g -O0 -Wall"
else 
  # On some compilers, -g implies -O0.  Make sure -O3 takes precedence!
  my_opt_cflags="-g -O3 -Wall"
  my_opt_cxxflags="-g -O3 -Wall"
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
        my_opt_cflags="-gopt -O3 -c9x"
      fi
    fi
    ;;

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

AC_MSG_NOTICE([INFO: final: CXX=${CXX} CXXFLAGS=${CXXFLAGS} HOST_CXXFLAGS=${HOST_CXXFLAGS}])
AC_MSG_NOTICE([INFO: final: CC=${CC} CFLAGS=${CFLAGS} HOST_CFLAGS=${HOST_CFLAGS}])

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

# External's minimum svn rev number, or else a fatal error.
EXTERNALS_MIN_REV=2469

AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([], [])])
AC_ARG_WITH([externals],
  [AS_HELP_STRING([--with-externals=PATH],
	  [path to external prereqs tree])],
  [],
  [with_externals=no])

if test "x$with_externals" != xno
then
  ext_file="${with_externals}/externs.conf"
  if test ! -f "$ext_file" ; then
    AC_MSG_ERROR([missing externals file: $ext_file])
  fi
  AC_MSG_NOTICE([using externals tree: $with_externals])
  externals_root=`( cd "$with_externals" && pwd )`
  . "$ext_file"
  if test "$ext_version" -ge "$EXTERNALS_MIN_REV" ; then :
  else
    AC_MSG_ERROR([externals tree too old, update to revision $EXTERNALS_MIN_REV or later])
  fi
  use_externals=yes
  externals_mesg="$ext_version"
else
  AC_MSG_WARN([not using --with-externals])
  use_externals=no
  externals_mesg=no
fi


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

case "$LIBDWARF" in
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

AC_MSG_RESULT([$LIBDWARF])

AC_SUBST([LIBDWARF_LIB])
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

case "$LIBELF" in
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

AC_MSG_RESULT([$LIBELF])

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

case "$LIBMONITOR" in
    /* )
	LIBMONITOR_INC="${LIBMONITOR}/include"
	if test -f "${LIBMONITOR_INC}/monitor.h" ; then :
	else
	    AC_MSG_ERROR([invalid libmonitor directory: $LIBMONITOR])
	fi
	LIBMONITOR_LIB="${LIBMONITOR}/lib"
	if test -f "${LIBMONITOR_LIB}/libmonitor.so" ; then
	    OPT_LIBMONITOR_DYNAMIC=yes
	fi
	if test -f "${LIBMONITOR_LIB}/libmonitor_wrap.a" ; then
	    OPT_LIBMONITOR_STATIC=yes
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

AC_SUBST([LIBMONITOR_INC])
AC_SUBST([LIBMONITOR_LIB])
AC_SUBST([LIBMONITOR_COPY])
AC_SUBST([LIBMONITOR_RUN_DIR])


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

if test "$use_externals" = yes ; then
  OPT_LIBUNWIND="$ext_libunwind_prefix"
  case "$OPT_LIBUNWIND" in
    /* ) OPT_HAVE_LIBUNWIND=yes ;;
  esac
fi

AC_ARG_WITH([libunwind],
  AS_HELP_STRING([--with-libunwind=PATH],
                 [use given LIBUNWIND installation (absolute path) with hpcrun (default is NO)]),
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
# with-monitor
#-------------------------------------------------

AC_MSG_CHECKING([for monitor])

OPT_WITH_MONITOR=no
OPT_INSTALL_MONITOR=no
MONITOR=
MONITOR_LIBS=
HPC_MONITOR=

if test "$use_externals" = yes ; then
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
  AC_DEFINE([HAVE_MONITOR], [], [Monitor library])
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

AC_MSG_CHECKING([for OpenAnalysis])

OA=no
if test "$use_externals" = yes ; then
  OA="$ext_open_analysis_prefix"
fi

AC_ARG_WITH([open-analysis],
  AS_HELP_STRING([--with-open-analysis=PATH],
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
# with-papi
#-------------------------------------------------

AC_MSG_CHECKING([whether configured with PAPI])

HPC_DEF_CHECK_CXX_PAPI_LINK

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
  if test "x$OPT_PAPI_LIBPATH" = x ; then
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

AC_MSG_CHECKING([for symtabAPI])

case "$SYMTABAPI" in
    /* )
	if test -f "${SYMTABAPI}/include/Symtab.h" ; then :
	else
	    AC_MSG_ERROR([invalid symtabAPI directory: $SYMTABAPI])
	fi
	SYMTABAPI_COPY=yes
	SYMTABAPI_RUN_DIR="$hpc_ext_libs_dir"
	;;
    no )
	SYMTABAPI_COPY=no
	SYMTABAPI_RUN_DIR=no
	;;
    * )
	AC_MSG_ERROR([symtabAPI directory must be absolute path: $SYMTABAPI])
	;;
esac

SYMTABAPI_INC="${SYMTABAPI}/include"
SYMTABAPI_LIB="${SYMTABAPI}/lib"

AC_MSG_RESULT([$SYMTABAPI])

AC_SUBST([SYMTABAPI_INC])
AC_SUBST([SYMTABAPI_LIB])
AC_SUBST([SYMTABAPI_COPY])
AC_SUBST([SYMTABAPI_RUN_DIR])


#-------------------------------------------------
# Option: --with-xed2=PATH
#-------------------------------------------------

XED2=no
if test "$use_externals" = yes ; then
    XED2="$ext_xed2_prefix"
fi

AC_ARG_WITH([xed2],
    [AS_HELP_STRING([--with-xed2=PATH],
	    [path to xed2 install directory])],
    [XED2="$withval"],
    [])

AC_MSG_CHECKING([for xed2])

case "$XED2" in
    /* )
	if test -f "${XED2}/include/xed-interface.h" ; then :
	else
	    AC_MSG_ERROR([invalid xed2 directory: $XED2])
	fi
	XED2_COPY=yes
	XED2_RUN_DIR="$hpc_ext_libs_dir"
	;;
    no )
	XED2_COPY=no
	XED2_RUN_DIR=no
	;;
    * )
	AC_MSG_ERROR([xed2 directory must be absolute path: $XED2])
	;;
esac

XED2_INC="${XED2}/include"
XED2_LIB="${XED2}/lib"

AC_MSG_RESULT([$XED2])

AC_SUBST([XED2_INC])
AC_SUBST([XED2_LIB])
AC_SUBST([XED2_COPY])
AC_SUBST([XED2_RUN_DIR])


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
XERCES_LDFLAGS="-L${XERCES}/lib -lxerces-c"

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
AC_SUBST([XERCES_COPY])
AC_SUBST([XERCES_LIB])


#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------

#-------------------------------------------------
# enable-hpcrun-flat: Only build hpcrun if on Linux and PAPI is present
#-------------------------------------------------

AC_MSG_CHECKING([whether to build hpcrun-flat])

OPT_ENABLE_HPCRUN_FLAT="no"
if test "$OPT_PAPI_DYNAMIC" = yes && test "$OPT_WITH_MONITOR" = yes \
   && test "${HOST_OS}" = "Linux"
then
  OPT_ENABLE_HPCRUN_FLAT="yes"
fi

AC_MSG_RESULT([${OPT_ENABLE_HPCRUN_FLAT}])
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
     test "$LIBMONITOR" = no || test "$SYMTABAPI" = no
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
AM_CONDITIONAL([OPT_ENABLE_HPCRUN], [test "$OPT_ENABLE_HPCRUN" = yes])



# Find location of libstdc++.  This is used to set LD_LIBRARY_PATH in
# hpcfnbounds.
#
# FIXME (tallent): This is completely broken for cross-compiling
# (there is no reason for the paths to be persistent).  Moreover, we
# should not be responsible for configuring runtime paths for
# libstdc++.
#
# FIXME -- Is there an Autoconf macro for this?
#
if test -n "${is_cross_compile}" ; then
  # For now, assume GCC.  We cannot use the more general test because
  # 'ldd' cannot execute in cross-compile mode.
  HPCRUN_LIBCXX_PATH=`${CXX} -print-file-name=libstdc++.so`
else
  src="/tmp/hello.$$.cc"
  obj="/tmp/hello.$$.out"
  cat <<EOF > "$src"
main(){}
EOF
  ${CXX} -o "$obj" "$src"
  HPCRUN_LIBCXX_PATH=`ldd $obj | $AWK '/libstdc\+\+/ {print $3}'`
  rm -f "$src" "$obj"
fi

HPCRUN_LIBCXX_PATH=`dirname $HPCRUN_LIBCXX_PATH`

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

if test "$OPT_LIBMONITOR_STATIC" = no ; then
  OPT_ENABLE_HPCRUN_STATIC=no
fi

AC_MSG_RESULT([${OPT_ENABLE_HPCRUN_STATIC}])
AM_CONDITIONAL(OPT_ENABLE_HPCRUN_STATIC, test "${OPT_ENABLE_HPCRUN_STATIC}" = "yes")


AC_MSG_CHECKING([whether to build hpcrun for dynamically linked executables])

OPT_ENABLE_HPCRUN_DYNAMIC=no
if test "$OPT_ENABLE_HPCRUN" = yes && test "$OPT_LIBMONITOR_DYNAMIC" = yes ; then
  OPT_ENABLE_HPCRUN_DYNAMIC=yes
fi

AC_MSG_RESULT([$OPT_ENABLE_HPCRUN_DYNAMIC])

AM_CONDITIONAL([OPT_ENABLE_HPCRUN_DYNAMIC],
  [test "$OPT_ENABLE_HPCRUN_DYNAMIC" = yes])


#-------------------------------------------------
# enable-mpi:
#-------------------------------------------------

AC_MSG_CHECKING([whether to build MPI versions (hpcprof-mpi)])

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

# tallent: FIXME: for now require an explicit request to enable MPI.
# Once hpcprof-mpi is ready, the second test should be replaced with:
#   "${OPT_REQ_ENABLE_MPI}" != "no"
OPT_ENABLE_MPI=no
if test -n "${MPICXX}" -a "${OPT_REQ_ENABLE_MPI}" = "yes" ; then
  OPT_ENABLE_MPI=yes
fi

AC_MSG_RESULT([${OPT_ENABLE_MPI} (request: ${OPT_REQ_ENABLE_MPI})])


if test "${OPT_REQ_ENABLE_MPI}" = "yes" -a "${OPT_ENABLE_MPI}" = no ; then
  AC_MSG_ERROR([MPI support requested but MPI compiler is invalid!])
fi

AM_CONDITIONAL(OPT_ENABLE_MPI, test "${OPT_ENABLE_MPI}" = "yes")


#-------------------------------------------------
# enable-lush, enable-lush-pthreads, with-cilk, monitor-exts
#-------------------------------------------------

AC_ARG_ENABLE([lush],
  [AS_HELP_STRING([--enable-lush],
                  [use lush backtrace in hpcrun])],
  [],
  [enable_lush=no])

AC_MSG_CHECKING([whether to use lush])

if test "$enable_lush" != yes ; then
  enable_lush=no
fi

AC_MSG_RESULT([$enable_lush])

AM_CONDITIONAL([OPT_ENABLE_LUSH], [test "$enable_lush" = yes])



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


# Note: the --enable-lush-pthreads and --enable-monitor-exts options
# are mostly temporary and may go away or may be folded into other
# options.

AC_ARG_ENABLE([lush-pthreads],
  [AS_HELP_STRING([--enable-lush-pthreads],
                  [enable lush pthreads (temp)])],
  [],
  [enable_lush_pthreads=no])

if test "$enable_lush_pthreads" = yes && test "$enable_lush" = no ; then
  AC_MSG_ERROR([enable-lush-pthreads requires enable-lush])
fi

AC_MSG_NOTICE([lush pthreads: $enable_lush_pthreads])
AM_CONDITIONAL([OPT_ENABLE_LUSH_PTHREADS], [test "$enable_lush_pthreads" = yes])


AC_ARG_ENABLE([monitor-exts],
  [AS_HELP_STRING([--enable-monitor-exts],
                  [enable extra monitor overrides (temp)])],
  [],
  [enable_monitor_exts=$enable_lush_pthreads])

wrap1='pthread_mutex_lock pthread_mutex_trylock pthread_mutex_unlock'
wrap2='pthread_spin_lock pthread_spin_trylock pthread_spin_unlock pthread_spin_destroy'
wrap3='pthread_cond_init pthread_cond_destroy pthread_cond_wait'
wrap4='pthread_cond_timedwait pthread_cond_signal pthread_cond_broadcast'

if test "$enable_monitor_exts" = yes ; then
  hpclink_add_csprof_wrap=yes
  hpclink_extra_wrap_names="$wrap1 $wrap2 $wrap3 $wrap4"
else
  hpclink_add_csprof_wrap=no
  hpclink_extra_wrap_names=
fi

AC_MSG_NOTICE([monitor extensions: $enable_monitor_exts])
AM_CONDITIONAL([OPT_ENABLE_MONITOR_EXTS], [test "$enable_monitor_exts" = yes])

AC_SUBST([hpclink_add_csprof_wrap])
AC_SUBST([hpclink_extra_wrap_names])


#-------------------------------------------------
# enable-bluegene: BG/* support
#-------------------------------------------------

AC_MSG_CHECKING([whether to enable IBM BlueGene/* support])

OPT_ENABLE_IBM_BLUEGENE="no"

AC_ARG_ENABLE([bluegene],
  AS_HELP_STRING([--enable-bluegene],
                 [Enable BlueGene/* support]),
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
  AC_DEFINE([HOST_SYSTEM_IBM_BLUEGENE], [], [IBM BlueGene/* support])
fi


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

OPT_UPC=no
OPT_UPC_IFLAGS=
OPT_UPC_LDFLAGS=
opt_upc_mesg=no

case "$with_upc" in
  /* )
    if test -f "${with_upc}/arch/include/spi/UPC.h" && \
       test -f "${with_upc}/runtime/SPI/libbgpspi.cnk.a"
    then
      OPT_UPC=yes
      OPT_UPC_IFLAGS="-I ${with_upc}/arch/include"
      OPT_UPC_LDFLAGS="${with_upc}/runtime/SPI/libbgpspi.cnk.a"
      opt_upc_mesg="$with_upc"
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
  AC_DEFINE([HAVE_HPC_GNUBINUTILS], [], [HPCToolkit binutils])
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
# Makefiles to create
#----------------------------------------------------------------------------

AC_CONFIG_FILES([ \
  Makefile \
  \
  doc/Makefile \
  doc/man/Makefile \
  doc/man/HPCToolkitVersionInfo.tex \
  doc/guides/Makefile \
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
  src/tool/hpcrun-flat/Makefile \
  src/tool/hpcstruct/Makefile \
  src/tool/misc/Makefile \
  src/tool/xprof/Makefile \
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

AC_CONFIG_FILES([src/tool/hpcrun/scripts/hpcrun],
  [chmod +x src/tool/hpcrun/scripts/hpcrun])
AC_CONFIG_FILES([src/tool/hpcrun/scripts/hpclink],
  [chmod +x src/tool/hpcrun/scripts/hpclink])
AC_CONFIG_FILES([src/tool/hpcfnbounds/hpcfnbounds],
  [chmod +x src/tool/hpcfnbounds/hpcfnbounds])
AC_CONFIG_FILES([src/tool/hpcprof/hpcprof],
  [chmod +x src/tool/hpcprof/hpcprof])
AC_CONFIG_FILES([src/tool/hpcprof-flat/hpcprof-flat],
  [chmod +x src/tool/hpcprof-flat/hpcprof-flat])
AC_CONFIG_FILES([src/tool/hpcproftt/hpcproftt],
  [chmod +x src/tool/hpcproftt/hpcproftt])

AC_SUBST_FILE([export_hpctoolkit])
export_hpctoolkit="${srcdir}/config/export-hpctoolkit"

AC_CONFIG_COMMANDS()

AC_OUTPUT()


#-------------------------------------------------
# Summary of the config options.
#-------------------------------------------------

AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([HPCToolkit summary])
AC_MSG_NOTICE([-------------------])
AC_MSG_NOTICE([  HPCToolkit version: ${PACKAGE_VERSION}])
AC_MSG_NOTICE([  Externals revision: ${externals_mesg}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  prefix: $prefix])
AC_MSG_NOTICE([  build:  $build])
AC_MSG_NOTICE([  host:   $host])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  build hpcrun:      ${enable_hpcrun_mesg}])
AC_MSG_NOTICE([  build hpcrun-flat: ${OPT_ENABLE_HPCRUN_FLAT}])
AC_MSG_NOTICE([  mpi support:       ${OPT_ENABLE_MPI}])
AC_MSG_NOTICE([  hpcrun/lush:       ${enable_lush}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  CC:       ${CC}])
AC_MSG_NOTICE([  CFLAGS:   ${CFLAGS} ${HOST_CFLAGS}])
AC_MSG_NOTICE([  CXX:      ${CXX}])
AC_MSG_NOTICE([  CXXFLAGS: ${CXXFLAGS} ${HOST_CXXFLAGS}])
AC_MSG_NOTICE([  LDFLAGS:  ${LDFLAGS}])
AC_MSG_NOTICE([  MPICXX:   ${MPICXX}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  binutils:     ${GNUBINUTILS}])
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
AC_MSG_NOTICE([  upc (BG/P):   ${opt_upc_mesg}])

