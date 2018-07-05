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
## Copyright ((c)) 2002-2017, Rice University
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
AUTOMAKE_OPTIONS = subdir-objects

#############################################################################
# Common settings
#############################################################################

include $(top_srcdir)/src/Makeinclude.config

#############################################################################
# Local settings
#############################################################################

MYSOURCES = \
	data/TCT-CFG.hpp \
	data/TCT-Time.hpp data/TCT-Time.cpp \
	data/TCT-Node.hpp data/TCT-Node.cpp \
	data/TCT-Cluster.hpp data/TCT-Cluster.cpp \
	data/TCT-Metrics.hpp data/TCT-Metrics.cpp \
	TraceAnalysis.hpp TraceAnalysis.cpp \
	TraceAnalysisCommon.hpp TraceAnalysisCommon.cpp \
	BinaryAnalyzer.hpp BinaryAnalyzer.cpp \
	CCTVisitor.hpp CCTVisitor.cpp \
	LocalTraceAnalyzer.hpp LocalTraceAnalyzer.cpp \
	TraceFileReader.hpp TraceFileReader.cpp \
	DifferenceQuantifier.hpp DifferenceQuantifier.cpp

# GNU binutils flags are needed for HPCLIB_ISA.
BOOST_INC = @BOOST_INC@
LIBDWARF_INC = @LIBDWARF_INC@
SYMTABAPI_INC = @SYMTABAPI_INC@

MYCXXFLAGS = \
	@HOST_CXXFLAGS@  \
	$(HPC_IFLAGS)  \
	@GNUBINUTILS_IFLAGS@  \
	-I$(BOOST_INC)  \
	-I$(LIBDWARF_INC)  \
	-I$(SYMTABAPI_INC)

if IS_HOST_AR
  MYAR = @HOST_AR@
else
  MYAR = $(AR) cru
endif

MYLIBADD = @HOST_LIBTREPOSITORY@

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

noinst_LTLIBRARIES = libHPCtrace-analysis.la

libHPCtrace_analysis_la_SOURCES  = $(MYSOURCES)
libHPCtrace_analysis_la_CFLAGS   = $(MYCFLAGS)
libHPCtrace_analysis_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCtrace_analysis_la_AR       = $(MYAR)
libHPCtrace_analysis_la_LIBADD   = $(MYLIBADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules


