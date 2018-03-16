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
## Copyright ((c)) 2002-2018, Rice University
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

BOOST_LIB = @BOOST_LIB@
BOOST_LIB_LIST = @BOOST_LIB_LIST@

LIBDWARF_LIB = @LIBDWARF_LIB@
LIBELF_LIB   = @LIBELF_LIB@
SYMTABAPI_LIB = @SYMTABAPI_LIB@
SYMTABAPI_LIB_LIST = @SYMTABAPI_LIB_LIST@
XED2_LIB_FLAGS = @XED2_LIB_FLAGS@
ZLIB_LIB = @ZLIB_LIB@

MYSOURCES = main.cpp Args.cpp

MYCXXFLAGS = \
	@HOST_CXXFLAGS@  \
	$(HPC_IFLAGS)  \
	@GNUBINUTILS_IFLAGS@

MYLDFLAGS = \
	@HOST_CXXFLAGS@ \
	@GNUBINUTILS_LDFLAGS@ \
	-ldl

MYLDADD = \
	@HOST_LIBTREPOSITORY@ \
	$(HPCLIB_Analysis) \
	$(HPCLIB_Banal) \
	$(HPCLIB_Banal_Simple) \
	$(HPCLIB_Prof) \
	$(HPCLIB_Binutils) \
	$(HPCLIB_ISA) \
	$(MY_LIB_XED) \
	$(HPCLIB_XML) \
	$(HPCLIB_Support) \
	$(HPCLIB_SupportLean) \
	-L$(SYMTABAPI_LIB) $(SYMTABAPI_LIB_LIST) \
	$(MY_ELF_DWARF) \
	@GNUBINUTILS_LDLIBS@ \
	@HOST_HPCSTRUCT_LDFLAGS@

if USE_BOOST_LIBS
MYLDADD += -L$(BOOST_LIB) $(BOOST_LIB_LIST)
endif

if OPT_DYNINST_LIBDW
MY_ELF_DWARF = -L$(LIBELF_LIB) -ldw -lelf
else
MY_ELF_DWARF = -L$(LIBDWARF_LIB) -ldwarf -L$(LIBELF_LIB) -lelf
endif

if HOST_CPU_X86_FAMILY
MY_LIB_XED = $(XED2_LIB_FLAGS)
else
MY_LIB_XED =
endif

if OPT_USE_ZLIB
MYLDADD += -L$(ZLIB_LIB) -lz
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

#----------------------------------------------------------------------

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

bin_SCRIPTS = hpcstruct
pkglibexec_PROGRAMS = hpcstruct-bin

hpcstruct_bin_SOURCES  = $(MYSOURCES)
hpcstruct_bin_CXXFLAGS = $(MYCXXFLAGS)
hpcstruct_bin_LDFLAGS  = $(MYLDFLAGS)
hpcstruct_bin_LDADD    = $(MYLDADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#----------------------------------------------------------------------

include $(top_srcdir)/src/Makeinclude.rules

