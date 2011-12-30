dnl
dnl  Externals configure.
dnl
dnl  Copyright (c) 2008-2011, Rice University.
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

AC_INIT([hpctoolkit-externals], [5.2.1], [hpctoolkit-forum@rice.edu],
        [hpctoolkit-externals], [http://hpctoolkit.org/])
AC_COPYRIGHT([Copyright (c) 2008-2011, Rice University.
See the file LICENSE for details.])

AC_CONFIG_AUX_DIR([config])
AC_PREREQ(2.67)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([Makefile.conf])
AC_CONFIG_FILES([externs.conf])
AC_CONFIG_FILES([options.conf])

AC_SUBST([CC])
AC_SUBST([CFLAGS])
AC_SUBST([CXX])
AC_SUBST([CXXFLAGS])
AC_SUBST([AR])

AC_SUBST([build])
AC_SUBST([build_cpu])
AC_SUBST([build_os])
AC_SUBST([host])
AC_SUBST([host_cpu])
AC_SUBST([host_os])
AC_SUBST([is_cross_compile])
AC_SUBST([prefix])

AC_PROG_CC
AC_PROG_CXX

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

failure_file=BUILD-FAILURE
externs_file=externs.conf
nobuild_cookie=.dont-build
version_file="${srcdir}/.svn-revision"

#------------------------------------------------------------
# Prefix
#------------------------------------------------------------

# Allow the user to set the prefix, but change the default to a subdir
# of the build directory.  Autoconf requires this to be an absolute
# path.  Normalize the path and mkdir the directory so we can write
# externs.conf into it.

if test "x$prefix" = xNONE ; then
    prefix="`pwd`/${host}"
fi
if mkdir -p "$prefix" ; then : ; else
    AC_MSG_ERROR([unable to mkdir $prefix])
fi
prefix=`( cd "$prefix" && pwd )`

#------------------------------------------------------------
# SVN revision
#------------------------------------------------------------

# If this directory is an svn sandbox, then use svn info (and store
# the result in a file), else expect the answer to be in the file.
# Note: svn update changes the revision number, so svn info must take
# precedence over the file.

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

AC_SUBST([svn_revision])

#------------------------------------------------------------
# Compiler issues
#------------------------------------------------------------

# is_cross_compile: non-empty string if cross-compiling

is_cross_compile=""
if test "x$build" != "x$host" ; then
   is_cross_compile=${host}
fi

# Warn if not using GNU gcc and g++.  Some packages don't build
# cleanly without gcc.  For example, binutils fails with icc.
# It's a pain to robustly identify GNU gcc/g++: Intel icc defines
# __GNUC__ and Ubuntu g++ --version doesn't display "GCC".

warn_non_gcc=no
$CC --version 2>&1 | grep -i 'free software found' >/dev/null 2>&1
if test $? -ne 0 ; then
    warn_non_gcc=yes
    AC_MSG_WARN([Not using the GNU C compiler.])
fi

$CXX --version 2>&1 | grep -i 'free software found' >/dev/null 2>&1
if test $? -ne 0 ; then
    warn_non_gcc=yes
    AC_MSG_WARN([Not using the GNU C++ compiler.])
fi

# Test if CXX supports <tr1/unordered_map>.  SymtabAPI uses this with
# gcc 4.3 and later, but some installations (biou) are missing this.

AC_MSG_CHECKING([if CXX supports tr1 unordered_map])

AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([
#include <functional>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
], [ans=yes], [ans=no])
AC_LANG_POP

AC_MSG_RESULT([$ans])

have_tr1_map="$ans"
AC_SUBST([have_tr1_map])

#------------------------------------------------------------
# GNU make
#------------------------------------------------------------

# Try to find GNU make.  Amazingly, AC doesn't have a macro for this.
# We only use this value when running configure scripts directly.
# When starting from a make target, MAKE is exported to the scripts.
# Thus, it's not fatal if we can't find gnu make here, as long as you
# later run a gnu-compatible make.

AC_MSG_CHECKING([for gnu make])

if gmake --version 2>&1 | grep -i gnu >/dev/null 2>&1 ; then
    gnu_make=gmake
elif make --version 2>&1 | grep -i gnu >/dev/null 2>&1 ; then
    gnu_make=make
else
    gnu_make=no
fi

AC_MSG_RESULT([$gnu_make])

if test "$gnu_make" = no ; then
    AC_MSG_WARN([unable to find gnu make])
    gnu_make=make
fi

AC_SUBST([gnu_make])

#------------------------------------------------------------
# Wordsize: --enable-wordsize=NUM
#------------------------------------------------------------

# Use sizeof(void *) to determine if CC produces 32- or 64-bit code.
# This option is rarely needed, it exists just in case AC computes the
# wrong value.

AC_CHECK_SIZEOF([void *])

default=`expr $ac_cv_sizeof_void_p '*' 8`

AC_ARG_ENABLE([wordsize],
    [AS_HELP_STRING([--enable-wordsize=NUM],
	[host wordsize in bits, 32 or 64, only needed if autoconf
	 computes the wrong value])],
    [wordsize="$enable_wordsize"],
    [wordsize="$default"])

AC_MSG_CHECKING([host wordsize])
AC_MSG_RESULT([$wordsize bits])

# The host wordsize also sets the multilib search path.  This only
# applies to outside packages via --with-pkg (ie, not built here).

case "$wordsize" in
    64 ) multilib_path='lib64 lib' ;;
    32 ) multilib_path='lib32 lib' ;;
    * ) AC_MSG_ERROR([wordsize must be 32 or 64: $wordsize bits]) ;;
