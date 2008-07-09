##
##  Csprof src/Makefile.am.
##
##  $Id: Makefile.am 151 2008-06-27 17:20:51Z krentel $
##

CSPROF_BASE_FILES =			\
	utilities/first_func.c		\
	all_sample_sources.c		\
	common.sample_source.c		\
	csprof_options.c		\
	csproflib.c			\
	data.c				\
	env.c				\
	epoch.c				\
	files.c				\
	handling_sample.c		\
	itimer.sample_source.c		\
	last.c				\
	libmonitor_upcalls.c		\
	metrics.c			\
	name.c				\
	registered_sample_sources.c	\
	sample_event.c			\
	segv_handler.c			\
	state.c				\
	system_server.c			\
	thread_data.c			\
	trace.c				\
	unsafe.c			\
	Dbg/pmsg.c			\
	fnbounds/fnbounds_common.c	\
	hpcfile/hpcfile_csprof.c	\
	hpcfile/hpcfile_cstree.c	\
	hpcfile/hpcfile_general.c	\
	lush/lush.c			\
	lush/lush-support.c		\
	lush/lush-support-rt.c		\
	lush/lushi-cb.c			\
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

CSPROF_LINUX_DYNAMIC_FILES = os/linux/dylib.c

CSPROF_PPC_FILES =				\
	unwind/ppc64/ppc64-build-intervals.c	\
	unwind/ppc64/ppc64-context.c		\
	unwind/ppc64/ppc64-unwind-support.c

CSPROF_X86_FILES =				\
	processor/x86-64/processor-libc.c	\
	processor/x86-64/swizzle.c		\
	unwind/x86-family/x86-all.c		\
	unwind/x86-family/x86-unwind-support.c

CSPROF_PAPI_FILES = papi.sample_source.c

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
	-I$(srcdir)/hpcfile		\
	-I$(srcdir)/lush		\
	-I$(srcdir)/lush-agents		\
	-I$(srcdir)/memory		\
	-I$(srcdir)/os/catamount	\
	-I$(srcdir)/os/linux		\
	-I$(srcdir)/processor/x86-64	\
	-I$(srcdir)/sampling/types/callstack	\
	-I$(srcdir)/unwind/common	\
	-I$(srcdir)/unwind/x86-family	\
	-I$(srcdir)/utilities

CSPROF_EXTERN_INCLUDES = @csprof_extern_includes@

lib_LTLIBRARIES = libcsprof.la
libexec_PROGRAMS = libcsprof.o
bin_SCRIPTS = scripts-new/csprof scripts-new/csprof-link
libexec_SCRIPTS = scripts-new/csprof_syms

libcsprof_la_SOURCES = 			\
	$(CSPROF_BASE_FILES)		\
	$(CSPROF_DYNAMIC_FILES)

libcsprof_o_SOURCES = 			\
	$(CSPROF_BASE_FILES)		\
	$(CSPROF_STATIC_FILES)

libcsprof_la_CPPFLAGS =			\
	$(CSPROF_CPP_DEFINES)		\
	$(CSPROF_INCLUDE_DIRS)		\
	$(CSPROF_EXTERN_INCLUDES)

libcsprof_o_CPPFLAGS =			\
	-DHPCRUN_STATIC_LINK		\
	$(CSPROF_CPP_DEFINES)		\
	$(CSPROF_INCLUDE_DIRS)		\
	$(CSPROF_EXTERN_INCLUDES)

libcsprof_la_CFLAGS = -std=c99
libcsprof_o_CFLAGS  = -std=c99

libcsprof_la_LDFLAGS = -Wl,-Bsymbolic 	\
	-L@libmonitor_prefix@/lib -lmonitor -lpthread

if CSPROF_AMTEST_IS_LINUX
    libcsprof_la_SOURCES += $(CSPROF_LINUX_DYNAMIC_FILES)
endif

if CSPROF_AMTEST_IS_PPC
    libcsprof_la_SOURCES += $(CSPROF_PPC_FILES)
    libcsprof_o_SOURCES  += $(CSPROF_PPC_FILES)
    libcsprof_la_CFLAGS += -D__ppc64__
    libcsprof_o_CFLAGS  += -D__ppc64__
endif

if CSPROF_AMTEST_IS_X86
    libcsprof_la_SOURCES += $(CSPROF_X86_FILES)
    libcsprof_o_SOURCES  += $(CSPROF_X86_FILES)
    libcsprof_la_LDFLAGS += -L@xed2_prefix@/lib -lxed
endif

if CSPROF_AMTEST_USE_PAPI
    libcsprof_la_SOURCES += $(CSPROF_PAPI_FILES)
    libcsprof_o_SOURCES  += $(CSPROF_PAPI_FILES)
    libcsprof_la_LDFLAGS += -L@papi_prefix@/lib -lpapi @papi_libs@
endif

libcsprof_la_SOURCES += utilities/last_func.c
libcsprof_o_SOURCES  += utilities/last_func.c

