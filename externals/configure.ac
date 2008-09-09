dnl
dnl  Externals configure.
dnl
dnl  Copyright (c) 2008, Rice University.
dnl  All rights reserved.
dnl
dnl  Redistribution and use in source and binary forms, with or without
dnl  modification, are permitted provided that the following conditions are
dnl  met:
dnl
dnl  * Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl
dnl  * Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl  * Neither the name of Rice University (RICE) nor the names of its
dnl    contributors may be used to endorse or promote products derived from
dnl    this software without specific prior written permission.
dnl
dnl  This software is provided by RICE and contributors "as is" and any
dnl  express or implied warranties, including, but not limited to, the
dnl  implied warranties of merchantability and fitness for a particular
dnl  purpose are disclaimed. In no event shall RICE or contributors be
dnl  liable for any direct, indirect, incidental, special, exemplary, or
dnl  consequential damages (including, but not limited to, procurement of
dnl  substitute goods or services; loss of use, data, or profits; or
dnl  business interruption) however caused and on any theory of liability,
dnl  whether in contract, strict liability, or tort (including negligence
dnl  or otherwise) arising in any way out of the use of this software, even
dnl  if advised of the possibility of such damage.
dnl
dnl  $Id$
dnl

AC_INIT([externals], [0.x])

AC_CONFIG_AUX_DIR([config])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([Makefile.conf])
AC_CONFIG_FILES([externs.conf])
AC_CONFIG_FILES([options.conf])

AC_SUBST([CC])
AC_SUBST([CFLAGS])
AC_SUBST([CXX])
AC_SUBST([CXXFLAGS])

AC_SUBST([build_cpu])
AC_SUBST([build_os])
AC_SUBST([host_cpu])
AC_SUBST([host_os])

AC_SUBST([prefix])

AC_SUBST([binutils_prefix])
AC_SUBST([libdwarf_prefix])
AC_SUBST([libelf_prefix])
AC_SUBST([libmonitor_prefix])
AC_SUBST([libxml2_prefix])
AC_SUBST([old_monitor_prefix])
AC_SUBST([open_analysis_prefix])
AC_SUBST([symtabAPI_prefix])
AC_SUBST([xed2_prefix])
AC_SUBST([xerces_prefix])

AC_SUBST([libdwarf_inc])
AC_SUBST([libdwarf_lib])
AC_SUBST([libelf_inc])
AC_SUBST([libelf_lib])
AC_SUBST([libxml2_inc])
AC_SUBST([libxml2_lib])

AC_SUBST([symtabAPI_platform])
AC_SUBST([xerces_platform])

AC_PROG_CC
AC_PROG_CXX

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

nobuild_cookie=.dont-build

#
# Default prefix: local subdir with host cpu-os.
#
# AC requires this to be absolute path.
#
if test "x$prefix" = xNONE ; then
    prefix="`pwd`/${host_cpu}-${host_os}"
fi

#
# Specify packages as:  --with-pkg=PATH
#
# Choices are:  build, search, PATH, or no.
#
# Internal configure variables:
#
#   pkg_avail:   build, PATH, or no, for display in summary.
#   pkg_prefix:  absolute path of final location, built or not.
#

#
# Option: --with-binutils=PATH
#
AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([ ], [ ])])
AC_ARG_WITH([binutils],
    [AS_HELP_STRING([--with-binutils=PATH],
	[path to binutils install directory (default build)])],
    [],
    [with_binutils=build])

AC_MSG_CHECKING([binutils])

