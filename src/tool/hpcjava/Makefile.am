# -*-Mode: makefile;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-jvmti/src/tool/hpcrun/Makefile.am $
## $Id: Makefile.am 4003 2012-11-09 05:17:55Z mfagan $
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2002-2012, Rice University
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
##
## File:
##   $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-jvmti/src/tool/hpcrun/Makefile.am $
##
## Description:
##   *Process with automake to produce Makefile.in*
##
##   Note: All local variables are prefixed with MY to prevent name
##   clashes with automatic automake variables.
##
#############################################################################

# We do not want the standard GNU files (NEWS README AUTHORS ChangeLog...)
AUTOMAKE_OPTIONS = foreign

#############################################################################
# Common settings
#############################################################################

include $(top_srcdir)/src/Makeinclude.config


#############################################################################
# Automake rules
#############################################################################


#-----------------------------------------------------------
# build targets
#-----------------------------------------------------------

pkglibexecdir = $(libexecdir)/$(PACKAGE)
plugin_srcdir = $(srcdir)/plugins
plugin_instdir = $(pkglibdir)/plugins

pkglib_LTLIBRARIES =
if OPT_ENABLE_HPCRUN_DYNAMIC
  pkglib_LTLIBRARIES += libhpcjava.la
  pkglib_LTLIBRARIES += libhpcjavaself.la
endif

pkglib_PROGRAMS =
pkglib_LIBRARIES =


#-----------------------------------------------------------
# source files, part 1
#-----------------------------------------------------------

LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@
PAPI_INC_FLGS =  @OPT_PAPI_IFLAGS@
PAPI_LD_FLGS =   @OPT_PAPI_LDFLAGS@
SYMTABAPI_INC =  @SYMTABAPI_INC@
XED2_INC = @XED2_INC@
XED2_LIB = @XED2_LIB@

MY_DYNAMIC_FILES = 					\
	libhpcjava.c		


MY_INCLUDE_DIRS =					\
	-I/home/la5/pkgs/externals/oprofile/include/		\
        -I$(top_srcdir)/src/tool/			\
        -I$(top_srcdir)/src/tool/hpcrun        		\
        -I$(top_srcdir)/src/tool/hpcrun/messages        \
        -I$(top_srcdir)/src/tool/hpcrun/memory 		\
        -I$(top_srcdir)/src/tool/hpcrun/unwind/common	\
        -I$(top_srcdir)/src/tool/hpcrun/unwind/generic-libunwind \
	$(HPC_IFLAGS)					\
	$(OPT_JAVA_IFLAGS)				\
	$(OPT_LIBUNWIND_IFLAGS)         

libhpcjava_la_SOURCES = 			\
	$(MY_DYNAMIC_FILES)

libhpcjavaself_la_SOURCES = 			\
	hpcrun_dummy.c				\
	$(MY_DYNAMIC_FILES)


#-----------------------------------------------------------
# cppflags
#-----------------------------------------------------------

libhpcjava_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcjavaself_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)


#-----------------------------------------------------------
# cflags
#-----------------------------------------------------------

libhpcjava_la_CFLAGS = -I/home/la5/pkgs/externals/oprofile/include/ $(CFLAGS) $(HOST_CFLAGS)

libhpcjavaself_la_CFLAGS = -I/home/la5/pkgs/externals/oprofile/include/ $(CFLAGS) $(HOST_CFLAGS)


#-----------------------------------------------------------
# ldflags
#-----------------------------------------------------------

#libhpcjava_la_LDFLAGS = -Wl,-Bsymbolic	\
#	-Wl,--strip-debug -lopagent
libhpcjava_la_LDFLAGS = -lopagent

libhpcjavaself_la_LDFLAGS = -lopagent

############################################################
# Install
############################################################
#
# Note: *-local and *-hook are special automake versions of the
# regular target. *-hook runs *after* install has completed

install-exec-hook: myinstall-exec

myinstall-exec: myinstall-libhpcjavaso \
		myinstall-libhpcjavaselfso


MYKEEPSYMS = $(addprefix -K , $(MY_INTERCEPTED_SYMS_LibHPCJavaSO))

#@OPT_OBJCOPY@ -S $(MYKEEPSYMS) $(DESTDIR)$(pkglibdir)/libhpcjava.so
myinstall-libhpcjavaso:
	rm -f $(DESTDIR)$(pkglibdir)/libhpjava.a
	@OPT_OBJCOPY@ $(DESTDIR)$(pkglibdir)/libhpcjava.so

myinstall-libhpcjavaselfso:
	rm -f $(DESTDIR)$(pkglibdir)/libhpjavaself.a
	@OPT_OBJCOPY@ $(DESTDIR)$(pkglibdir)/libhpcjavaself.so


