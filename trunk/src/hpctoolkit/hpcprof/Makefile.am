# -*-Mode: makefile;-*-
# $Header$

## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002, Rice University 
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
#
# File:
#   $Source$
#
# Description:
#   *Process with automake to produce Makefile.in*
#   
#   Note: All local variables are prefixed with MY to prevent name
#   clashes with automatic automake variables.
#
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

MYSOURCES_HPCRUN = \
	hpcrun.h hpcrun.cc \
	hpcpapi.h hpcpapi.c \
	dlpapi.h dlpapi.c 

MYSOURCES_LIBHPCRUN_PRELOAD = \
	monitor_preload.c \
	monitor.h monitor.c \
	hpcpapi.h hpcpapi.c \
	rtmap.h rtmap.c 

MYSOURCES_LIBHPCRUN_STATIC = \
	monitor_static.cc \
	monitor.h monitor.c \
	hpcpapi.h hpcpapi.c \
	rtmap.h rtmap.c 

MYSOURCES_HPCPROF = \
	hpcprof.h hpcprof.cc \
	summary.h summary.cc \
	loadmodule.h loadmodule.cc \
	proffile.h proffile.cc 


MYIFLAGS_PAPI = \
	-DHPC_PAPI_LIBSO=\"@OPT_PAPI_LIBSO@\"   \
	-DHPC_PAPI_LIBPATH=\"@OPT_PAPI_LIBPATH@\"

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@

MYCFLAGS_HPCRUN   = $(MYCFLAGS)   $(MYIFLAGS_PAPI)
MYCXXFLAGS_HPCRUN = $(MYCXXFLAGS) $(MYIFLAGS_PAPI)

MYCFLAGS_HPCPROF   = $(MYCFLAGS)   @GNUBINUTILS_IFLAGS@
MYCXXFLAGS_HPCPROF = $(MYCXXFLAGS) @GNUBINUTILS_IFLAGS@


MYLDFLAGS_HPCRUN    = \
	@HOST_CXXFLAGS@ -R @OPT_PAPI_LIBPATH@
MYLDFLAGS_LIBHPCRUN_PRELOAD = \
	@HOST_CXXFLAGS@ -Xcompiler @HOST_LINK_NO_START_FILES@
MYLDFLAGS_LIBHPCRUN_STATIC = \
	@HOST_CXXFLAGS@ -all-static
MYLDFLAGS_HPCPROF   = \
	@HOST_CXXFLAGS@


MYLDADD_HPCRUN    = \
	$(HPC_LIBSUPPORT) \
	-ldl
MYLDADD_LIBHPCRUN_PRELOAD = \
	$(HPC_LIBHPCFILE) \
	@OPT_PAPI_LDFLAGS@ \
	-ldl
MYLDADD_LIBHPCRUN_STATIC = \
	$(HPC_LIBHPCFILE) \
	@OPT_PAPI_LIBA@
MYLDADD_HPCPROF   = \
	$(HPC_LIBHPCFILE) \
	@HOST_LIBTREPOSITORY@ \
	@GNUBINUTILS_LDFLAGS@


MYCLEAN = @HOST_LIBTREPOSITORY@

MYINSTALLDIR = $(DESTDIR)$(prefix)

#############################################################################
# Automake rules
#############################################################################

bin_PROGRAMS            = hpcrun
hpcrun_SOURCES          = $(MYSOURCES_HPCRUN)
hpcrun_CFLAGS           = $(MYCFLAGS_HPCRUN)
hpcrun_CXXFLAGS         = $(MYCXXFLAGS_HPCRUN)
hpcrun_LDFLAGS          = $(MYLDFLAGS_HPCRUN)
hpcrun_LDADD            = $(MYLDADD_HPCRUN)

# N.B.: We only care about the dynamically linked version of this
# library.  An install-hook below ensures only intercepted symbols
# will be visible.
lib_LTLIBRARIES         = libhpcrun.la
libhpcrun_la_SOURCES    = $(MYSOURCES_LIBHPCRUN_PRELOAD)
libhpcrun_la_CFLAGS     = $(MYCFLAGS)
libhpcrun_la_LDFLAGS    = $(MYLDFLAGS_LIBHPCRUN_PRELOAD)
libhpcrun_la_LIBADD     = $(MYLDADD_LIBHPCRUN_PRELOAD)

# N.B.: We use the PROGRAMS primary to get free linking.  An
# install-hook below moves this from bin to lib in the installation.
bin_PROGRAMS               += libhpcrun-static.o
libhpcrun_static_o_SOURCES  = $(MYSOURCES_LIBHPCRUN_STATIC)
libhpcrun_static_o_CFLAGS   = $(MYCFLAGS)
libhpcrun_static_o_CXXFLAGS = $(MYCXXFLAGS)
libhpcrun_static_o_LDFLAGS  = $(MYLDFLAGS_LIBHPCRUN_STATIC)
libhpcrun_static_o_LDADD    = $(MYLDADD_LIBHPCRUN_STATIC)

# Can We use libtool to get free linking?  It complains.
#lib_LTLIBRARIES             += libhpcrun-static.o
#libhpcrun_static_o_SOURCES  = $(MYSOURCES_LIBHPCRUN_STATIC)
#libhpcrun_static_o_CFLAGS   = $(MYCFLAGS)
#libhpcrun_static_o_CXXFLAGS = $(MYCXXFLAGS)
#libhpcrun_static_o_LDFLAGS  = $(MYLDFLAGS_LIBHPCRUN_STATIC)
#libhpcrun_static_o_LIBADD   = $(MYLDADD_LIBHPCRUN_STATIC)

bin_PROGRAMS           += hpcprof
hpcprof_SOURCES         = $(MYSOURCES_HPCPROF)
hpcprof_CFLAGS          = $(MYCFLAGS_HPCPROF)
hpcprof_CXXFLAGS        = $(MYCXXFLAGS_HPCPROF)
hpcprof_LDFLAGS         = $(MYLDFLAGS_HPCPROF)
hpcprof_LDADD           = $(MYLDADD_HPCPROF)

MOSTLYCLEANFILES        = $(MYCLEAN)

#############################################################################
# Other rules
#############################################################################

MY_INTERCEPTED_SYMS_COMMON = \
	__libc_start_main  __BP___libc_start_main \
	execl execlp execle execv execvp execve \
	fork vfork \
	pthread_create

MY_INTERCEPTED_SYMS_LIBHPCRUNSO = \
	_init _fini \
	$(MY_INTERCEPTED_SYMS_COMMON)

MY_INTERCEPTED_SYMS_LIBHPCRUNST = \
	$(MY_INTERCEPTED_SYMS_COMMON)


# *-hook runs after install has completed
install-exec-local: myinstall-exec

myinstall-exec: myinstall-libhpcrunso myinstall-libhpcrun-static


MYKEEPSYMS = $(addprefix -K , $(MY_INTERCEPTED_SYMS_LIBHPCRUNSO))

myinstall-libhpcrunso:
	rm -f $(MYINSTALLDIR)/lib/libhpcrun.a
	objcopy -S $(MYKEEPSYMS) $(MYINSTALLDIR)/lib/libhpcrun.so

myinstall-libhpcrun-static: 
	mv $(MYINSTALLDIR)/bin/libhpcrun-static.o $(MYINSTALLDIR)/lib/.

.PHONY: myinstall-exec myinstall-libhpcrunso myinstall-libhpcrun-static

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

