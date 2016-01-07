# -*-Mode: makefile;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2002-2016, Rice University
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
##   $HeadURL$
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
# Local settings
#############################################################################

MYSOURCES_HPCRunFlat = \
	hpcrun.h hpcrun.cpp \
	Args.hpp Args.cpp \
	hpcpapi.h hpcpapi.c \
	dlpapi.h dlpapi.c 

MYSOURCES_LibHPCRunFlat_PRELOAD = \
	monitor_preload.c \
	monitor.h monitor.c \
	hpcpapi.h hpcpapi.c \
	rtmap.h rtmap.c 

MYSOURCES_LibHPCRunFlat_STATIC = \
	monitor_static.cpp \
	monitor.h monitor.c \
	hpcpapi.h hpcpapi.c \
	rtmap.h rtmap.c 


MYIFLAGS_MONITOR = \
	-DHPC_LIBMONITOR_SO=\"libmonitor.so\" \
	-DHPC_MONITOR=\"@HPC_MONITOR@\"

MYIFLAGS_PAPI = \
	-DHPC_LIBPAPI_SO=\"libpapi.so\" \
	-DHPC_PAPI=\"@OPT_PAPI@\"

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@

MYCFLAGS_HPCRUN   = $(MYCFLAGS)   $(MYIFLAGS_MONITOR) $(MYIFLAGS_PAPI)
MYCXXFLAGS_HPCRUN = $(MYCXXFLAGS) $(MYIFLAGS_MONITOR) $(MYIFLAGS_PAPI)


MYLDFLAGS_HPCRUN    = \
	@HOST_CXXFLAGS@
MYLDFLAGS_LIBHPCRUN_PRELOAD = \
	@HOST_CXXFLAGS@ -Xcompiler @HOST_LINK_NO_START_FILES@ \
	-R@OPT_PAPI_LIBPATH@
MYLDFLAGS_LIBHPCRUN_STATIC = \
	@HOST_CXXFLAGS@ -all-static


MYLDADD_HPCRUN    = \
	$(HPCLIB_Support) \
	-ldl
MYLDADD_LIBHPCRUN_PRELOAD = \
	$(HPCLIB_ProfLean) \
	-ldl \
	@OPT_PAPI_LDFLAGS@

MYLDADD_LIBHPCRUN_STATIC = \
	$(HPCLIB_ProfLean) \
	@OPT_PAPI_LDFLAGS@


MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

bin_PROGRAMS            = hpcrun-flat
hpcrun_flat_SOURCES     = $(MYSOURCES_HPCRunFlat)
hpcrun_flat_CFLAGS      = $(MYCFLAGS_HPCRUN)
hpcrun_flat_CXXFLAGS    = $(MYCXXFLAGS_HPCRUN)
hpcrun_flat_LDFLAGS     = $(MYLDFLAGS_HPCRUN)
hpcrun_flat_LDADD       = $(MYLDADD_HPCRUN)

# N.B.: We only care about the dynamically linked version of this
# library.  An install-hook below ensures only intercepted symbols
# will be visible.
pkglib_LTLIBRARIES        = libhpcrun-flat.la
libhpcrun_flat_la_SOURCES = $(MYSOURCES_LibHPCRunFlat_PRELOAD)
libhpcrun_flat_la_CFLAGS  = $(MYCFLAGS)
libhpcrun_flat_la_LDFLAGS = $(MYLDFLAGS_LIBHPCRUN_PRELOAD)
libhpcrun_flat_la_LIBADD  = $(MYLDADD_LIBHPCRUN_PRELOAD)

# N.B.: We use the PROGRAMS macro primarily to get free linking, since
# using lib_LTLIBRARIES doesn't work.  An install-hook below moves
# this from bin to lib in the installation.
##lib_LTLIBRARIES             += libhpcrun-static.o
#bin_PROGRAMS               += libhpcrun-static.o
#libhpcrun_static_o_SOURCES  = $(MYSOURCES_LibHPCRunFlat_STATIC)
#libhpcrun_static_o_CFLAGS   = $(MYCFLAGS)
#libhpcrun_static_o_CXXFLAGS = $(MYCXXFLAGS)
#libhpcrun_static_o_LDFLAGS  = $(MYLDFLAGS_LIBHPCRUN_STATIC)
#libhpcrun_static_o_LDADD    = $(MYLDADD_LIBHPCRUN_STATIC)

MOSTLYCLEANFILES        = $(MYCLEAN)

#############################################################################
# Other rules
#############################################################################

############################################################
# Install
############################################################

MY_INTERCEPTED_SYMS_COMMON = \
	__libc_start_main  __BP___libc_start_main \
	execl execlp execle execv execvp execve \
	fork vfork \
	_exit _Exit \
	pthread_create

MY_INTERCEPTED_SYMS_LibHPCRunSO = \
	_init _fini \
	$(MY_INTERCEPTED_SYMS_COMMON)

MY_INTERCEPTED_SYMS_LIBHPCRUNST = \
	$(MY_INTERCEPTED_SYMS_COMMON)

# Note: *-local and *-hook are special automake versions of the
# regular target. *-hook runs *after* install has completed

install-exec-hook: myinstall-exec

myinstall-exec: myinstall-libhpcrunso myinstall-libhpcrun-static


MYKEEPSYMS = $(addprefix -K , $(MY_INTERCEPTED_SYMS_LibHPCRunSO))

myinstall-libhpcrunso:
	rm -f $(DESTDIR)$(pkglibdir)/libhpcrun-flat.a
	@OPT_OBJCOPY@ -S $(MYKEEPSYMS) $(DESTDIR)$(pkglibdir)/libhpcrun-flat.so

myinstall-libhpcrun-static: 
#	mv $(bindir)/libhpcrun-static.o $(libdir)/.

.PHONY: myinstall-exec myinstall-libhpcrunso myinstall-libhpcrun-static

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

