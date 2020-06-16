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

AUTOMAKE_OPTIONS = foreign subdir-objects

include $(top_srcdir)/src/Makeinclude.config

#----------------------------------------------------------------------

BINUTILS_IFLAGS = @BINUTILS_IFLAGS@
BOOST_IFLAGS = @BOOST_IFLAGS@
DYNINST_IFLAGS = @DYNINST_IFLAGS@
LIBDWARF_INC = @LIBDWARF_INC@
LIBELF_INC = @LIBELF_INC@
TBB_IFLAGS = @TBB_IFLAGS@
VALGRIND_IFLAGS = @VALGRIND_IFLAGS@

if OPT_DYNINST_LIBDWARF
DWARF_IFLAGS = -I$(LIBDWARF_INC)
endif

MYSOURCES = \
	cuda/CFGParser.cpp  \
	cuda/CudaCFGFactory.cpp  \
	cuda/CudaFunction.cpp  \
	cuda/GraphReader.cpp \
	cuda/CudaBlock.cpp  \
	cuda/CudaCodeSource.cpp  \
	cuda/ReadCubinCFG.cpp \
	Struct.cpp  \
	Struct-Inline.cpp  \
	Struct-Output.cpp

SIMPLE_SRCS = StructSimple.cpp

MYCXXFLAGS = \
	@HOST_CXXFLAGS@  \
	$(HPC_IFLAGS)  \
	$(BINUTILS_IFLAGS)  \
	$(BOOST_IFLAGS)  \
	$(DWARF_IFLAGS)  \
	-I$(LIBELF_INC)  \
	$(DYNINST_IFLAGS)  \
	$(TBB_IFLAGS)    \
	$(VALGRIND_IFLAGS)

MYLIBADD = @HOST_LIBTREPOSITORY@

MYCLEAN = @HOST_LIBTREPOSITORY@

#----------------------------------------------------------------------

pkglibexecdir = @my_pkglibexecdir@

noinst_LTLIBRARIES = libHPCbanal.la libHPCbanal_simple.la

libHPCbanal_la_SOURCES  = $(MYSOURCES)
libHPCbanal_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCbanal_la_LIBADD   = $(MYLIBADD)

if OPT_ENABLE_OPENMP
libHPCbanal_la_CXXFLAGS += $(OPENMP_FLAG)
endif

libHPCbanal_simple_la_SOURCES  = $(SIMPLE_SRCS)
libHPCbanal_simple_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCbanal_simple_la_LIBADD   = $(MYLIBADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#----------------------------------------------------------------------

include $(top_srcdir)/src/Makeinclude.rules

