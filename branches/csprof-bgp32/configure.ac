dnl
dnl  Csprof top-level configure.ac
dnl
dnl  $Id: configure.ac 153 2008-06-27 19:23:27Z krentel $
dnl

AC_INIT([csprof], [0.x])

AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_FILES([src/scripts-new/csprof], [chmod 755 src/scripts-new/csprof])
AC_CONFIG_FILES([src/scripts-new/csprof_syms], [chmod 755 src/scripts-new/csprof_syms])
AC_CONFIG_FILES([src/scripts-new/csprof-link], [chmod 755 src/scripts-new/csprof-link])

AC_SUBST([csprof_extern_includes])
AC_SUBST([libmonitor_prefix])
AC_SUBST([symbols_prefix])
AC_SUBST([symtabAPI_prefix])
AC_SUBST([xed2_prefix])
AC_SUBST([libcpp_path])
AC_SUBST([papi_prefix])
AC_SUBST([papi_libs])

#
# Disable libtool's static library.  Enable_static only applies to
# libtool's .a file, which we don't use.  The csprof static case needs
# a .o file, and automake builds that as a program, not a library.
# This needs to come before PROG_LIBTOOL.
#
enable_static=no

AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AC_PROG_AWK
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

#
# Returns: true if an item ($1) is a member of the space-separated
# list ($2).
#
is_member()
{
    item="$1"
    for elt in $2
    do
        if test "x$elt" = "x$item" ; then
	    return 0
	fi
    done
    return 1
}

csprof_extern_includes=

#
# CPU type.
#
csprof_is_ppc=no
csprof_is_x86=no
case "$host_cpu" in
    *ppc* | *Ppc* | *PPC* | *power*pc* | *Power*pc* | *POWER*PC* )
	csprof_is_ppc=yes
	;;
    *86* )
	csprof_is_x86=yes
	;;
    * )
	AC_MSG_ERROR([unknown cpu type: $host_cpu])
	;;
esac

AM_CONDITIONAL([CSPROF_AMTEST_IS_PPC],
    [test x$csprof_is_ppc = xyes])
AM_CONDITIONAL([CSPROF_AMTEST_IS_X86],
    [test x$csprof_is_x86 = xyes])

#
# OS type.
#
csprof_is_linux=no
case "$host_os" in
    *linux* | *Linux* | *LINUX* )
	csprof_is_linux=yes
	;;
    * )
	AC_MSG_ERROR([unknown os type: $host_os])
	;;
esac

AM_CONDITIONAL([CSPROF_AMTEST_IS_LINUX],
    [test x$csprof_is_linux = xyes])

#
# Option: --with-libmonitor=PATH
#
AC_ARG_WITH([dummy-blank-line], [AS_HELP_STRING([ ], [ ])])
AC_ARG_WITH([libmonitor],
    [AS_HELP_STRING([--with-libmonitor=PATH],
        [path to libmonitor install directory (required)])],
    [],
    [with_libmonitor=no])

