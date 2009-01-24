# -*-Mode: makefile;-*-
## $Id$

#############################################################################
#
# File:
#   $Source$
#
# Description:
#   *Process with automake to produce Makefile.in*
#   
#   Note: All local variables are prefixed with MY to prevent name
#   clashes with automatic automake variables.
#
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

pkglib_LTLIBRARIES = libcsprof.la

bin_SCRIPTS = scripts/hpcrun

if OPT_ENABLE_HPCRUN_STATIC
  pkglib_PROGRAMS = libcsprof.o

  bin_SCRIPTS += scripts/hpclink
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
PROF_LEAN_INC = $(top_srcdir)/src/lib/prof-lean

MY_BASE_FILES =			\
	utilities/first_func.c		\
	csprof_options.c		\
	csproflib.c			\
	data.c				\
	env.c				\
	epoch.c				\
	files.c				\
	handling_sample.c		\
	last.c				\
	libmonitor_upcalls.c		\
	metrics.c			\
	name.c				\
	sample_event.c			\
	sample_sources_all.c		\
	sample_source_common.c		\
	sample_source_itimer.c		\
	sample_sources_registered.c	\
	segv_handler.c			\
	state.c				\
	system_server.c			\
	thread_data.c			\
	thread_use.c			\
	trace.c				\
	unsafe.c			\
	messages/pmsg.c			\
	fnbounds/fnbounds_common.c	\
	lush/lush-backtrace.c		\
	lush/lush.c			\
	lush/lushi-cb.c			\
	lush/lush-support-rt.c		\
	memory/list.c			\
	memory/mem.c			\
	sampling/types/callstack/cct.c	\
	sampling/types/callstack/dump_backtraces.c	\
	unwind/common/backtrace.c	\
	unwind/common/splay.c		\
	unwind/common/stack_troll.c	\
	utilities/executable-path.c	\
	utilities/spinlock.c		\
	utilities/tokenize.c		\
	utilities/unlink.c

MY_DYNAMIC_FILES = 			\
	fnbounds/fnbounds_dynamic.c	\
	csprof_dlfns.c

MY_STATIC_FILES = 			\
	fnbounds/fnbounds_static.c

MY_LINUX_DYNAMIC_FILES = 		\
	os/linux/dylib.c


MY_MIPS_FILES = \
	unwind/mips/mips-unwind.c \
	unwind/mips/mips-unwind-interval.c

MY_PPC_FILES = \
	unwind/ppc64/ppc64-build-intervals.c	\
	unwind/ppc64/ppc64-context.c		\
	unwind/ppc64/ppc64-unwind-interval.c    \
	unwind/ppc64/ppc64-unwind-support.c

MY_X86_FILES = \
	processor/x86-64/processor-libc.c		\
	processor/x86-64/swizzle.c			\
	unwind/x86-family/x86-all.c			\
	unwind/x86-family/x86-unwind-interval.c		\
	unwind/x86-family/x86-unwind-interval-fixup.c	\
	unwind/x86-family/x86-unwind.c		        \
	unwind/x86-family/x86-unwind-support.c		\
	unwind/x86-family/manual-intervals/x86-linux-dlresolver.c


MY_PAPI_FILES = 			\
	sample_source_papi.c

MY_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DCSPROF_LAST_EDGE_ONLY=1	\
	-DCSPROF_PERF=1			\
	-DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1	\
	-DCSPROF_THREADS=1		\
	-DINLINE_FN=1

MY_INCLUDE_DIRS =			\
	-I$(srcdir)/messages	        \
	-I$(srcdir)/fnbounds		\
	-I$(srcdir)/lush		\
	-I$(srcdir)/lush-agents		\
	-I$(srcdir)/memory		\
	-I$(srcdir)/os/catamount	\
	-I$(srcdir)/os/linux		\
	-I$(srcdir)/sampling/types/callstack	\
	-I$(srcdir)/unwind/common	\
	-I$(srcdir)/utilities		\
	$(HPC_IFLAGS)			\
	-I$(HPCFNBOUNDS_INC)		\
	-I$(LIBMONITOR_INC)		\
	-I$(PROF_LEAN_INC)		\
	-I$(SYMTABAPI_INC)


MY_MIPS_INCLUDE_DIRS = \
	-I$(srcdir)/processor/mips \
	-I$(srcdir)/unwind/mips

MY_PPC_INCLUDE_DIRS = 		\
	-I$(srcdir)/unwind/ppc64

MY_X86_INCLUDE_DIRS =		\
	-I$(srcdir)/processor/x86-64	\
	-I$(srcdir)/unwind/x86-family	\
	-I$(XED2_INC)


libcsprof_la_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_DYNAMIC_FILES)

libcsprof_o_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_STATIC_FILES)

libcsprof_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libcsprof_o_CPPFLAGS =			\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libcsprof_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libcsprof_o_CFLAGS  = $(CFLAGS) $(HOST_CFLAGS)

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
    libcsprof_la_CFLAGS += -D__ppc64__
    libcsprof_o_CFLAGS  += -D__ppc64__
endif

if HOST_CPU_X86
    libcsprof_la_SOURCES  += $(MY_X86_FILES)
    libcsprof_o_SOURCES   += $(MY_X86_FILES)
    libcsprof_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(MY_X86_INCLUDE_DIRS)
    libcsprof_la_LDFLAGS  += $(XED2_LIB)/libxed.a
    libcsprof_o_LDFLAGS   += $(XED2_LIB)/libxed.a
endif


if OPT_HAVE_PAPI
    libcsprof_la_SOURCES  += $(MY_PAPI_FILES)
    libcsprof_o_SOURCES   += $(MY_PAPI_FILES)
    libcsprof_la_CPPFLAGS += $(PAPI_INC_FLGS)
    libcsprof_o_CPPFLAGS  += $(PAPI_INC_FLGS)
    libcsprof_la_LDFLAGS  += $(PAPI_LD_FLGS)
    libcsprof_o_LDFLAGS   += $(PAPI_LD_FLGS)
endif

libcsprof_la_SOURCES += utilities/last_func.c
libcsprof_o_SOURCES  += utilities/last_func.c


# FIXME: This is specific to Rice.

MY_AGENT_RTSUPPORT = lush/lush-support-rt.h lush/lush-support-rt.c

MY_CILK_INSTALL = /home/eraxxon/develop/cilk/cilk-install

MY_AGENT_CILK_SOURCES = \
	lush-agents/agent-cilk.h lush-agents/agent-cilk.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_CILK_IFLAGS = -I$(MY_CILK_INSTALL)/include

MY_AGENT_CILK_CFLAGS = \
	$(MY_AGENT_CILK_IFLAGS) \
	$(MY_INCLUDE_DIRS)


MY_AGENT_PTHREAD_SOURCES = \
	lush-agents/agent-pthread.h lush-agents/agent-pthread.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_PTHREAD_CFLAGS = \
	$(MY_INCLUDE_DIRS)


MY_AGENT_TBB_SOURCES = \
	lush-agents/agent-tbb.h lush-agents/agent-tbb.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_TBB_CFLAGS = \
	$(MY_INCLUDE_DIRS)


if OPT_ENABLE_LUSH
  pkglib_LTLIBRARIES       += libagent-cilk.la
  libagent_cilk_la_SOURCES  = $(MY_AGENT_CILK_SOURCES)
  libagent_cilk_la_CFLAGS   = $(MY_AGENT_CILK_CFLAGS)

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
