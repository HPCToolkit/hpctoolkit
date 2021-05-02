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
AUTOMAKE_OPTIONS = foreign

#############################################################################
# Common settings
#############################################################################

include $(top_srcdir)/src/Makeinclude.config


#############################################################################
# Local settings
#############################################################################

EXT_LIBS = lib/hpctoolkit/ext-libs

LIBDWARF_INC = @LIBDWARF_INC@
LIBDWARF_LIB = @LIBDWARF_LIB@

LIBELF_INC = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@

if OPT_DYNINST_LIBDW
MY_ELF_DWARF = -L$(LIBELF_LIB) -ldw -lelf
else
MY_ELF_DWARF = -L$(LIBDWARF_LIB) -ldwarf -L$(LIBELF_LIB) -lelf
endif

MYSOURCES = \
	main.cpp args.cpp

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @BINUTILS_IFLAGS@ @VALGRIND_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @BINUTILS_IFLAGS@ @XERCES_IFLAGS@ \
	@BOOST_IFLAGS@ @VALGRIND_IFLAGS@ -std=gnu++17

MYLDFLAGS = \
	-Wl,-rpath='$(prefix)/$(EXT_LIBS)' \
	-Wl,-rpath='$$ORIGIN/../$(EXT_LIBS)' \
	-lstdc++fs \
	@HOST_CXXFLAGS@ \
	@XERCES_LDFLAGS@ \
	@LZMA_LDFLAGS_DYN@ \
    -L@LIBELF_LIB@ -ldw -lelf

MYLDADD = \
	@HOST_LIBTREPOSITORY@ \
	$(HPCLIB_Profile) \
	$(HPCLIB_ProfileStandalone) \
	$(HPCLIB_ProfLean) \
	$(HPCLIB_SupportLean) \
	$(MY_ELF_DWARF) \
	@LZMA_LDFLAGS_STAT@ \
	@XERCES_LDLIBS@ \
	@BINUTILS_LIBS@ \
	@BOOST_LFLAGS@ \
	@HOST_HPCPROF_LDFLAGS@ \
	$(HPCLIB_Binutils)


if OPT_ENABLE_OPENMP
MYCFLAGS += $(OPENMP_FLAG)
MYCXXFLAGS += $(OPENMP_FLAG)
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

bin_PROGRAMS = hpcprof2

hpcprof2_SOURCES  = $(MYSOURCES)
hpcprof2_CFLAGS   = $(MYCFLAGS)
hpcprof2_CXXFLAGS = $(MYCXXFLAGS)
hpcprof2_LDFLAGS  = $(MYLDFLAGS)
hpcprof2_LDADD    = $(MYLDADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