esac

AC_SUBST([wordsize])

# Check that CC and CXX produce the same wordsize.  Also check that
# CFLAGS doesn't change the wordsize.  This shouldn't be necessary,
# but some packages can't handle -m64 in CFLAGS, they require
# something like CC='gcc -m64'.

srcfile="conftest.$$.c"
objfile="conftest.$$.o"

rm -f "$srcfile"
cat >"$srcfile" <<EOF
int foo(void)
{ return 2; }
EOF

get_wordsize()
{
    output=`file $objfile`
    case "$output" in
	*64-bit* ) ans=64 ;;
	*32-bit* ) ans=32 ;;
	* ) ans=0 ;;
    esac
    echo $ans
}

AC_MSG_CHECKING([host wordsize from CC])
rm -f "$objfile"
$CC -c -o "$objfile" "$srcfile" >/dev/null
wordsize_cc=`get_wordsize`
AC_MSG_RESULT([$wordsize_cc bits])

AC_MSG_CHECKING([host wordsize from CXX])
rm -f "$objfile"
$CXX -c -o "$objfile" "$srcfile" >/dev/null
wordsize_cxx=`get_wordsize`
AC_MSG_RESULT([$wordsize_cxx bits])

rm -f "$objfile"
$CC $CFLAGS -c -o "$objfile" "$srcfile" >/dev/null
wordsize_cflags=`get_wordsize`

rm -f "$objfile"
$CXX $CXXFLAGS -c -o "$objfile" "$srcfile" >/dev/null
wordsize_cxxflags=`get_wordsize`

rm -f "$srcfile" "$objfile"

warn_wordsize_mismatch=no
if test "$wordsize" != "$wordsize_cc" \
    || test "$wordsize" != "$wordsize_cxx" \
    || test "$wordsize" != "$wordsize_cflags" \
    || test "$wordsize" != "$wordsize_cxxflags"
then
    AC_MSG_WARN([CC or CXX generate inconsistent wordsizes])
    warn_wordsize_mismatch=yes
fi

#------------------------------------------------------------
# Install directory: --enable-subdirs
#------------------------------------------------------------

# Install packages into: (1) a single, unified directory with shared
# bin, include, lib dirs, or (2) separate subidrs, one for each
# package.  Old monitor always uses a separate subdir due to name
# conflicts.

AC_ARG_ENABLE([subdirs],
    [AS_HELP_STRING([--enable-subdirs],
	[install packages into separate subdirs (default yes)])],
    [],
    [enable_subdirs=yes])

AC_MSG_CHECKING([install directory])

