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
## Copyright ((c)) 2002-2019, Rice University
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

# We no longer use binutils to crack instructions on any platform.
# All that's left is access to the symbol table for struct simple, and
# at some point, that will be moved to symtabapi.

AUTOMAKE_OPTIONS = foreign

include $(top_srcdir)/src/Makeinclude.config

#----------------------------------------------------------------------

MYSOURCES = \
	ISATypes.hpp \
	ISA.hpp ISA.cpp

MYCFLAGS   = @HOST_CFLAGS@   $(HPC_IFLAGS) @BINUTILS_IFLAGS@
MYCXXFLAGS = @HOST_CXXFLAGS@ $(HPC_IFLAGS) @BINUTILS_IFLAGS@

MYLIBADD = @HOST_LIBTREPOSITORY@

MYCLEAN = @HOST_LIBTREPOSITORY@

#----------------------------------------------------------------------

noinst_LTLIBRARIES = libHPCisa.la

libHPCisa_la_SOURCES  = $(MYSOURCES)
libHPCisa_la_CFLAGS   = $(MYCFLAGS)
libHPCisa_la_CXXFLAGS = $(MYCXXFLAGS)
libHPCisa_la_LIBADD   = $(MYLIBADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#----------------------------------------------------------------------

include $(top_srcdir)/src/Makeinclude.rules

