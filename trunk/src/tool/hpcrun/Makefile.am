##
##  Csprof Makefile.am.
##
##  $Id$
##

include $(top_srcdir)/src/Makeinclude.config

pkglibexecdir = $(libexecdir)/$(PACKAGE)

pkglib_LTLIBRARIES = libcsprof.la
pkglib_PROGRAMS = libcsprof.o
bin_SCRIPTS = scripts/hpcrun scripts/hpclink
pkglibexec_SCRIPTS = scripts/hpcfnbounds

LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@
PAPI_INC_FLGS = @OPT_PAPI_IFLAGS@
PAPI_LD_FLGS =  @OPT_PAPI_LDFLAGS@
SYMTABAPI_INC =  @SYMTABAPI_INC@
XED2_INC = @XED2_INC@
XED2_LIB = @XED2_LIB@

PROF_LEAN_INC = $(top_srcdir)/src/lib/prof-lean

CSPROF_BASE_FILES =			\
	utilities/first_func.c		\
	sample_sources_all.c		\
	sample_source_common.c		\
	csprof_options.c		\
	csproflib.c			\
	data.c				\
	env.c				\
	epoch.c				\
	files.c				\
	handling_sample.c		\
	sample_source_itimer.c		\
	last.c				\
	libmonitor_upcalls.c		\
	metrics.c			\
	name.c				\
	sample_sources_registered.c	\
	sample_event.c			\
	segv_handler.c			\
	state.c				\
	system_server.c			\
	thread_data.c			\
	trace.c				\
	unsafe.c			\
	Dbg/pmsg.c			\
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
	unwind/common/intervals.c	\
	unwind/common/splay.c		\
	unwind/common/stack_troll.c	\
	unwind/common/unwind.c		\
	utilities/executable-path.c	\
	utilities/spinlock.c		\
	utilities/tokenize.c		\
	utilities/unlink.c

CSPROF_DYNAMIC_FILES = 			\
	fnbounds/fnbounds_dynamic.c	\
	csprof_dlfns.c

CSPROF_STATIC_FILES = 			\
	fnbounds/fnbounds_static.c

CSPROF_LINUX_DYNAMIC_FILES = 		\
	os/linux/dylib.c

CSPROF_PPC_FILES =				\
	unwind/ppc64/ppc64-build-intervals.c	\
	unwind/ppc64/ppc64-context.c		\
	unwind/ppc64/ppc64-unwind-support.c

CSPROF_X86_FILES =					\
	processor/x86-64/processor-libc.c		\
	processor/x86-64/swizzle.c			\
	unwind/x86-family/x86-all.c			\
	unwind/x86-family/x86-unwind-interval-fixup.c	\
	unwind/x86-family/x86-unwind-support.c		\
	unwind/x86-family/manual-intervals/x86-linux-dlresolver.c

CSPROF_PAPI_FILES = 			\
	sample_source_papi.c

CSPROF_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DCSPROF_LAST_EDGE_ONLY=1	\
	-DCSPROF_PERF=1			\
	-DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1	\
	-DCSPROF_THREADS=1		\
	-DINLINE_FN=1

CSPROF_INCLUDE_DIRS =			\
	-I$(srcdir)/Dbg			\
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
	-I$(LIBMONITOR_INC)		\
	-I$(PROF_LEAN_INC)		\
	-I$(SYMTABAPI_INC)

CSPROF_PPC_INCLUDE_DIRS = 		\
	-I$(srcdir)/unwind/ppc64

CSPROF_X86_INCLUDE_DIRS =		\
	-I$(srcdir)/processor/x86-64	\
	-I$(srcdir)/unwind/x86-family	\
	-I$(XED2_INC)

libcsprof_la_SOURCES = 			\
	$(CSPROF_BASE_FILES)		\
	$(CSPROF_DYNAMIC_FILES)

libcsprof_o_SOURCES = 			\
	$(CSPROF_BASE_FILES)		\
	$(CSPROF_STATIC_FILES)

libcsprof_la_CPPFLAGS =			\
	$(CSPROF_CPP_DEFINES)		\
	$(CSPROF_INCLUDE_DIRS)

libcsprof_o_CPPFLAGS =			\
	-DHPCRUN_STATIC_LINK		\
	$(CSPROF_CPP_DEFINES)		\
	$(CSPROF_INCLUDE_DIRS)

libcsprof_la_CFLAGS = -std=c99 $(CFLAGS) $(HOST_CFLAGS)
libcsprof_o_CFLAGS  = -std=c99 $(CFLAGS) $(HOST_CFLAGS)

libcsprof_la_LIBADD = $(HPC_LIBPROFLEAN)
libcsprof_o_LDADD   = $(HPC_LIBPROFLEAN)

libcsprof_la_LDFLAGS = -Wl,-Bsymbolic 	\
	-L$(LIBMONITOR_LIB) -lmonitor -lpthread

libcsprof_o_LDFLAGS =

if HOST_OS_LINUX
    libcsprof_la_SOURCES += $(CSPROF_LINUX_DYNAMIC_FILES)
endif

if HOST_CPU_PPC
    libcsprof_la_SOURCES  += $(CSPROF_PPC_FILES)
    libcsprof_o_SOURCES   += $(CSPROF_PPC_FILES)
    libcsprof_la_CPPFLAGS += $(CSPROF_PPC_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(CSPROF_PPC_INCLUDE_DIRS)
    libcsprof_la_CFLAGS += -D__ppc64__
    libcsprof_o_CFLAGS  += -D__ppc64__
endif

if HOST_CPU_X86
    libcsprof_la_SOURCES  += $(CSPROF_X86_FILES)
    libcsprof_o_SOURCES   += $(CSPROF_X86_FILES)
    libcsprof_la_CPPFLAGS += $(CSPROF_X86_INCLUDE_DIRS)
    libcsprof_o_CPPFLAGS  += $(CSPROF_X86_INCLUDE_DIRS)
    libcsprof_la_LDFLAGS  += $(XED2_LIB)/libxed.a
    libcsprof_o_LDFLAGS   += $(XED2_LIB)/libxed.a
endif

if OPT_HAVE_PAPI
    libcsprof_la_SOURCES  += $(CSPROF_PAPI_FILES)
    libcsprof_o_SOURCES   += $(CSPROF_PAPI_FILES)
    libcsprof_la_CPPFLAGS += $(PAPI_INC_FLGS)
    libcsprof_o_CPPFLAGS  += $(PAPI_INC_FLGS)
    libcsprof_la_LDFLAGS  += $(PAPI_LD_FLGS)
    libcsprof_o_LDFLAGS   += $(PAPI_LD_FLGS)
endif

libcsprof_la_SOURCES += utilities/last_func.c
libcsprof_o_SOURCES  += utilities/last_func.c


# FIXME: This is specific to Rice.
MY_CILK_INSTALL = /home/eraxxon/develop/cilk/cilk-install

MY_CILK_AGENT_SOURCES = \
	lush-agents/agent-cilk.h lush-agents/agent-cilk.c \
	lush/lush-support-rt.h   lush/lush-support-rt.c

MY_CILK_AGENT_IFLAGS = -I$(MY_CILK_INSTALL)/include

MY_CILK_AGENT_CFLAGS = \
	$(MY_CILK_AGENT_IFLAGS) \
	$(CSPROF_INCLUDE_DIRS)

if OPT_ENABLE_LUSH
  pkglib_LTLIBRARIES       += libagent-cilk.la
  libagent_cilk_la_SOURCES  = $(MY_CILK_AGENT_SOURCES)
  libagent_cilk_la_CFLAGS   = $(MY_CILK_AGENT_CFLAGS)
endif

