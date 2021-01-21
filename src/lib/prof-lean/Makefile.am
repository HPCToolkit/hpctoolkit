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
AUTOMAKE_OPTIONS = foreign subdir-objects

#############################################################################
# Common settings
#############################################################################

include $(top_srcdir)/src/Makeinclude.config

#############################################################################
# Local settings
#############################################################################

MYSOURCES = \
	hpcrun-metric.h \
	\
	hpcrun-fmt.h hpcrun-fmt.c \
	hpcrunflat-fmt.h \
	\
	hpcfmt.h hpcfmt.c \
	hpcio.h hpcio.c \
	hpcio-buffer.c \
	\
	atomic.h \
	atomic-op.h atomic-op.i \
	mcs-lock.h mcs-lock.c \
	pfq-rwlock.h pfq-rwlock.c \
	spinlock.h spinlock.c \
        urand.h urand.c \
	usec_time.h usec_time.c \
	\
	BalancedTree.h BalancedTree.c \
	\
	placeholders.c \
	\
	lush/lush-support.h lush/lush-support.c \
	\
	binarytree.h binarytree.c \
	cskiplist.h cskiplist.c \
	crypto-hash.h crypto-hash.c \
	queues.h queues.c \
	stacks.h stacks.c \
	bistack.h bistack.c \
	bichannel.h bichannel.c \
	producer_wfq.h producer_wfq.c \
	generic_pair.h generic_pair.c \
	generic_val.h  mem_manager.h \
	procmaps.h procmaps.c \
	vdso.h vdso.c \
	randomizer.h randomizer.c \
	splay-uint64.h splay-uint64.c \
	elf-helper.h elf-helper.c \
	\
	id-tuple.h id-tuple.c \
	tracedb.h tracedb.c 

MYCFLAGS = @HOST_CFLAGS@ $(HPC_IFLAGS) $(MBEDTLS_IFLAGS)  -I$(LIBELF_INC)

if IS_HOST_AR
  MYAR = @HOST_AR@
else
  MYAR = $(AR) cru
endif

MYLIBADD =

MYCLEAN =

#############################################################################
# zlib
#############################################################################

if OPT_USE_ZLIB
#MYSOURCES += \
#	compress.h compress.c

#MYLIBADD   += -L$(ZLIB_LIB) -lz
#MYCFLAGS   += -I$(ZLIB_INC) 

else
#MYSOURCES += \
#	compress.h compress_dummy.c
endif

#############################################################################
# Automake rules
#############################################################################

noinst_LTLIBRARIES = libHPCprof-lean.la

libHPCprof_lean_la_SOURCES  = $(MYSOURCES)
libHPCprof_lean_la_CFLAGS   = $(MYCFLAGS)
libHPCprof_lean_la_AR       = $(MYAR)
libHPCprof_lean_la_LIBADD   = $(MYLIBADD)

MOSTLYCLEANFILES = $(MYCLEAN)

#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules

