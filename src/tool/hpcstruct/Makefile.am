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
## Copyright ((c)) 2002-2021, Rice University
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

AUTOMAKE_OPTIONS = foreign

include $(top_srcdir)/src/Makeinclude.config

#----------------------------------------------------------------------

# Fixme: new elfutils from spack wants bzip, xz and zlib.

BOOST_IFLAGS = @BOOST_IFLAGS@
BOOST_LFLAGS = @BOOST_LFLAGS@

DYNINST_IFLAGS = @DYNINST_IFLAGS@
DYNINST_LFLAGS = @DYNINST_LFLAGS@

LIBDWARF_INC = @LIBDWARF_INC@
LIBDWARF_LIB = @LIBDWARF_LIB@

SYMTABAPI_LIB = @SYMTABAPI_LIB@
SYMTABAPI_LIB_LIST = @SYMTABAPI_LIB_LIST@

LIBELF_INC = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@

XED2_LIB_FLAGS = @XED2_LIB_FLAGS@
ZLIB_LIB = @ZLIB_LIB@

LZMA_LDFLAGS_STAT = @LZMA_LDFLAGS_STAT@
LZMA_LDFLAGS_DYN  = @LZMA_LDFLAGS_DYN@

TBB_IFLAGS    = @TBB_IFLAGS@
TBB_LFLAGS    = @TBB_LFLAGS@
TBB_PROXY_LIB = @TBB_PROXY_LIB@

if OPT_ENABLE_IGC
IGC_LDFLGS = @OPT_IGC_LDFLAGS@
IGC_IFLAGS = @OPT_IGC_IFLAGS@
endif

MYSOURCES = main.cpp Args.cpp

MYCXXFLAGS = \
	@HOST_CXXFLAGS@  \
	$(HPC_IFLAGS)  \
	@BINUTILS_IFLAGS@ \
  $(BOOST_IFLAGS)  \
  $(DYNINST_IFLAGS) \
  -I$(LIBELF_INC)  \
  $(IGC_IFLAGS) \
  $(TBB_IFLAGS)

MYLDFLAGS = \
	@HOST_CXXFLAGS@ \
	-ldl

MYLDADD = \
	@HOST_HPCSTRUCT_LDFLAGS@ \
	$(HPCLIB_Analysis) \
	$(HPCLIB_Banal) \
	$(HPCLIB_Banal_Simple) \
	$(HPCLIB_Prof) \
	$(HPCLIB_Binutils) \
	$(HPCLIB_ProfLean) \
	$(HPCLIB_ISA) \
	$(MY_LIB_XED) \
	$(HPCLIB_XML) \
	$(HPCLIB_Support) \
	$(HPCLIB_SupportLean) \
	$(DYNINST_LFLAGS) \
	$(BOOST_LFLAGS) \
	$(MY_ELF_DWARF) \
	@BINUTILS_LIBS@ \
  $(IGC_LDFLGS) \
	$(HPCLIB_gettext) \
	$(LZMA_LDFLAGS_DYN) \
	$(MBEDTLS_LIBS) \
	$(TBB_LFLAGS)

if OPT_DYNINST_LIBDW
MY_ELF_DWARF = -L$(LIBELF_LIB) -ldw -lelf
else
MY_ELF_DWARF = -L$(LIBDWARF_LIB) -ldwarf -L$(LIBELF_LIB) -lelf
endif

if OPT_ENABLE_OPENMP
MYCXXFLAGS += $(OPENMP_FLAG)
endif

if HOST_CPU_X86_FAMILY
MY_LIB_XED = $(XED2_LIB_FLAGS)
else
MY_LIB_XED =
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

GENHEADERS = \
	gpubin-analysis.h \
	usage.h

#----------------------------------------------------------------------

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

noinst_HEADERS = $(GENHEADERS)
BUILT_SOURCES = $(GENHEADERS)

bin_SCRIPTS = hpcstruct
pkglibexec_SCRIPTS = dotgraph
pkglibexec_PROGRAMS = hpcstruct-bin dotgraph-bin

hpcstruct_bin_SOURCES  = $(MYSOURCES)
hpcstruct_bin_CXXFLAGS = $(MYCXXFLAGS)
hpcstruct_bin_LDFLAGS  = $(MYLDFLAGS)
hpcstruct_bin_LDADD    = $(MYLDADD)

dotgraph_bin_SOURCES  = DotGraph.cpp
dotgraph_bin_CXXFLAGS = $(MYCXXFLAGS)
dotgraph_bin_LDFLAGS  = $(MYLDFLAGS)
dotgraph_bin_LDADD    = $(MYLDADD)

MOSTLYCLEANFILES = $(MYCLEAN)
#############################################################################
# Other rules
#############################################################################

%.h : $(srcdir)/%.txt
	$(srcdir)/stringify.sh < $<  > $@


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

