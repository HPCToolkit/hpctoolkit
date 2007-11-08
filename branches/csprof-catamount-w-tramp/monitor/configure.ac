dnl
dnl  Libmonitor top-level configure.ac.
dnl

AC_INIT([libmonitor], [1.0], [hpc@cs.rice.edu], [monitor])
AC_REVISION([$Id$])

AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_FILES([src/monitor-link], [chmod +x src/monitor-link])
AC_CONFIG_FILES([src/monitor-run],  [chmod +x src/monitor-run])

AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AC_SUBST([wrap_list], ["main _exit"])
AC_SUBST([library_list])

#
# Option: --enable-debug=no
#
AC_ARG_ENABLE([dummy-blank-line], [AS_HELP_STRING([ ], [ ])])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
	[debug mode is always on [default=no]])],
    [],
    [enable_debug=no])

if test "x$enable_debug" = xyes ; then
    AC_DEFINE([MONITOR_DEBUG_DEFAULT_ON], [1],
	[Debug mode defaults to on.])
fi

#
# Option: --enable-fork=yes
# Also requires fork and exec families.
#
AC_ARG_ENABLE([fork],
    [AS_HELP_STRING([--enable-fork],
        [include support for fork and exec families [default=yes]])],
    [],
    [enable_fork=yes])

if test "x$enable_fork" = xyes ; then
    AC_SEARCH_LIBS([fork], [c], [], [enable_fork=no])
fi
if test "x$enable_fork" = xyes ; then
    AC_SEARCH_LIBS([execve], [c], [], [enable_fork=no])
fi
if test "x$enable_fork" = xyes ; then
    AC_DEFINE([MONITOR_USE_FORK], [1],
	[Include support for fork and exec families.])
    wrap_list="${wrap_list} fork vfork"
    wrap_list="${wrap_list} execl execlp execle execv execvp execve"
fi

AM_CONDITIONAL([MONITOR_TEST_USE_FORK],
    [test x$enable_fork = xyes])

#
# Option: --enable-link-preload=yes
# Also requires dlsym and __libc_start_main.
#
AC_ARG_ENABLE([link-preload],
    [AS_HELP_STRING([--enable-link-preload],
	[build dynamic shared library to run monitor with LD_PRELOAD
	[default=yes]])],
    [],
    [enable_link_preload=yes])

if test "x$enable_link_preload" = xyes ; then
    AC_SEARCH_LIBS([dlsym], [dl], [], [enable_link_preload=no])
fi
if test "x$enable_link_preload" = xyes ; then
    AC_SEARCH_LIBS([__libc_start_main], [c], [], [enable_link_preload=no])
fi

AM_CONDITIONAL([MONITOR_TEST_LINK_PRELOAD],
    [test x$enable_link_preload = xyes])

#
# Option: --enable-link-static=yes
#
AC_ARG_ENABLE([link-static],
    [AS_HELP_STRING([--enable-link-static],
	[build monitor.o object file to link monitor statically
	[default=yes]])],
    [],
    [enable_link_static=yes])

AM_CONDITIONAL([MONITOR_TEST_LINK_STATIC],
    [test x$enable_link_static = xyes])

#
# Option: --enable-pthreads=yes
# Also requires libpthread.
#
AC_ARG_ENABLE([pthreads],
    [AS_HELP_STRING([--enable-pthreads],
	[include pthread support [default=yes]])],
    [],
    [enable_pthreads=yes])

if test "x$enable_pthreads" = xyes ; then
    AC_SEARCH_LIBS([pthread_create], [pthread], [], [enable_pthreads=no])
fi

if test "x$enable_pthreads" = xyes ; then
    AC_DEFINE([MONITOR_USE_PTHREADS], [1],
	[Include support for pthreads.])
    wrap_list="${wrap_list} pthread_create"
    library_list="${library_list} pthread"
    #
    # Test if pthread_equal and pthread_cleanup_push/pop are macros or
    # library functions.
    #
    OLDLIBS="$LIBS"
    LIBS=
    AC_CHECK_LIB([pthread], [pthread_equal],
        [AC_DEFINE([MONITOR_PTHREAD_EQUAL_IS_FCN], [1],
	    [pthread_equal is a function, not a macro.])])
    LIBS=
    AC_CHECK_LIB([pthread], [pthread_cleanup_push],
        [AC_DEFINE([MONITOR_PTHREAD_CLEANUP_PUSH_IS_FCN], [1],
	    [pthread_cleanup_push is a function, not a macro.])])
    LIBS="$OLDLIBS"
fi

AM_CONDITIONAL([MONITOR_TEST_USE_PTHREADS],
    [test x$enable_pthreads = xyes])

AC_OUTPUT

#
# Display a summary of the config options.
#
AC_MSG_NOTICE([monitor prefix: $prefix])
AC_MSG_NOTICE([monitor build: $build])
if test "x$host" != "x$build" ; then
    AC_MSG_NOTICE([monitor host: $host])
fi
if test "x$target" != x && test "x$target" != "x$build" ; then
    AC_MSG_NOTICE([monitor target: $target])
fi
AC_MSG_NOTICE([monitor CC CFLAGS: $CC $CFLAGS])
AC_MSG_NOTICE([monitor debug mode: $enable_debug])
AC_MSG_NOTICE([monitor use fork: $enable_fork])
AC_MSG_NOTICE([monitor link preload: $enable_link_preload])
AC_MSG_NOTICE([monitor link static: $enable_link_static])
AC_MSG_NOTICE([monitor use pthreads: $enable_pthreads])
