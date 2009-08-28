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

pkglib_LTLIBRARIES =
bin_SCRIPTS = scripts/hpcsummary

if OPT_ENABLE_HPCRUN_DYNAMIC
  pkglib_LTLIBRARIES += libcsprof.la
  bin_SCRIPTS += scripts/hpcrun
endif

if OPT_ENABLE_HPCRUN_STATIC
  pkglib_PROGRAMS = libcsprof.o
  bin_SCRIPTS += scripts/hpclink
endif

if OPT_ENABLE_MONITOR_EXTS
  pkglib_LIBRARIES = libcsprof_wrap.a
endif

pkglibexec_SCRIPTS = scripts/hpclog


LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@
PAPI_INC_FLGS =  @OPT_PAPI_IFLAGS@
PAPI_LD_FLGS =   @OPT_PAPI_LDFLAGS@
SYMTABAPI_INC =  @SYMTABAPI_INC@
XED2_INC = @XED2_INC@
XED2_LIB = @XED2_LIB@

HPCFNBOUNDS_INC = $(top_srcdir)/src/tool/hpcfnbounds

MY_BASE_FILES =			\
	utilities/first_func.c		\
	\
	main.h main.c			\
	csprof_options.c		\
	csproflib.c			\
	data.c				\
	disabled.c			\
	env.c				\
	epoch.c				\
	files.c				\
	handling_sample.c		\
	last.c				\
	metrics.c			\
	name.c				\
	sample_event.c			\
	sample_sources_all.c		\
	sample_source_common.c		\
	sample_source_itimer.c		\
	sample_source_none.c            \
	sample_sources_registered.c	\
	segv_handler.c			\
	state.c				\
	system_server.c			\
	thread_data.c			\
	thread_use.c			\
	trace.c				\
	unsafe.c			\
	write_data.c		        \
	\
	cct/cct.c			\
	\
	unwind/common/backtrace.c	\
	unwind/common/splay.c		\
	unwind/common/stack_troll.c	\
	unwind/common/ui_tree.c		\
	\
	lush/lush-backtrace.h lush/lush-backtrace.c \
	lush/lush.h lush/lush.c	\
	lush/lush-pthread.h lush/lush-pthread.i lush/lush-pthread.c \
	lush/lush-support-rt.h lush/lush-support-rt.c \
	\
	lush/lushi.h \
	lush/lushi-cb.h	lush/lushi-cb.c	\
	\
	fnbounds/fnbounds_common.c	\
	\
	memory/list.c			\
	memory/mem.c			\
	\
	messages/debug-flag.c	        \
	messages/messages-sync.c	\
	messages/messages-async.c	\
	messages/fmt.c	                \
	\
	utilities/executable-path.h utilities/executable-path.c \
	utilities/tokenize.h utilities/tokenize.c \
	utilities/unlink.h utilities/unlink.c


MY_DYNAMIC_FILES = 			\
	fnbounds/fnbounds_dynamic.c	\
	csprof_dlfns.c

MY_STATIC_FILES = 			\
	fnbounds/fnbounds_static.c

MY_LINUX_DYNAMIC_FILES = 		\
	os/linux/dylib.c

MY_MONITOR_EXT_FILES = \
	monitor-exts/monitor_ext.h  \
	monitor-exts/over_cond.c  \
	monitor-exts/over_mutex.c \
	monitor-exts/over_spin.c


MY_MIPS_FILES = \
	unwind/common/default_validation_summary.c \
	unwind/mips/mips-unwind.c \
	unwind/mips/mips-unwind-interval.c

MY_PPC_FILES = \
	unwind/common/default_validation_summary.c \
	unwind/ppc64/ppc64-unwind.c \
	unwind/ppc64/ppc64-unwind-interval.c

MY_X86_FILES = \
	unwind/x86-family/x86-all.c			\
	unwind/x86-family/x86-cold-path.c		\
	unwind/x86-family/x86-validate-retn-addr.c	\
	unwind/x86-family/x86-unwind-interval.c		\
	unwind/x86-family/x86-unwind-interval-fixup.c	\
	unwind/x86-family/x86-unwind.c		        \
	unwind/x86-family/x86-unwind-support.c		\
	unwind/x86-family/manual-intervals/x86-linux-dlresolver.c


MY_PAPI_FILES = 			\
	sample_source_papi.c

MY_UPC_FILES = sample_source_upc.c

MY_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DCSPROF_LAST_EDGE_ONLY=1	\
	-DCSPROF_PERF=1			\
	-DCSPROF_THREADS=1		\
	-DINLINE_FN=1

MY_INCLUDE_DIRS =			\
	-I$(srcdir)/messages	        \
	-I$(srcdir)/fnbounds		\
	-I$(srcdir)/memory		\
	-I$(srcdir)/os/catamount	\
	-I$(srcdir)/os/linux		\
	-I$(srcdir)/cct			\
	-I$(srcdir)/unwind/common	\
	-I$(srcdir)/utilities		\
	$(HPC_IFLAGS)			\
	-I$(HPCFNBOUNDS_INC)		\
	-I$(LIBMONITOR_INC)		\
	-I$(SYMTABAPI_INC)


MY_MIPS_INCLUDE_DIRS = \
	-I$(srcdir)/unwind/mips

MY_PPC_INCLUDE_DIRS = 		\
	-I$(srcdir)/unwind/ppc64

MY_X86_INCLUDE_DIRS =		\
	-I$(srcdir)/unwind/x86-family	\
	-I$(XED2_INC)


