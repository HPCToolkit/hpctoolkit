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
	hpcrun.h hpcrun.c \
	hpcpapi.h hpcpapi.c \
	dlpapi.h dlpapi.c

MYSOURCES_LIBHPCRUN = \
	preload.c \
	hpcpapi.h hpcpapi.c \
	rtmap.h rtmap.c 

MYSOURCES_HPCPROF = \
	hpcprof.h hpcprof.cc \
	summary.h summary.cc \
	loadmodule.h loadmodule.cc \
	proffile.h proffile.cc 


MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @OPT_PAPI_IFLAGS@
MYCFLAGS_HPCRUN = \
	$(MYCFLAGS) \
	-DHPC_PAPI_DLOPEN=\"@OPT_PAPI_DLOPEN@\" \
	-DHPC_PAPI_LIBPATH=\"@OPT_PAPI_LIBPATH@\"


MYLDFLAGS_HPCRUN    = @HOST_CXXFLAGS@ -R @OPT_PAPI_LIBPATH@
MYLDFLAGS_LIBHPCRUN = @HOST_CXXFLAGS@ -Xcompiler @HOST_LINK_NO_START_FILES@
MYLDFLAGS_HPCPROF   = @HOST_CXXFLAGS@


MYLDADD_HPCRUN    = -ldl
MYLDADD_LIBHPCRUN = \
	$(HPC_LIBHPCFILE) \
	@OPT_PAPI_LDFLAGS@ \
	-ldl
MYLDADD_HPCPROF   = \
	$(HPC_LIBHPCFILE) \
	@HOST_LIBTREPOSITORY@ \
	@GNUBINUTILS_LDFLAGS@


MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

bin_PROGRAMS         = hpcrun
hpcrun_SOURCES       = $(MYSOURCES_HPCRUN)
hpcrun_CFLAGS        = $(MYCFLAGS_HPCRUN)
hpcrun_LDFLAGS       = $(MYLDFLAGS_HPCRUN)
hpcrun_LDADD         = $(MYLDADD_HPCRUN)

lib_LTLIBRARIES      = libhpcrun.la
libhpcrun_la_SOURCES = $(MYSOURCES_LIBHPCRUN)
libhpcrun_la_CFLAGS  = $(MYCFLAGS)
libhpcrun_la_LDFLAGS = $(MYLDFLAGS_LIBHPCRUN)
libhpcrun_la_LIBADD  = $(MYLDADD_LIBHPCRUN)

bin_PROGRAMS        += hpcprof
hpcprof_SOURCES      = $(MYSOURCES_HPCPROF)
hpcprof_CFLAGS       = $(MYCFLAGS)
hpcprof_CXXFLAGS     = $(MYCXXFLAGS)
hpcprof_LDFLAGS      = $(MYLDFLAGS_HPCPROF)
hpcprof_LDADD        = $(MYLDADD_HPCPROF)

MOSTLYCLEANFILES     = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

