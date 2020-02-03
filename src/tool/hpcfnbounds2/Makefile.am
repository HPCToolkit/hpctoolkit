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

AUTOMAKE_OPTIONS = foreign

include $(top_srcdir)/src/Makeinclude.config

#----------------------------------------------------------------------

EXT_LIBS = lib/hpctoolkit/ext-libs

LIBELF_INC = @LIBELF_INC@
LIBELF_LIB = @LIBELF_LIB@

MYSOURCES = \
	fnbounds.c \
	debug_fn.c \
	scan.c \
	server.c

MYCPPFLAGS = $(HPC_IFLAGS) -I$(LIBELF_INC)

MYCXXFLAGS = @HOST_CXXFLAGS@
MYCFLAGS   = @HOST_CFLAGS@

MYLDADD = -L$(LIBELF_LIB) -lelf

MYLDFLAGS = \
	-Wl,-rpath='$(prefix)/$(EXT_LIBS)' \
	-Wl,-rpath='$$ORIGIN/../../$(EXT_LIBS)'

#----------------------------------------------------------------------

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

pkglibexec_PROGRAMS = hpcfnbounds2

hpcfnbounds2_SOURCES  = $(MYSOURCES)
hpcfnbounds2_CPPFLAGS = $(MYCPPFLAGS)
hpcfnbounds2_CFLAGS   = $(MYCFLAGS)
hpcfnbounds2_LDADD    = $(MYLDADD)
hpcfnbounds2_LDFLAGS  = $(MYLDFLAGS)

#----------------------------------------------------------------------

include $(top_srcdir)/src/Makeinclude.rules