case "$with_libmonitor" in
    /* )
	if test ! -f "${with_libmonitor}/include/monitor.h" ; then
	    AC_MSG_ERROR([invalid libmonitor directory: $with_libmonitor])
	fi
	;;
    no )
	AC_MSG_ERROR([must set --with-libmonitor=/path/to/libmonitor])
	;;
    * )
	AC_MSG_ERROR([libmonitor directory must be absolute path: $with_libmonitor])
	;;
esac

csprof_extern_includes="-I${with_libmonitor}/include"
libmonitor_prefix="$with_libmonitor"

#
# Option: --with-papi=PATH
#
AC_ARG_WITH([papi],
    [AS_HELP_STRING([--with-papi=PATH],
        [path to PAPI install directory (optional)])],
    [],
    [with_papi=no])

papi_libs=

if test "x$with_papi" = xno ; then
    AC_MSG_WARN([configured without PAPI])
else
    case "$with_papi" in
	/* ) ;;
	* ) AC_MSG_ERROR([PAPI directory must be absolute path: $with_papi])
	    ;;
    esac
    AC_MSG_CHECKING([PAPI install directory ($with_papi)])

    incl="${with_papi}/include"
    if test ! -f "${incl}/papi.h" ; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([invalid PAPI directory: no papi.h])
    fi
    if is_member "-I$incl" "$csprof_extern_includes" ; then :
    else
	csprof_extern_includes="$csprof_extern_includes -I$incl"
    fi

    so_file="${with_papi}/lib/libpapi.so"
    if test ! -f "$so_file" ; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([invalid PAPI directory: no libpapi.so])
    fi
    ldd "$so_file" | grep libperfctr >/dev/null 2>&1
    if test $? = 0 ; then
	papi_libs='-lperfctr'
    fi
    ldd "$so_file" | grep libpfm >/dev/null 2>&1
    if test $? = 0 ; then
	papi_libs="$papi_libs -lpfm"
    fi

    AC_MSG_RESULT([$papi_libs])
fi

papi_prefix="$with_papi"

AM_CONDITIONAL([CSPROF_AMTEST_USE_PAPI],
    [test x$with_papi != xno])

#
# Option: --with-symbols=PATH
#
AC_ARG_WITH([symbols],
    [AS_HELP_STRING([--with-symbols=PATH],
        [path to symbols install directory (required)])],
    [],
    [with_symbols=no])

case "$with_symbols" in
    /* )
	if test ! -x "${with_symbols}/libexec/stapi_syms" ; then
	    AC_MSG_ERROR([invalid symbols directory: $with_symbols])
	fi
	;;
    no )
	AC_MSG_ERROR([must set --with-symbols=/path/to/symbols])
	;;
    * )
	AC_MSG_ERROR([symbols directory must be absolute path: $with_symbols])
	;;
esac

symbols_prefix="$with_symbols"

#
# Option: --with-symtabAPI=PATH
#
AC_ARG_WITH([symtabAPI],
    [AS_HELP_STRING([--with-symtabAPI=PATH],
        [path to symtabAPI install directory (required)])],
    [],
    [with_symtabAPI=no])

case "$with_symtabAPI" in
    /* )
	if test ! -f "${with_symtabAPI}/include/Symtab.h" ; then
	    AC_MSG_ERROR([invalid symtabAPI directory: $with_symtabAPI])
	fi
	;;
    no )
	AC_MSG_ERROR([must set --with-symtabAPI=/path/to/symtabAPI])
	;;
    * )
	AC_MSG_ERROR([symtabAPI directory must be absolute path: $with_symtabAPI])
	;;
esac

incl="-I${with_symtabAPI}/include"
if is_member "$incl" "$csprof_extern_includes" ; then : ; else
    csprof_extern_includes="$csprof_extern_includes $incl"
fi
symtabAPI_prefix="$with_symtabAPI"

#
# Option: --with-xed2=PATH
#
AC_ARG_WITH([xed2],
    [AS_HELP_STRING([--with-xed2=PATH],
        [path to XED2 install directory (required on x86)])],
    [],
    [with_xed2=no])

if test "x$csprof_is_x86" = xyes ; then
    case "$with_xed2" in
	/* )
	    if test ! -f "${with_xed2}/include/xed-interface.h" ; then
		AC_MSG_ERROR([invalid XED2 directory: $with_xed2])
	    fi
	    ;;
	no )
	    AC_MSG_ERROR([must set --with-xed2=/path/to/XED2])
	    ;;
	* )
	    AC_MSG_ERROR([XED2 directory must be absolute path: $with_xed2])
	    ;;
    esac

    incl="-I${with_xed2}/include"
    if is_member "$incl" "$csprof_extern_includes" ; then : ; else
	csprof_extern_includes="$csprof_extern_includes $incl"
    fi
else
    with_xed2=no
fi

xed2_prefix="$with_xed2"

#
# Find location of libstdc++.
# FIXME -- Is there an Autoconf macro for this?
#
src="/tmp/hello.$$.cc"
obj="/tmp/hello.$$.o"
cat <<EOF > "$src"
main(){}
EOF
g++ -o "$obj" "$src"
libcpp_path=`ldd $obj | $AWK '/libstdc\+\+/ {print $3}'`
libcpp_path=`dirname $libcpp_path`
rm -f "$src" "$obj"


AC_OUTPUT

#
# Summary of the config options.
#
AC_MSG_NOTICE([])
AC_MSG_NOTICE([csprof summary])
AC_MSG_NOTICE([prefix: $prefix])
AC_MSG_NOTICE([host: $host_cpu, $host_vendor, $host_os])
AC_MSG_NOTICE([CC, CFLAGS: $CC $CFLAGS])
AC_MSG_NOTICE([libmonitor: $with_libmonitor])
AC_MSG_NOTICE([papi: $with_papi])
AC_MSG_NOTICE([symbols: $with_symbols])
AC_MSG_NOTICE([symtabAPI: $with_symtabAPI])
AC_MSG_NOTICE([xed2: $with_xed2])