if test "x$enable_subdirs" = xyes ; then
    binutils_local_prefix="${prefix}/binutils"
    libdwarf_local_prefix="${prefix}/libdwarf"
    libelf_local_prefix="${prefix}/libelf"
    libmonitor_local_prefix="${prefix}/libmonitor"
    libunwind_local_prefix="${prefix}/libunwind"
    libxml2_local_prefix="${prefix}/libxml2"
    old_monitor_local_prefix="${prefix}/old-monitor"
    open_analysis_local_prefix="${prefix}/open-analysis"
    symtabAPI_local_prefix="${prefix}/symtabAPI"
    xed2_local_prefix="${prefix}/xed2"
    xerces_local_prefix="${prefix}/xerces"
    enable_subdirs_mesg=separate
else
    binutils_local_prefix="$prefix"
    libdwarf_local_prefix="$prefix"
    libelf_local_prefix="$prefix"
    libmonitor_local_prefix="$prefix"
    libunwind_local_prefix="$prefix"
    libxml2_local_prefix="$prefix"
    old_monitor_local_prefix="${prefix}/old-monitor"
    open_analysis_local_prefix="$prefix"
    symtabAPI_local_prefix="$prefix"
    xed2_local_prefix="$prefix"
    xerces_local_prefix="$prefix"
    enable_subdirs_mesg=shared
fi

AC_MSG_RESULT([$enable_subdirs_mesg])

#------------------------------------------------------------
# Copy skeleton directories
#------------------------------------------------------------

# Support a separate build directory by copying the package skeleton
# directories.  Just copy regular files and the patches subdir, don't
# copy other subdirs.

packages='binutils libdwarf libelf libmonitor libunwind libxml2
old-monitor open-analysis symtabAPI xed2 xerces'

