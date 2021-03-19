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

LIBDWARF_INC = @LIBDWARF_INC@
LIBDWARF_LIB = @LIBDWARF_LIB@

LIBELF_INC = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@

XED2_LIB_FLAGS = @XED2_LIB_FLAGS@

if OPT_DYNINST_LIBDW
MY_ELF_DWARF = -L$(LIBELF_LIB) -ldw -lelf
else
MY_ELF_DWARF = -L$(LIBDWARF_LIB) -ldwarf -L$(LIBELF_LIB) -lelf
endif

MYSOURCES = \
	main.cpp \
	Args.hpp Args.cpp \
	ParallelAnalysis.hpp ParallelAnalysis.cpp

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @BINUTILS_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @BINUTILS_IFLAGS@ @XERCES_IFLAGS@


MYLDFLAGS = \
	@HPCPROFMPI_LT_LDFLAGS@ \
	@HOST_CXXFLAGS@ \
	@XERCES_LDFLAGS@ \
	@LZMA_PROF_MPI_LIBS@ 

MYLDADD = \
	@HOST_LIBTREPOSITORY@ \
	$(HPCLIB_Analysis) \
	$(HPCLIB_Banal_Simple) \
	$(HPCLIB_ProfXML) \
	$(HPCLIB_Prof) \
	$(HPCLIB_Binutils) \
	$(HPCLIB_ProfLean) \
	$(HPCLIB_ISA) \
	$(MY_LIB_XED) \
	$(HPCLIB_XML) \
	$(HPCLIB_Support) \
	$(HPCLIB_SupportLean) \
	$(MY_ELF_DWARF) \
	@LZMA_PROF_MPI_LIBS@ \
	@XERCES_LDLIBS@ \
	@BINUTILS_LIBS@ \
	$(HPCLIB_gettext) \
	@HOST_HPCPROF_LDFLAGS@ \
	@MBEDTLS_LIBS@

if HOST_CPU_X86_FAMILY
MY_LIB_XED = $(XED2_PROF_MPI_LIBS)
else
MY_LIB_XED =
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

CXX = $(MPICXX)

bin_SCRIPTS = hpcprof-mpi

pkglibexec_PROGRAMS = hpcprof-mpi-bin

hpcprof_mpi_bin_SOURCES  = $(MYSOURCES)
hpcprof_mpi_bin_CFLAGS   = $(MYCFLAGS)
hpcprof_mpi_bin_CXXFLAGS = $(MYCXXFLAGS)
hpcprof_mpi_bin_LDFLAGS  = $(MYLDFLAGS)
hpcprof_mpi_bin_LDADD    = $(MYLDADD)

MOSTLYCLEANFILES = $(MYCLEAN)

install-exec-hook:
	$(call HPC_moveIfStaticallyLinked,$(DESTDIR)$(pkglibexecdir)/hpcprof-mpi-bin$(EXEEXT),$(DESTDIR)$(bindir)/hpcprof-mpi$(EXEEXT))


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

