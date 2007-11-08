dnl
dnl Libmonitor top-level configure.ac.
dnl

AC_INIT([libmonitor], [1.0], [hpc@cs.rice.edu], [monitor])
AC_REVISION([$Id$])

AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AC_SEARCH_LIBS([dlsym], [dl])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
