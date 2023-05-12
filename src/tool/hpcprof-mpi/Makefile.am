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
## Copyright ((c)) 2002-2023, Rice University
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

EXT_LIBS = lib/hpctoolkit/ext-libs

LIBELF_INC = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@

XED2_LIB_FLAGS = @XED2_LIB_FLAGS@

MY_ELF_DWARF = -L$(LIBELF_LIB) -ldw -lelf

MYSOURCES = \
	main.cpp tree.cpp \
	mpi.cpp ../hpcprof/args.cpp

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS)
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @BOOST_IFLAGS@


MYLDFLAGS = \
	-Wl,-rpath='$(prefix)/$(EXT_LIBS)' \
	-Wl,-rpath='$$ORIGIN/../$(EXT_LIBS)' \
	-lstdc++fs \
	@HPCPROFMPI_LT_LDFLAGS@ \
	@HOST_CXXFLAGS@ \
	@XERCES_LDFLAGS@ \
	@LZMA_PROF_MPI_LIBS@ \
	@YAMLCPP_LDFLAGS@ \
	-L@LIBELF_LIB@ -ldw -lelf

LIBSTDCXX_RPATH_WHEN_NO_LAUNCHSCRIPT = \
	-Wl,--disable-new-dtags -Wl,-rpath='@HPCRUN_LIBCXX_PATH@'

MYLDADD = \
	@HOST_LIBTREPOSITORY@ \
	$(HPCLIB_Profile) \
	$(HPCLIB_ProfLean) \
	$(HPCLIB_SupportLean) \
	$(MY_ELF_DWARF) \
	@LZMA_PROF_MPI_LIBS@ \
	@XERCES_LDLIBS@ \
	@LIBIBERTY_LIBS@ \
	@YAMLCPP_LDLIBS@ \
	@HOST_HPCPROF_LDFLAGS@

if OPT_ENABLE_OPENMP
MYCFLAGS += $(OPENMP_FLAG)
MYCXXFLAGS += $(OPENMP_FLAG)
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

CXX = $(MPICXX)

bin_PROGRAMS = hpcprof-mpi

hpcprof_mpi_SOURCES  = $(MYSOURCES)
hpcprof_mpi_CFLAGS   = $(MYCFLAGS)
hpcprof_mpi_CXXFLAGS = $(MYCXXFLAGS)
hpcprof_mpi_LDFLAGS  = $(MYLDFLAGS) $(LIBSTDCXX_RPATH_WHEN_NO_LAUNCHSCRIPT)
hpcprof_mpi_LDADD    = $(MYLDADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
