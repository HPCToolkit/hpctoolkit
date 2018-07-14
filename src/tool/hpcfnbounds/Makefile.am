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

BOOST_INC = @BOOST_INC@
BOOST_LIB = @BOOST_LIB@
BOOST_LIB_LIST = @BOOST_LIB_LIST@

LIBDWARF_INC = @LIBDWARF_INC@
LIBDWARF_LIB = @LIBDWARF_LIB@

LIBELF_INC   = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@

SYMTABAPI_INC = @SYMTABAPI_INC@
SYMTABAPI_LIB = @SYMTABAPI_LIB@
SYMTABAPI_LIB_LIST = @SYMTABAPI_LIB_LIST@

XED2_INC = @XED2_INC@
XED2_LIB_FLAGS = @XED2_LIB_FLAGS@

ZLIB_LIB = @ZLIB_LIB@

MYSOURCES = \
	main.cpp \
	code-ranges.cpp \
	function-entries.cpp \
	intervals.cpp \
	sections.cpp \
	server.cpp

MYCPPFLAGS = $(HPC_IFLAGS) -I$(BOOST_INC) -I$(LIBELF_INC) -I$(LIBDWARF_INC) \
	-I$(SYMTABAPI_INC)

MYCXXFLAGS = @HOST_CXXFLAGS@

MYLDADD = libeh_frames.a  \
	$(HPCLIB_SupportLean) \
	-L$(SYMTABAPI_LIB) $(SYMTABAPI_LIB_LIST)

if USE_BOOST_LIBS
  MYLDADD += -L$(BOOST_LIB) $(BOOST_LIB_LIST)
endif

if OPT_DYNINST_LIBDW
  MYLDADD += -L$(LIBELF_LIB) -ldw -lelf -ldl
else
  MYLDADD += -L$(LIBDWARF_LIB) -ldwarf -L$(LIBELF_LIB) -lelf
endif

if OPT_USE_ZLIB
  MYLDADD += -L$(ZLIB_LIB) -lz
endif

MYCLEAN = @HOST_LIBTREPOSITORY@

#############################################################################
# Automake rules
#############################################################################

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

noinst_LIBRARIES = libeh_frames.a

pkglibexec_PROGRAMS = hpcfnbounds-bin
pkglibexec_SCRIPTS =  hpcfnbounds

libeh_frames_a_SOURCES = eh-frames.cpp
libeh_frames_a_CPPFLAGS = $(HPC_IFLAGS) -I$(BOOST_INC) -I$(LIBDWARF_INC)
libeh_frames_a_CXXFLAGS = $(MYCXXFLAGS)

hpcfnbounds_bin_SOURCES  = $(MYSOURCES)
hpcfnbounds_bin_CPPFLAGS = $(MYCPPFLAGS)
hpcfnbounds_bin_CXXFLAGS = $(MYCXXFLAGS)
hpcfnbounds_bin_LDADD    = $(MYLDADD)

if HOST_CPU_X86_FAMILY
  hpcfnbounds_bin_SOURCES  += x86-process-ranges.cpp amd-xop.c
  hpcfnbounds_bin_CPPFLAGS += -I$(XED2_INC)
  hpcfnbounds_bin_LDADD    += $(XED2_LIB_FLAGS)
else 
if HOST_CPU_AARCH64
  hpcfnbounds_bin_SOURCES  += arm-process-ranges.cpp
else
  hpcfnbounds_bin_SOURCES += generic-process-ranges.cpp
endif
endif

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
