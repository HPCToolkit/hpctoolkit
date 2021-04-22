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
## Copyright ((c)) 2002-2020, Rice University
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
AUTOMAKE_OPTIONS = foreign subdir-objects

#############################################################################
# Common settings
#############################################################################

include $(top_srcdir)/src/Makeinclude.config

#############################################################################
# Local settings
#############################################################################

MYSOURCES = \
	stdshim/shared_mutex.cpp stdshim/atomic.cpp stdshim/futex-detail.c \
	\
	util/ragged_vector.cpp util/once.cpp util/xml.cpp util/log.cpp \
	util/file-posix.cpp \
	\
	pipeline.cpp source.cpp context.cpp scope.cpp metric.cpp \
	module.cpp attributes.cpp dataclass.cpp sink.cpp packedids.cpp \
	\
	sources/hpcrun.cpp sources/packed.cpp sources/hpcrun4.cpp \
	\
	sinks/hpctracedb2.cpp sinks/packed.cpp sinks/experimentxml4.cpp \
	sinks/sparsedb.cpp \
	\
	finalizers/struct.cpp finalizers/directclassification.cpp \
	finalizers/denseids.cpp

MYSTANDALONESOURCES = mpi/standalone.cpp

# GNU binutils flags are needed for HPCLIB_ISA.
MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @VALGRIND_IFLAGS@ -I@LIBELF_INC@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @BOOST_IFLAGS@ @VALGRIND_IFLAGS@ \
	     @XERCES_IFLAGS@ -I@LIBELF_INC@ -std=gnu++17

if OPT_ENABLE_OPENMP
MYCFLAGS += $(OPENMP_FLAG)
MYCXXFLAGS += $(OPENMP_FLAG)
endif

if IS_HOST_AR
  MYAR = @HOST_AR@
else
  MYAR = $(AR) cru
endif

MYLIBADD = @HOST_LIBTREPOSITORY@ -L@LIBELF_LIB@ -ldw -lelf

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

noinst_LTLIBRARIES = libHPCprofile.la libHPCprofile_standalone.la

libHPCprofile_la_SOURCES  = $(MYSOURCES)
libHPCprofile_la_CFLAGS   = $(MYCFLAGS)
libHPCprofile_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCprofile_la_AR       = $(MYAR)
libHPCprofile_la_LIBADD   = $(MYLIBADD)

libHPCprofile_standalone_la_SOURCES  = $(MYSTANDALONESOURCES)
libHPCprofile_standalone_la_CFLAGS   = $(MYCFLAGS)
libHPCprofile_standalone_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCprofile_standalone_la_AR       = $(MYAR)
libHPCprofile_standalone_la_LIBADD   = $(MYLIBADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

