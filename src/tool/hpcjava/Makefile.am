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

LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@

OPT_LIBUNWIND_IFLAGS = @OPT_LIBUNWIND_IFLAGS@

OPROFILE_IFLAGS =  @OPROFILE_IFLAGS@
OPROFILE_LIB = @OPROFILE_LIB@
OPROFILE_BIN = @OPROFILE_BIN@

JAVA_IFLAGS = @JAVA_IFLAGS@

HPCRUN_SRCDIR = $(top_srcdir)/src/tool/hpcrun

pkglib_LTLIBRARIES =
if OPT_ENABLE_HPCRUN_DYNAMIC
  pkglib_LTLIBRARIES += libhpcjava.la
endif

libhpcjava_la_SOURCES = libhpcjava.c

libhpcjava_la_CPPFLAGS = 		\
	$(HPC_IFLAGS)			\
	-I$(top_srcdir)/src/tool/	\
	-I$(HPCRUN_SRCDIR)		\
	-I$(HPCRUN_SRCDIR)/fnbounds	\
	-I$(HPCRUN_SRCDIR)/memory	\
	-I$(HPCRUN_SRCDIR)/messages	\
	-I$(HPCRUN_SRCDIR)/unwind/common  \
	-I$(HPCRUN_SRCDIR)/unwind/generic-libunwind \
	-I$(LIBMONITOR_INC)	\
	$(OPT_LIBUNWIND_IFLAGS)	\
	$(OPROFILE_IFLAGS)	\
	$(JAVA_IFLAGS)

libhpcjava_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)

libhpcjava_la_LDFLAGS = \
	-L$(LIBMONITOR_LIB) -lmonitor \
	-L$(OPROFILE_LIB) -lopagent

pkglibexecdir = @my_pkglibexecdir@
#pkglibexec_PROGRAMS = ${OPROFILE_BIN}/opjitconv


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