libcsprof_la_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_DYNAMIC_FILES)

libcsprof_o_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_STATIC_FILES)

libcsprof_wrap_a_SOURCES =		\
	$(MY_MONITOR_EXT_FILES)

libcsprof_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libcsprof_o_CPPFLAGS =			\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libcsprof_wrap_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libcsprof_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libcsprof_o_CFLAGS  = $(CFLAGS) $(HOST_CFLAGS)
libcsprof_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)

libcsprof_la_LIBADD = $(HPC_LIBPROFLEAN)
libcsprof_o_LDADD   = $(HPC_LIBPROFLEAN)

libcsprof_la_LDFLAGS = -Wl,-Bsymbolic 	\
	-L$(LIBMONITOR_LIB) -lmonitor -lpthread -lrt

libcsprof_o_LDFLAGS =

if HOST_OS_LINUX
    libcsprof_la_SOURCES += $(MY_LINUX_DYNAMIC_FILES)
endif


if HOST_CPU_MIPS
    libcsprof_la_SOURCES  += $(MY_MIPS_FILES)
    libcsprof_o_SOURCES   += $(MY_MIPS_FILES)
    libcsprof_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(MY_MIPS_INCLUDE_DIRS)
endif

if HOST_CPU_PPC
    libcsprof_la_SOURCES  += $(MY_PPC_FILES)
    libcsprof_o_SOURCES   += $(MY_PPC_FILES)
    libcsprof_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(MY_PPC_INCLUDE_DIRS)
endif

if HOST_CPU_X86
    libcsprof_la_SOURCES  += $(MY_X86_FILES)
    libcsprof_o_SOURCES   += $(MY_X86_FILES)
    libcsprof_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(MY_X86_INCLUDE_DIRS)
    libcsprof_la_LDFLAGS  += $(XED2_LIB)/libxed.a
    libcsprof_o_LDFLAGS   += $(XED2_LIB)/libxed.a
endif

if OPT_PAPI_DYNAMIC
    libcsprof_la_SOURCES  += $(MY_PAPI_FILES)
    libcsprof_la_CPPFLAGS += $(PAPI_INC_FLGS)
    libcsprof_la_LDFLAGS  += $(PAPI_LD_FLGS)
endif

if OPT_PAPI_STATIC
    libcsprof_o_SOURCES   += $(MY_PAPI_FILES)
    libcsprof_o_CPPFLAGS  += $(PAPI_INC_FLGS)
    libcsprof_o_LDFLAGS   += $(PAPI_LD_FLGS)
endif

if OPT_ENABLE_UPC
    libcsprof_la_SOURCES  += $(MY_UPC_FILES)
    libcsprof_o_SOURCES   += $(MY_UPC_FILES)
    libcsprof_la_CPPFLAGS += $(OPT_UPC_IFLAGS)
    libcsprof_o_CPPFLAGS  += $(OPT_UPC_IFLAGS)
    libcsprof_la_LDFLAGS  += $(OPT_UPC_LDFLAGS)
    libcsprof_o_LDFLAGS   += $(OPT_UPC_LDFLAGS)
endif

if OPT_ENABLE_LUSH_PTHREADS
    libcsprof_la_CPPFLAGS += -DLUSH_PTHREADS
    libcsprof_o_CPPFLAGS  += -DLUSH_PTHREADS
endif

if OPT_ENABLE_MONITOR_EXTS
    libcsprof_la_SOURCES  += $(MY_MONITOR_EXT_FILES)
    libcsprof_la_CPPFLAGS += -DHPCRUN_MONITOR_EXTS
    libcsprof_o_CPPFLAGS  += -DHPCRUN_MONITOR_EXTS
endif


libcsprof_la_SOURCES += utilities/last_func.c
libcsprof_o_SOURCES  += utilities/last_func.c


# FIXME: This is specific to Rice.

MY_AGENT_RTSUPPORT = lush/lush-support-rt.h lush/lush-support-rt.c

MY_AGENT_CILK_SOURCES = \
	lush-agents/agent-cilk.h lush-agents/agent-cilk.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_CILK_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(OPT_CILK_IFLAGS) \
	$(MY_INCLUDE_DIRS)


MY_AGENT_PTHREAD_SOURCES = \
	lush-agents/agent-pthread.h lush-agents/agent-pthread.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_PTHREAD_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(MY_INCLUDE_DIRS)


MY_AGENT_TBB_SOURCES = \
	lush-agents/agent-tbb.h lush-agents/agent-tbb.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_TBB_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(MY_INCLUDE_DIRS)


if OPT_ENABLE_LUSH
if OPT_WITH_CILK
  pkglib_LTLIBRARIES       += libagent-cilk.la
  libagent_cilk_la_SOURCES  = $(MY_AGENT_CILK_SOURCES)
  libagent_cilk_la_CFLAGS   = $(MY_AGENT_CILK_CFLAGS)
endif

  pkglib_LTLIBRARIES       += libagent-pthread.la
  libagent_pthread_la_SOURCES  = $(MY_AGENT_PTHREAD_SOURCES)
  libagent_pthread_la_CFLAGS   = $(MY_AGENT_PTHREAD_CFLAGS)

  pkglib_LTLIBRARIES       += libagent-tbb.la
  libagent_tbb_la_SOURCES  = $(MY_AGENT_TBB_SOURCES)
  libagent_tbb_la_CFLAGS   = $(MY_AGENT_TBB_CFLAGS)
endif


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
