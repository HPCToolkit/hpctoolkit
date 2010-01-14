# -*-Mode: makefile;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## -----------------------------------
## Part of HPCToolkit (hpctoolkit.org)
## -----------------------------------
## 
## Copyright ((c)) 2002-2009, Rice University 
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

#############################################################################
# Automake rules
#############################################################################

pkglibexecdir = $(libexecdir)/$(PACKAGE)

pkglibexec_PROGRAMS = hpcfnbounds-bin
pkglibexec_SCRIPTS =  hpcfnbounds

LIBDWARF_LIB = @LIBDWARF_LIB@
LIBELF_LIB = @LIBELF_LIB@
SYMTABAPI_INC = @SYMTABAPI_INC@
SYMTABAPI_LIB = @SYMTABAPI_LIB@
XED2_INC = @XED2_INC@
XED2_LIB = @XED2_LIB@

SYMBOLS_BASE_FILES = \
	main.cpp \
	\
	code-ranges.cpp \
	function-entries.cpp \
	intervals.cpp

hpcfnbounds_bin_SOURCES = $(SYMBOLS_BASE_FILES)

hpcfnbounds_bin_CPPFLAGS = -I$(SYMTABAPI_INC)

hpcfnbounds_bin_CXXFLAGS = $(CXXFLAGS) $(HOST_CXXFLAGS)

hpcfnbounds_bin_LDFLAGS = 			\
    -L$(SYMTABAPI_LIB) -lsymtabAPI -lcommon	\
    -L$(LIBDWARF_LIB) -ldwarf			\
    -L$(LIBELF_LIB) -lelf

if HOST_CPU_X86
    hpcfnbounds_bin_SOURCES  += x86-process-ranges.cpp
    hpcfnbounds_bin_CPPFLAGS += -I$(XED2_INC)
    hpcfnbounds_bin_LDFLAGS  += $(XED2_LIB)/libxed.a
else
    hpcfnbounds_bin_SOURCES += generic-process-ranges.cpp
endif

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