if test "x$srcdir" != x.
then
    AC_MSG_CHECKING([copying package directories])
    for pkg in $packages build-utils
    do
        if mkdir -p "$pkg" ; then : ; else
	    AC_MSG_ERROR([unable to mkdir $pkg])
	fi
	for file in "${srcdir}/${pkg}"/*
	do
	    if test -f "$file" ; then
		if cp -f "$file" "$pkg" ; then : ; else
		    AC_MSG_ERROR([unable to copy $file])
		fi
	    fi
	done
	for dir in "${srcdir}/${pkg}"/patch*
	do
	    if test -d "$dir" ; then
		base=`basename "$dir"`
		if mkdir -p "${pkg}/${base}" ; then : ; else
		    AC_MSG_ERROR([unable to mkdir $pkg/$base])
		fi
		for file in "$dir"/*
		do
		    if test -f "$file" ; then
			if cp -f "$file" "${pkg}/${base}" ; then : ; else
			    AC_MSG_ERROR([unable to copy $file])
			fi
		    fi
		done
	    fi
	done
    done

    # Symlink the distfiles directory.
    if test ! -d distfiles ; then
	if ln -s "${srcdir}/distfiles" distfiles ; then : ; else
	    AC_MSG_ERROR([unable to symlink distfiles])
	fi
    fi

    AC_MSG_RESULT([done])
fi

#------------------------------------------------------------
# Binutils: --with-binutils=PATH
#------------------------------------------------------------

AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([ ], [ ])])
AC_ARG_WITH([binutils],
    [AS_HELP_STRING([--with-binutils=PATH],
	[path to binutils install directory (default build)])],
    [],
    [with_binutils=build])

AC_MSG_CHECKING([binutils])

binutils_avail=no
binutils_prefix=no
cookie="binutils/${nobuild_cookie}"
rm -f "$cookie"

case "$with_binutils" in
    build | yes )
	binutils_avail=build
	binutils_prefix="$binutils_local_prefix"
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
if test "x$binutils_avail" != xbuild && test "x$binutils_avail" != xno ; then
    AC_MSG_WARN([using non-patched binutils version])
fi

AC_SUBST([binutils_prefix])

#------------------------------------------------------------
# Libdwarf: --with-libdwarf=PATH
#------------------------------------------------------------

AC_ARG_WITH([libdwarf],
    [AS_HELP_STRING([--with-libdwarf=PATH],
	[path to libdwarf install directory (default build)])],
    [],
    [with_libdwarf=build])

AC_MSG_CHECKING([libdwarf])

libdwarf_avail=no
libdwarf_prefix=no
libdwarf_inc=no
libdwarf_lib=no
cookie="libdwarf/${nobuild_cookie}"
rm -f "$cookie"

case "$with_libdwarf" in
    build | yes )
	libdwarf_avail=build
	libdwarf_prefix="$libdwarf_local_prefix"
	libdwarf_inc="${libdwarf_prefix}/include"
	libdwarf_lib="${libdwarf_prefix}/lib"
	;;
    /* )
	libdwarf_avail="$with_libdwarf"
	libdwarf_prefix="$with_libdwarf"
	if test -f "${libdwarf_prefix}/include/libdwarf.h" ; then
	    libdwarf_inc="${libdwarf_prefix}/include"
	else
	    AC_MSG_ERROR([invalid libdwarf directory: $with_libdwarf])
	fi
	for lib in $multilib_path fail ; do
	    if test -f "${libdwarf_prefix}/${lib}/libdwarf.so" ; then
		libdwarf_lib="${libdwarf_prefix}/${lib}"
		break
	    fi
	    if test "$lib" = fail ; then
		AC_MSG_ERROR([invalid libdwarf directory: $with_libdwarf])
	    fi
	done
	echo "installed in: $libdwarf_avail" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([libdwarf directory must be absolute path: $with_libdwarf])
	;;
esac

AC_MSG_RESULT([$libdwarf_avail])

AC_SUBST([libdwarf_prefix])
AC_SUBST([libdwarf_inc])
AC_SUBST([libdwarf_lib])

#------------------------------------------------------------
# Libelf: --with-libelf=PATH
#------------------------------------------------------------

# FIXME: libelf installs and includes its header files as
# <libelf/libelf.h> and not <libelf.h>.  It's a mess to support both
# (libdwarf handles this, but symtabAPI does not), so we follow
# libelf's usage.

AC_ARG_WITH([libelf],
    [AS_HELP_STRING([--with-libelf=PATH],
	[path to libelf install directory (default build)])],
    [],
    [with_libelf=build])

AC_MSG_CHECKING([libelf])

libelf_avail=no
libelf_prefix=no
libelf_inc=no
libelf_lib=no
cookie="libelf/${nobuild_cookie}"
rm -f "$cookie"

case "$with_libelf" in
    build | yes )
	libelf_avail=build
	libelf_prefix="$libelf_local_prefix"
	libelf_inc="${libelf_prefix}/include"
	libelf_lib="${libelf_prefix}/lib"
	;;
    /* )
	libelf_avail="$with_libelf"
	libelf_prefix="$with_libelf"
	if test -f "${libelf_prefix}/include/libelf/libelf.h" ; then
	    libelf_inc="${libelf_prefix}/include"
	else
	    AC_MSG_ERROR([unable to find <libelf/libelf.h> in: $libelf_prefix])
	fi
	for lib in $multilib_path fail ; do
	    if test -f "${libelf_prefix}/${lib}/libelf.so" ; then
		libelf_lib="${libelf_prefix}/${lib}"
		break
	    fi
	    if test "$lib" = fail ; then
		AC_MSG_ERROR([invalid libelf directory: $libelf_prefix])
	    fi
	done
	echo "installed in: $libelf_avail" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([libelf directory must be absolute path: $with_libelf])
	;;
esac

AC_MSG_RESULT([$libelf_avail])

AC_SUBST([libelf_prefix])
AC_SUBST([libelf_inc])
AC_SUBST([libelf_lib])

#------------------------------------------------------------
# Libmonitor: --with-libmonitor=PATH
#------------------------------------------------------------

AC_ARG_WITH([libmonitor],
    [AS_HELP_STRING([--with-libmonitor=PATH],
	[path to libmonitor install directory (default build)])],
    [],
    [with_libmonitor=build])

AC_MSG_CHECKING([libmonitor])

libmonitor_avail=no
libmonitor_prefix=no
cookie="libmonitor/${nobuild_cookie}"
rm -f "$cookie"

case "$with_libmonitor" in
    build | yes )
	libmonitor_avail=build
	libmonitor_prefix="$libmonitor_local_prefix"
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

AC_SUBST([libmonitor_prefix])

#------------------------------------------------------------
# Libunwind: --with-libunwind=PATH
#------------------------------------------------------------

# Now used all platforms.

# The 32-bit version of libunwind doesn't build correctly on x86-64
# due to some cross-compile issues, so disable that choice for now.
# This is only the default, you can reenable with --with-libunwind.

default=build
case "$host_cpu" in
    *x86*64* )
	if test "$wordsize" = 32 ; then
	    default=no
	fi
	;;
esac

AC_ARG_WITH([libunwind],
    [AS_HELP_STRING([--with-libunwind=PATH],
	[path to libunwind install directory (default build)])],
    [],
    [with_libunwind="$default"])

AC_MSG_CHECKING([libunwind])

libunwind_avail=no
libunwind_prefix=no
libunwind_inc=no
libunwind_lib=no
cookie="libunwind/${nobuild_cookie}"
rm -f "$cookie"

case "$with_libunwind" in
    build | yes )
	libunwind_avail=build
	libunwind_prefix="$libunwind_local_prefix"
	libunwind_inc="${libunwind_prefix}/include"
	libunwind_lib="${libunwind_prefix}/lib"
	;;
    /* )
	libunwind_avail="$with_libunwind"
	libunwind_prefix="$with_libunwind"
	if test -f "${libunwind_prefix}/include/libunwind.h" ; then
	    libunwind_inc="${libunwind_prefix}/include"
	else
	    AC_MSG_ERROR([invalid libunwind: $libunwind_prefix])
	fi
	for lib in $multilib_path fail ; do
	    if test -f "${libunwind_prefix}/${lib}/libunwind.so" ; then
		libunwind_lib="${libunwind_prefix}/${lib}"
		break
	    fi
	    if test "$lib" = fail ; then
		AC_MSG_ERROR([invalid libunwind directory: $with_libunwind])
	    fi
	done
	echo "installed in: $libunwind_avail" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([libunwind directory must be absolute path: $with_libunwind])
	;;
esac

AC_MSG_RESULT([$libunwind_avail])

AC_SUBST([libunwind_prefix])
AC_SUBST([libunwind_inc])
AC_SUBST([libunwind_lib])

#------------------------------------------------------------
# Libxml2: --with-libxml2=PATH
#------------------------------------------------------------

AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2=PATH],
	[path to libxml2 install directory (default build)])],
    [],
    [with_libxml2=build])

AC_MSG_CHECKING([libxml2])

libxml2_avail=no
libxml2_prefix=no
libxml2_inc=no
libxml2_lib=no
cookie="libxml2/${nobuild_cookie}"
rm -f "$cookie"

case "$with_libxml2" in
    build | yes )
	libxml2_avail=build
	libxml2_prefix="$libxml2_local_prefix"
	libxml2_inc="${libxml2_prefix}/include/libxml2"
	libxml2_lib="${libxml2_prefix}/lib"
	;;
    /* )
	libxml2_avail="$with_libxml2"
	libxml2_prefix="$with_libxml2"
	if test -d "${with_libxml2}/include/libxml2" ; then
	    libxml2_inc="${libxml2_prefix}/include/libxml2"
	else
	    AC_MSG_ERROR([invalid libxml2 directory: $with_libxml2])
	fi
	for lib in $multilib_path fail ; do
	    if test -f "${libxml2_prefix}/${lib}/libxml2.so" ; then
		libxml2_lib="${libxml2_prefix}/${lib}"
		break
	    fi
	    if test "$lib" = fail ; then
		AC_MSG_ERROR([invalid libxml2 directory: $with_libxml2])
	    fi
	done
	echo "installed in: $libxml2_avail" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([libxml2 directory must be absolute path: $with_libxml2])
	;;
esac

AC_MSG_RESULT([$libxml2_avail])

AC_SUBST([libxml2_prefix])
AC_SUBST([libxml2_inc])
AC_SUBST([libxml2_lib])

#------------------------------------------------------------
# Old Monitor: --with-old-monitor=PATH
#------------------------------------------------------------

# Set the default to no on Cray XT, the build fails there, and we
# don't use it on this platform anyway.

default=build
if test "x$XTOS_VERSION" != x ; then
    default=no
fi

AC_ARG_WITH([old-monitor],
    [AS_HELP_STRING([--with-old-monitor=PATH],
	[path to old monitor install directory (default build)])],
    [],
    [with_old_monitor=$default])

AC_MSG_CHECKING([old-monitor])

old_monitor_avail=no
old_monitor_prefix=no
cookie="old-monitor/${nobuild_cookie}"
rm -f "$cookie"

case "$with_old_monitor" in
    build | yes )
	old_monitor_avail=build
	old_monitor_prefix="$old_monitor_local_prefix"
	;;
    /* )
	if test -f "${with_old_monitor}/include/monitor.h" ; then
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

AC_SUBST([old_monitor_prefix])

#------------------------------------------------------------
# Open Analysis: --with-open-analysis=PATH
#------------------------------------------------------------

AC_ARG_WITH([open-analysis],
    [AS_HELP_STRING([--with-open-analysis=PATH],
	[path to open analysis install directory (default build)])],
    [],
    [with_open_analysis=build])

AC_MSG_CHECKING([open-analysis])

open_analysis_avail=no
open_analysis_prefix=no
cookie="open-analysis/${nobuild_cookie}"
rm -f "$cookie"

case "$with_open_analysis" in
    build | yes )
	open_analysis_avail=build
	open_analysis_prefix="$open_analysis_local_prefix"
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

AC_SUBST([open_analysis_prefix])

#------------------------------------------------------------
# SymtabAPI: --with-symtabAPI=PATH
#------------------------------------------------------------

AC_ARG_WITH([symtabAPI],
    [AS_HELP_STRING([--with-symtabAPI=PATH],
	[path to symtabAPI install directory (default build)])],
    [],
    [with_symtabAPI=build])

AC_MSG_CHECKING([symtabAPI])

symtabAPI_avail=no
symtabAPI_prefix=no
symtabAPI_platform=no
cookie="symtabAPI/${nobuild_cookie}"
rm -f "$cookie"

case "$with_symtabAPI" in
    build | yes )
	#
	# SymtabAPI understands a fixed set of platforms, and we only
	# handle a subset of those.  This is also why you should not
	# invent non-standard platform names (yuck).
	#
	# On x86 and powerpc, choose the platform based on the object
	# file's wordsize, not the host wordsize.  This allows
	# building a 32-bit version for a 64-bit host.  On ia64 and
	# mips, symtabAPI only has a 64-bit version.
	#
	case "$host_cpu" in
	    *i*86* | *x86* )
		case "$wordsize" in
		    64 ) symtabAPI_platform=x86_64-unknown-linux2.4 ;;
		    32 ) symtabAPI_platform=i386-unknown-linux2.4 ;;
		esac
		;;
	    *ia64* )
		symtabAPI_platform=ia64-unknown-linux2.4
		;;
	    *mips64* )
		symtabAPI_platform=mips64_linux
		;;
	    *power* | *ppc* )
		case "$wordsize" in
		    64 ) symtabAPI_platform=ppc64_linux ;;
		    32 ) symtabAPI_platform=ppc32_linux ;;
		esac
		;;
	esac
	if test "$symtabAPI_platform" != no ; then
	    symtabAPI_avail=build
	    symtabAPI_prefix="$symtabAPI_local_prefix"
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
	echo "installed in: $symtabAPI_avail" > "$cookie"
	;;
    no )
	echo "not available" > "$cookie"
	;;
    * )
	AC_MSG_ERROR([symtabAPI directory must be absolute path: $with_symtabAPI])
	;;
esac

AC_MSG_RESULT([$symtabAPI_avail])

AC_SUBST([symtabAPI_prefix])
AC_SUBST([symtabAPI_platform])

#------------------------------------------------------------
# XED: --with-xed2=PATH
#------------------------------------------------------------

# Used on x86 and x86_64.

default=no
case "$host_cpu" in
    *x86* | *i*86* )
	default=build
	;;
esac

AC_ARG_WITH([xed2],
    [AS_HELP_STRING([--with-xed2=PATH],
	[path to xed2 install directory (default build)])],
    [],
    [with_xed2="$default"])

AC_MSG_CHECKING([xed2])

xed2_avail=no
xed2_prefix=no
cookie="xed2/${nobuild_cookie}"
rm -f "$cookie"

case "$with_xed2" in
    build | yes )
	xed2_avail=build
	xed2_prefix="$xed2_local_prefix"
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

AC_SUBST([xed2_prefix])

#------------------------------------------------------------
# Xerces: --with-xerces=PATH
#------------------------------------------------------------

AC_ARG_WITH([xerces],
    [AS_HELP_STRING([--with-xerces=PATH],
	[path to xerces/c install directory (default build)])],
    [],
    [with_xerces=build])

AC_MSG_CHECKING([xerces])

xerces_avail=no
xerces_prefix=no
cookie="xerces/${nobuild_cookie}"
rm -f "$cookie"

case "$with_xerces" in
    build | yes )
	xerces_avail=build
	xerces_prefix="$xerces_local_prefix"
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

AC_SUBST([xerces_prefix])
AC_SUBST([xerces_platform])

#------------------------------------------------------------
# MPI: --with-mpi=PATH
#------------------------------------------------------------

# Pass the MPI path to old monitor.

AC_ARG_WITH([mpi],
    [AS_HELP_STRING([--with-mpi=PATH],
	[pass the MPI path to old monitor,
	 not used by libmonitor (default no)])],
    [],
    [with_mpi=no])

case "$with_mpi" in
    /* )
	if test ! -f "${with_mpi}/include/mpi.h" ; then
	    AC_MSG_ERROR([invalid mpi directory: $with_mpi])
	fi
	;;
    no )
	;;
    * )
	AC_MSG_ERROR([mpi directory must be absolute path: $with_mpi])
	;;
esac

AC_SUBST([with_mpi])

#------------------------------------------------------------
# Summary
#------------------------------------------------------------

AC_OUTPUT

# Copy externs.conf into prefix dir, this must come after AC_OUTPUT.
if cp -f "$externs_file" "$prefix" ; then : ; else
    AC_MSG_ERROR([unable to copy $externs_file])
fi

rm -f "$failure_file"

AC_MSG_NOTICE([------------------])
AC_MSG_NOTICE([Externals summary])
AC_MSG_NOTICE([------------------])

if test "$warn_non_gcc" = yes ; then
    AC_MSG_WARN([Not using the GNU C or C++ compiler.])
    AC_MSG_WARN([This is not necessarily an error, but some packages may not])
    AC_MSG_WARN([compile cleanly without gcc or g++.  If the build fails, try])
    AC_MSG_WARN([using GNU gcc and g++ before reporting an error.])
    AC_MSG_NOTICE([--------------------------])
fi

if test "$warn_wordsize_mismatch" = yes ; then
    AC_MSG_WARN([Host wordsize mismatch.])
    AC_MSG_WARN([Either CC ($wordsize_cc) and CXX ($wordsize_cxx) generate different wordsizes,])
    AC_MSG_WARN([or they do not match the host wordsize ($wordsize), or they generate])
    AC_MSG_WARN([different wordsizes with and without CFLAGS.  If you are])
    AC_MSG_WARN([using a CFLAGS option to reset the wordsize, then put this])
    AC_MSG_WARN([option in CC with something like CC='gcc -m${wordsize}'.])
    AC_MSG_NOTICE([--------------------------])
fi

AC_MSG_NOTICE([svn revision: $svn_revision])
AC_MSG_NOTICE([prefix: $prefix])
AC_MSG_NOTICE([build: $build])
AC_MSG_NOTICE([host:  $host])
AC_MSG_NOTICE([CC:  '$CC',  CFLAGS:   '$CFLAGS'])
AC_MSG_NOTICE([CXX: '$CXX',  CXXFLAGS: '$CXXFLAGS'])
AC_MSG_NOTICE([AR:  '$AR'])
AC_MSG_NOTICE([wordsize: $wordsize bits])
AC_MSG_NOTICE([binutils: $binutils_avail])
AC_MSG_NOTICE([libdwarf: $libdwarf_avail])
AC_MSG_NOTICE([libelf: $libelf_avail])
AC_MSG_NOTICE([libmonitor: $libmonitor_avail])
AC_MSG_NOTICE([libunwind: $libunwind_avail])
AC_MSG_NOTICE([libxml2: $libxml2_avail])
AC_MSG_NOTICE([old-monitor: $old_monitor_avail])
AC_MSG_NOTICE([open-analysis: $open_analysis_avail])
AC_MSG_NOTICE([symtabAPI: $symtabAPI_avail])
AC_MSG_NOTICE([xed2: $xed2_avail])
AC_MSG_NOTICE([xerces: $xerces_avail])
AC_MSG_NOTICE([mpi: $with_mpi])