binutils_avail=no
binutils_prefix=no
cookie="${srcdir}/binutils/${nobuild_cookie}"
rm -f "$cookie"
case "$with_binutils" in
    build )
	binutils_avail=build
	binutils_prefix="$prefix"
	;;
    /* )
	if test -f "${with_binutils}/include/bfd.h" ; then
	    binutils_avail="$with_binutils"
	    binutils_prefix="$with_binutils"
	else
	    AC_MSG_ERROR([invalid binutils directory: $with_binutils])
	fi
	echo "installed in: $with_binutils" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([binutils directory must be absolute path: $with_binutils])
	;;
esac

AC_MSG_RESULT([$binutils_avail])
if test "x$binutils_avail" != xbuild ; then
    AC_MSG_WARN([using non-patched binutils version])
fi

#
# Option: --with-libdwarf=PATH
#
AC_ARG_WITH([libdwarf],
    [AS_HELP_STRING([--with-libdwarf=PATH],
	[path to libdwarf install directory (default build)])],
    [],
    [with_libdwarf=build])

AC_MSG_CHECKING([libdwarf])

libdwarf_avail=no
case "$with_libdwarf" in
    build )
	libdwarf_avail=build
	;;
    search )
	for dir in /usr/local /usr
	do
	    if test -f "${dir}/include/libdwarf.h" ; then
		libdwarf_avail="$dir"
		break
	    fi
	done
	if test "$libdwarf_avail" = no ; then
	    libdwarf_avail=build
	fi
	;;
    /* )
	if test -f "${with_libdwarf}/include/libdwarf.h" ; then
	    libdwarf_avail="$with_libdwarf"
	else
	    AC_MSG_ERROR([invalid libdwarf directory: $with_libdwarf])
	fi
	;;
    no )
	;;
    * )
	AC_MSG_ERROR([libdwarf directory must be absolute path: $with_libdwarf])
	;;
esac

AC_MSG_RESULT([$libdwarf_avail])

#
# Most of this could be moved into symtabAPI's configure, especially
# if symtabAPI had a native configure script.
#
cookie="${srcdir}/libdwarf/${nobuild_cookie}"
rm -f "$cookie"
case "$libdwarf_avail" in
    build )
	libdwarf_prefix="$prefix"
	libdwarf_inc="${libdwarf_prefix}/include"
	libdwarf_lib="${libdwarf_prefix}/lib"
	;;
    /* )
	libdwarf_prefix="$libdwarf_avail"
	if test -f "${libdwarf_prefix}/include/libdwarf.h" ; then
	    libdwarf_inc="${libdwarf_prefix}/include"
	else
	    AC_MSG_ERROR([libdwarf search failed: $libdwarf_prefix])
	fi
	if test -f "${libdwarf_prefix}/lib64/libdwarf.so" ; then
	    libdwarf_lib="${libdwarf_prefix}/lib64"
	elif test -f "${libdwarf_prefix}/lib/libdwarf.so" ; then
	    libdwarf_lib="${libdwarf_prefix}/lib"
	else
	    AC_MSG_ERROR([libdwarf search failed: $libdwarf_prefix])
	fi
	echo "installed in: $libdwarf_avail" > "$cookie"
	;;
    * )
	libdwarf_prefix=no
	libdwarf_inc=no
	libdwarf_lib=no
	echo "not available" > "$cookie"
	;;
esac

#
# Option: --with-libelf=PATH
#
AC_ARG_WITH([libelf],
    [AS_HELP_STRING([--with-libelf=PATH],
	[path to libelf install directory (default build)])],
    [],
    [with_libelf=build])

valid_libelf_dir()
{
    test -f "$1/include/libelf.h" || test -f "$1/include/libelf/libelf.h"
}

AC_MSG_CHECKING([libelf])

libelf_avail=no
case "$with_libelf" in
    build )
	libelf_avail=build
	;;
    search )
	for dir in /usr/local /usr
	do
	    if valid_libelf_dir "$dir" ; then
		libelf_avail="$dir"
		break
	    fi
	done
	if test "$libelf_avail" = no ; then
	    libelf_avail=build
	fi
	;;
    /* )
	if valid_libelf_dir "$with_libelf" ; then
	    libelf_avail="$with_libelf"
	else
	    AC_MSG_ERROR([invalid libelf directory: $with_libelf])
	fi
	;;
    no )
	;;
    * )
	AC_MSG_ERROR([libelf directory must be absolute path: $with_libelf])
	;;
esac

AC_MSG_RESULT([$libelf_avail])

#
# Most of this could be moved into symtabAPI's configure, especially
# if symtabAPI had a native configure script.
#
cookie="${srcdir}/libelf/${nobuild_cookie}"
rm -f "$cookie"
case "$libelf_avail" in
    build )
	libelf_prefix="$prefix"
	libelf_inc="${libelf_prefix}/include/libelf"
	libelf_lib="${libelf_prefix}/lib"
	;;
    /* )
	libelf_prefix="$libelf_avail"
	if test -f "${libelf_prefix}/include/libelf.h" ; then
	    libelf_inc="${libelf_prefix}/include"
	elif test -f "${libelf_prefix}/include/libelf/libelf.h" ; then
	    libelf_inc="${libelf_prefix}/include/libelf"
	else
	    AC_MSG_ERROR([libelf search failed: $libelf_prefix])
	fi
	if test -f "${libelf_prefix}/lib64/libelf.so" ; then
	    libelf_lib="${libelf_prefix}/lib64"
	elif test -f "${libelf_prefix}/lib/libelf.so" ; then
	    libelf_lib="${libelf_prefix}/lib"
	else
	    AC_MSG_ERROR([libelf search failed: $libelf_prefix])
	fi
	echo "installed in: $libelf_avail" > "$cookie"
	;;
    * )
	libelf_prefix=no
	libelf_inc=no
	libelf_lib=no
	echo "not available" > "$cookie"
	;;
esac

#
# Option: --with-libmonitor=PATH
#
AC_ARG_WITH([libmonitor],
    [AS_HELP_STRING([--with-libmonitor=PATH],
	[path to libmonitor install directory (default build)])],
    [],
    [with_libmonitor=build])

AC_MSG_CHECKING([libmonitor])

libmonitor_avail=no
libmonitor_prefix=no
cookie="${srcdir}/libmonitor/${nobuild_cookie}"
rm -f "$cookie"
case "$with_libmonitor" in
    build )
	libmonitor_avail=build
	libmonitor_prefix="$prefix"
	;;
    /* )
	if test -f "${with_libmonitor}/include/monitor.h" ; then
	    libmonitor_avail="$with_libmonitor"
	    libmonitor_prefix="$with_libmonitor"
	else
	    AC_MSG_ERROR([invalid libmonitor directory: $with_libmonitor])
	fi
	echo "installed in: $with_libmonitor" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([libmonitor directory must be absolute path: $with_libmonitor])
	;;
esac

AC_MSG_RESULT([$libmonitor_avail])

#
# Option: --with-libxml2=PATH
#
AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2=PATH],
	[path to libxml2 install directory (default build)])],
    [],
    [with_libxml2=build])

valid_libxml2_dir()
{
    test -d "$1/include/libxml2"
}

AC_MSG_CHECKING([libxml2])

libxml2_avail=no
case "$with_libxml2" in
    build )
	libxml2_avail=build
	;;
    search )
	for dir in /usr/local /usr
	do
	    if valid_libxml2_dir "$dir" ; then
		libxml2_avail="$dir"
		break
	    fi
	done
	if test "$libxml2_avail" = no ; then
	    libxml2_avail=build
	fi
	;;
    /* )
	if valid_libxml2_dir "$with_libxml2" ; then
	    libxml2_avail="$with_libxml2"
	else
	    AC_MSG_ERROR([invalid libxml2 directory: $with_libxml2])
	fi
	;;
    no )
	;;
    * )
	AC_MSG_ERROR([libxml2 directory must be absolute path: $with_libxml2])
	;;
esac

AC_MSG_RESULT([$libxml2_avail])

#
# Most of this could be moved into symtabAPI's configure, especially
# if symtabAPI had a native configure script.
#
cookie="${srcdir}/libxml2/${nobuild_cookie}"
rm -f "$cookie"
case "$libxml2_avail" in
    build )
	libxml2_prefix="$prefix"
	libxml2_inc="${libxml2_prefix}/include/libxml2"
	libxml2_lib="${libxml2_prefix}/lib"
	;;
    /* )
	libxml2_prefix="$libxml2_avail"
	libxml2_inc="${libxml2_prefix}/include/libxml2"
	if test -f "${libxml2_prefix}/lib64/libxml2.so" ; then
	    libxml2_lib="${libxml2_prefix}/lib64"
	elif test -f "${libxml2_prefix}/lib/libxml2.so" ; then
	    libxml2_lib="${libxml2_prefix}/lib"
	else
	    AC_MSG_ERROR([libxml2 search failed: $libxml2_prefix])
	fi
	echo "installed in: $libxml2_avail" > "$cookie"
	;;
    * )
	libxml2_prefix=no
	libxml2_inc=no
	libxml2_lib=no
	echo "not available" > "$cookie"
	;;
esac

#
# Option: --with-old-monitor=PATH
#
AC_ARG_WITH([old-monitor],
    [AS_HELP_STRING([--with-old-monitor=PATH],
	[path to open analysis install directory (default build)])],
    [],
    [with_old_monitor=build])

AC_MSG_CHECKING([old-monitor])

old_monitor_avail=no
old_monitor_prefix=no
cookie="${srcdir}/old-monitor/${nobuild_cookie}"
rm -f "$cookie"
case "$with_old_monitor" in
    build )
	old_monitor_avail=build
	old_monitor_prefix="${prefix}/old-monitor"
	;;
    /* )
	if test -d "${with_old_monitor}/include/monitor.h" ; then
	    old_monitor_avail="$with_old_monitor"
	    old_monitor_prefix="$with_old_monitor"
	else
	    AC_MSG_ERROR([invalid old-monitor directory: $with_old_monitor])
	fi
	echo "installed in: $with_old_monitor" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([old-monitor directory must be absolute path: $with_old_monitor])
	;;
esac

AC_MSG_RESULT([$old_monitor_avail])

#
# Option: --with-open-analysis=PATH
#
AC_ARG_WITH([open-analysis],
    [AS_HELP_STRING([--with-open-analysis=PATH],
	[path to open analysis install directory (default build)])],
    [],
    [with_open_analysis=build])

AC_MSG_CHECKING([open-analysis])

open_analysis_avail=no
open_analysis_prefix=no
cookie="${srcdir}/open-analysis/${nobuild_cookie}"
rm -f "$cookie"
case "$with_open_analysis" in
    build )
	open_analysis_avail=build
	open_analysis_prefix="$prefix"
	;;
    /* )
	if test -d "${with_open_analysis}/include/OpenAnalysis" ; then
	    open_analysis_avail="$with_open_analysis"
	    open_analysis_prefix="$with_open_analysis"
	else
	    AC_MSG_ERROR([invalid open-analysis directory: $with_open_analysis])
	fi
	echo "installed in: $with_open_analysis" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([open-analysis directory must be absolute path: $with_open_analysis])
	;;
esac

AC_MSG_RESULT([$open_analysis_avail])

#
# Option: --with-symtabAPI=PATH
#
AC_ARG_WITH([symtabAPI],
    [AS_HELP_STRING([--with-symtabAPI=PATH],
	[path to symtabAPI install directory (default build)])],
    [],
    [with_symtabAPI=build])

AC_MSG_CHECKING([symtabAPI])

symtabAPI_avail=no
symtabAPI_prefix=no
symtabAPI_platform=no
cookie="${srcdir}/symtabAPI/${nobuild_cookie}"
rm -f "$cookie"
case "$with_symtabAPI" in
    build )
	#
	# SymtabAPI understands a fixed set of platforms, and we only
	# handle a subset of those.  This is also why you should not
	# invent non-standard platform names (yuck).
	#
	case "$host_cpu" in
	    *x86*64* )
		symtabAPI_platform=x86_64-unknown-linux2.4
		;;
	    *i*86* | *x86* )
		symtabAPI_platform=i386-unknown-linux2.4
		;;
	    *ia64* )
		symtabAPI_platform=ia64-unknown-linux2.4
		;;
	    *power*64* | *ppc*64* )
		symtabAPI_platform=ppc64_linux
		;;
	    *power* | *ppc* )
		symtabAPI_platform=ppc32_linux
		;;
	esac
	if test "$symtabAPI_platform" != no ; then
	    symtabAPI_avail=build
	    symtabAPI_prefix="$prefix"
	else
	    echo "not available on this platform" > "$cookie"
	fi
	;;
    /* )
	if test -f "${with_symtabAPI}/include/Symtab.h" ; then
	    symtabAPI_avail="$with_symtabAPI"
	    symtabAPI_prefix="$with_symtabAPI"
	else
	    AC_MSG_ERROR([invalid symtabAPI directory: $with_symtabAPI])
	fi
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([symtabAPI directory must be absolute path: $with_symtabAPI])
	;;
esac

AC_MSG_RESULT([$symtabAPI_avail])

#
# Option: --with-xed2=PATH
#
# Used on x86 and x86_64.
#
AC_ARG_WITH([xed2],
    [AS_HELP_STRING([--with-xed2=PATH],
	[path to xed2 install directory (default build)])],
    [],
    [with_xed2=build])

AC_MSG_CHECKING([xed2])

xed2_avail=no
xed2_prefix=no
cookie="${srcdir}/xed2/${nobuild_cookie}"
rm -f "$cookie"
case "$with_xed2" in
    build )
	case "$host_cpu" in
	    *x86* | *i*86* )
		xed2_avail=build
		xed2_prefix="$prefix"
		;;
	    * )
		echo "not used on this platform" > "$cookie"
		;;
	esac
	;;
    /* )
	if test -f "${with_xed2}/include/xed-interface.h" ; then
	    xed2_avail="$with_xed2"
	    xed2_prefix="$with_xed2"
	else
	    AC_MSG_ERROR([invalid xed2 directory: $with_xed2])
	fi
	echo "installed in: $with_xed2" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([xed2 directory must be absolute path: $with_xed2])
	;;
esac

AC_MSG_RESULT([$xed2_avail])

#
# Option: --with-xerces=PATH
#
AC_ARG_WITH([xerces],
    [AS_HELP_STRING([--with-xerces=PATH],
	[path to xerces/c install directory (default build)])],
    [],
    [with_xerces=build])

AC_MSG_CHECKING([xerces])

xerces_avail=no
xerces_prefix=no
cookie="${srcdir}/xerces/${nobuild_cookie}"
rm -f "$cookie"
case "$with_xerces" in
    build )
	xerces_avail=build
	xerces_prefix="$prefix"
	;;
    /* )
	if test -f "${with_xerces}/include/xercesc/dom/DOM.hpp" ; then
	    xerces_avail="$with_xerces"
	    xerces_prefix="$with_xerces"
	else
	    AC_MSG_ERROR([invalid xerces directory: $with_xerces])
	fi
	echo "installed in: $with_xerces" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([xerces directory must be absolute path: $with_xerces])
	;;
esac

AC_MSG_RESULT([$xerces_avail])

xerces_platform=linux

AC_OUTPUT

#
# Display a summary of the config options.
#
AC_MSG_NOTICE([])
AC_MSG_NOTICE([Externals summary])
AC_MSG_NOTICE([prefix: $prefix])
if test "x$build" != "x$host" ; then
    AC_MSG_NOTICE([build: $build])
fi
AC_MSG_NOTICE([host: $host])
AC_MSG_NOTICE([CC, CFLAGS: $CC $CFLAGS])
AC_MSG_NOTICE([CXX, CXXFLAGS: $CXX $CXXFLAGS])
AC_MSG_NOTICE([binutils: $binutils_avail])
AC_MSG_NOTICE([libdwarf: $libdwarf_avail])
AC_MSG_NOTICE([libelf: $libelf_avail])
AC_MSG_NOTICE([libmonitor: $libmonitor_avail])
AC_MSG_NOTICE([libxml2: $libxml2_avail])
AC_MSG_NOTICE([old-monitor: $old_monitor_avail])
AC_MSG_NOTICE([open-analysis: $open_analysis_avail])
AC_MSG_NOTICE([symtabAPI: $symtabAPI_avail])
AC_MSG_NOTICE([xed2: $xed2_avail])
AC_MSG_NOTICE([xerces: $xerces_avail])

