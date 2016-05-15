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
## Copyright ((c)) 2002-2016, Rice University
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

PYTHON = python


#############################################################################
# Automake rules
#############################################################################

# FIXME: tallent: hpcrun's build is highly parameterized and therefore
# will be more complex than other parts of HPCToolkit.  However, this
# makefile is surely more convoluted than necessary.  Sorting it out
# would take more time than I have right now.
#
# An initial list of parameters:
# - statically vs. dynamically linked app
# - unwinder selection
# - logical unwinding plugins
# - system, language, problem-specific sample sources
# - architecture/system/mpi-specific code (unwinder, sample sources, etc)

#-----------------------------------------------------------
# build targets
#-----------------------------------------------------------

if OPT_BGQ_BACKEND
  SUBDIRS = utilities/bgq-cnk
endif

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

plugin_srcdir = $(srcdir)/plugins
plugin_instdir = $(pkglibdir)/plugins

bin_SCRIPTS =
pkglibexec_SCRIPTS =
include_HEADERS =
pkglib_PROGRAMS =
pkglib_LIBRARIES =
pkglib_LTLIBRARIES =

if OPT_BUILD_FRONT_END
  bin_SCRIPTS += scripts/hpcsummary
  pkglibexec_SCRIPTS += scripts/hpclog
  include_HEADERS += hpctoolkit.h
endif

if OPT_ENABLE_HPCRUN_DYNAMIC
  pkglib_LTLIBRARIES += libhpcrun.la
  pkglib_LTLIBRARIES += libhpcrun_ga.la
  pkglib_LTLIBRARIES += libhpcrun_io.la
  pkglib_LTLIBRARIES += libhpcrun_memleak.la
  pkglib_LTLIBRARIES += libhpcrun_pthread.la
if OPT_ENABLE_CUDA
  pkglib_LTLIBRARIES += libhpcrun_gpu.la
endif
  pkglib_LTLIBRARIES += libhpctoolkit.la
  bin_SCRIPTS += scripts/hpcrun
endif

if OPT_ENABLE_HPCRUN_STATIC
  pkglib_PROGRAMS  += libhpcrun.o
  pkglib_LIBRARIES += libhpcrun_wrap.a
  pkglib_LIBRARIES += libhpcrun_ga_wrap.a
  pkglib_LIBRARIES += libhpcrun_io_wrap.a
  pkglib_LIBRARIES += libhpcrun_memleak_wrap.a
  pkglib_LIBRARIES += libhpcrun_pthread_wrap.a
if OPT_ENABLE_CUDA
  pkglib_LIBRARIES += libhpcrun_gpu_wrap.a
endif
  pkglib_LIBRARIES += libhpctoolkit.a
if HOST_CPU_PPC
  pkglib_LIBRARIES += libibm_xlomp_wrappers.a
endif
  bin_SCRIPTS += scripts/hpclink
endif

if OPT_ENABLE_MPI_WRAP
  pkglib_LTLIBRARIES += libhpcrun_mpi.la
endif


#-----------------------------------------------------------
# source files, part 1
#-----------------------------------------------------------

LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@
PAPI_INC_FLGS =  @OPT_PAPI_IFLAGS@
PAPI_LD_FLGS =   @OPT_PAPI_LDFLAGS@
XED2_INC = @XED2_INC@
XED2_HPCRUN_LIBS =  @XED2_HPCRUN_LIBS@
XED2_HPCLINK_LIBS = @XED2_HPCLINK_LIBS@
CUPTI_INC_FLGS = @OPT_CUPTI_IFLAGS@
CUPTI_BASE = @OPT_CUPTI@

HPCFNBOUNDS_INC = $(top_srcdir)/src/tool/hpcfnbounds

PLUGIN_CONFIG_FILES = ga io memleak pthread

if HOST_CPU_PPC
  PLUGIN_CONFIG_FILES += ibm-xlomp
endif

UNW_UNIV_FILES = \
	unwind/common/backtrace.c	\
        unwind/common/unw-throw.c

UNW_COMMON_FILES = \
        $(UNW_UNIV_FILES)               \
	unwind/common/splay.c		\
	unwind/common/stack_troll.c	\
	unwind/common/ui_tree.c

UNW_X86_FILES = \
       $(UNW_COMMON_FILES) \
	unwind/x86-family/x86-all.c			\
	unwind/x86-family/amd-xop.c                     \
	unwind/x86-family/x86-cold-path.c		\
	unwind/x86-family/x86-validate-retn-addr.c	\
	unwind/x86-family/x86-unwind-interval.c		\
	unwind/x86-family/x86-unwind-interval-fixup.c	\
	unwind/x86-family/x86-unwind.c		        \
	unwind/x86-family/x86-libunwind.c               \
	unwind/x86-family/x86-unwind-support.c		\
	unwind/x86-family/manual-intervals/x86-gcc-main64.c \
	unwind/x86-family/manual-intervals/x86-linux-dlresolver.c \
	unwind/x86-family/manual-intervals/x86-intel11-f90main.c \
	unwind/x86-family/manual-intervals/x86-intel14-omp.c \
	unwind/x86-family/manual-intervals/x86-intel-composer2013-mic.c \
	unwind/x86-family/manual-intervals/x86-32bit-main.c \
	unwind/x86-family/manual-intervals/x86-gcc-adjust.c \
	unwind/x86-family/manual-intervals/x86-32bit-icc-variant.c \
	unwind/x86-family/manual-intervals/x86-pgi-mp_pexit.c


UNW_X86_INCLUDE_DIRS = \
	-I$(srcdir)/unwind/x86-family	\
	-I$(XED2_INC)

UNW_PPC64_FILES = \
        $(UNW_COMMON_FILES) \
	unwind/ppc64/ppc64-unwind.c \
	unwind/ppc64/ppc64-unwind-interval.c

UNW_PPC64_INCLUDE_DIRS = \
        -I$(srcdir)/unwind/ppc64

UNW_PPC64_LD_FLAGS =

# generic libunwind uwinder
# NOTE: the unwinder code is specific to the
#  unwind implementation (which wraps libunwind)
# The IFLAGS and LDFLAGS related to the libunwind
# library component are handled via the LIBUNWIND
# configuration

UNW_LIBUNW_FILES = \
       $(UNW_UNIV_FILES) \
       unwind/generic-libunwind/libunw-unwind.c \
       unwind/common/default_validation_summary.c

UNW_LIBUNW_INCLUDE_DIRS = \
       -I$(srcdir)/unwind/generic-libunwind

UNW_LIBUNW_LD_FLAGS = 

UNW_MIPS_FILES = \
        $(UNW_COMMON_FILES) \
	unwind/mips/mips-unwind.c \
	unwind/mips/mips-unwind-interval.c

UNW_MIPS_INCLUDE_DIRS = \
        -I$(srcdir)/unwind/mips

UNW_MIPS_LD_FLAGS =

MY_BASE_FILES =				\
	utilities/first_func.c		\
	\
	main.h main.c			\
	disabled.c			\
        closure-registry.c              \
        cct_insert_backtrace.c          \
        cct_backtrace_finalize.c        \
	env.c				\
	epoch.c				\
	files.c				\
	handling_sample.c		\
	hpcrun-initializers.c		\
	hpcrun_options.c		\
	hpcrun_stats.c			\
	loadmap.c			\
	metrics.c			\
	name.c				\
	rank.c				\
	sample_event.c			\
	sample_prob.c			\
	sample_sources_all.c		\
	sample-sources/blame-shift/blame-shift.c \
	sample-sources/blame-shift/blame-map.c   \
	sample-sources/blame-shift/directed.c    \
	sample-sources/blame-shift/undirected.c    \
	sample-sources/omp-mutex.c      \
	sample-sources/omp-idle.c       \
        sample-sources/common.c         \
	sample-sources/ga.c		\
	sample-sources/io.c 		\
	sample-sources/itimer.c		\
	sample-sources/idle.c		\
	sample-sources/memleak.c	\
	sample-sources/pthread-blame.c  \
	sample-sources/none.c           \
        sample-sources/retcnt.c         \
        sample-sources/sample-filters.c \
        sample-sources/sync.c           \
	sample_sources_registered.c	\
	segv_handler.c			\
	start-stop.c			\
	term_handler.c			\
	thread_data.c			\
	thread_use.c			\
	threadmgr.c			\
	trace.c				\
	weak.c				\
	write_data.c		        \
	\
	cct/cct_bundle.c		\
        cct/cct_ctxt.c			\
	cct/cct.c               	\
	\
        cct2metrics.c                   \
        \
        trampoline/common/trampoline.c  \
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
	memory/mem.c			\
        memory/mmap.c                   \
	\
	messages/debug-flag.c	        \
	messages/messages-sync.c	\
	messages/messages-async.c	\
	messages/fmt.c	                \
	\
	ompt/ompt-callstack.c           \
	ompt/ompt-defer.c               \
	ompt/ompt-defer-write.c         \
	ompt/ompt-interface.c           \
	ompt/ompt-region.c              \
	ompt/ompt-region-map.c          \
	ompt/ompt-state-placeholders.c  \
	ompt/ompt-task.c                \
	ompt/ompt-thread.c              \
	ompt/ompt-task-map.c            \
	\
	utilities/executable-path.h utilities/executable-path.c \
	utilities/ip-normalized.h utilities/ip-normalized.c \
	utilities/tokenize.h utilities/tokenize.c \
	utilities/unlink.h utilities/unlink.c

if OPT_ENABLE_MPI_WRAP
MY_BASE_FILES +=  			\
	sample-sources/mpi.c
endif


MY_DYNAMIC_FILES = 			\
	fnbounds/fnbounds_client.c	\
	fnbounds/fnbounds_dynamic.c	\
	monitor-exts/openmp.c		\
	hpcrun_dlfns.c                  \
        custom-init-dynamic.c

MY_STATIC_FILES = 			\
	fnbounds/fnbounds_static.c      \
        custom-init-static.c

MY_LINUX_DYNAMIC_FILES = 		\
	os/linux/dylib.c


MY_MIPS_FILES = \
	unwind/common/default_validation_summary.c

MY_PPC_FILES = \
	unwind/common/default_validation_summary.c \
        trampoline/ppc64/ppc64-tramp.s \
        \
        utilities/arch/ppc64/ppc64-context-pc.c

MY_X86_FILES = \
        trampoline/x86-family/x86-tramp.S \
        \
        utilities/arch/x86-family/x86-context-pc.c

MY_IA64_FILES = \
        trampoline/ia64/ia64-tramp.s \
        utilities/arch/ia64/ia64-context-pc.c

if OPT_PAPI_COMPONENT
MY_PAPI_FILES = \
    sample-sources/papi-c.c \
    sample-sources/papi-c-extended-info.c
else
MY_PAPI_FILES = sample-sources/papi.c
endif

if OPT_PAPI_CUPTI
MY_PAPI_FILES += sample-sources/papi-c-cupti.c
endif

MY_UPC_FILES = sample-sources/upc.c

MY_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DINLINE_FN=1

#
# BG/Q backend requires special treatment to avoid deadlocks
#
if OPT_BGQ_BACKEND
   MY_CPP_DEFINES += -DUSE_HW_THREAD_ID -DNONZERO_THRESHOLD
endif

MY_INCLUDE_DIRS =			\
        -I$(top_srcdir)/src/tool        \
	-I$(srcdir)/messages	        \
	-I$(srcdir)/fnbounds		\
	-I$(srcdir)/memory		\
	-I$(srcdir)/os/linux		\
	-I$(srcdir)/cct			\
	-I$(srcdir)/ompt                \
	-I$(srcdir)/unwind/common	\
	-I$(srcdir)/utilities		\
	$(HPC_IFLAGS)			\
	$(OPT_LIBUNWIND_IFLAGS)         \
	-I$(HPCFNBOUNDS_INC)		\
        $(OPT_CUPTI_IFLAGS)             \
	-I$(LIBMONITOR_INC)


MY_MIPS_INCLUDE_DIRS = \
	-I$(srcdir)/unwind/mips

MY_PPC_INCLUDE_DIRS = 		\
        -I$(srcdir)/utilities/arch/ppc64 \
	-I$(srcdir)/unwind/ppc64

MY_X86_INCLUDE_DIRS =		\
        -I$(srcdir)/utilities/arch/x86-family

MY_IA64_INCLUDE_DIRS =         \
        -I$(srcdir)/utilities/arch/ia64

libhpcrun_la_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_DYNAMIC_FILES)

libhpcrun_o_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_STATIC_FILES)


libhpcrun_wrap_a_SOURCES =		\
	monitor-exts/openmp.c

libhpcrun_ga_la_SOURCES = 		\
	sample-sources/ga-overrides.c

libhpcrun_ga_wrap_a_SOURCES = 		\
	sample-sources/ga-overrides.c

libhpcrun_io_la_SOURCES = 		\
	sample-sources/io-over.c

libhpcrun_io_wrap_a_SOURCES = 		\
	sample-sources/io-over.c

libhpcrun_memleak_la_SOURCES = 		\
	sample-sources/memleak-overrides.c

libhpcrun_memleak_wrap_a_SOURCES = 	\
	sample-sources/memleak-overrides.c

libhpcrun_pthread_la_SOURCES = 		\
	sample-sources/pthread-blame-overrides.c

libhpcrun_pthread_wrap_a_SOURCES = 	\
	sample-sources/pthread-blame-overrides.c

libhpcrun_gpu_la_SOURCES = 		\
	sample-sources/gpu_blame-overrides.c \
	gpu_blame-driver-overrides-generated.c \
	gpu_blame-runtime-overrides-generated.c

libhpcrun_gpu_wrap_a_SOURCES = 	\
	sample-sources/gpu_blame-overrides.c  \
	gpu_blame-driver-overrides-generated.c \
	gpu_blame-runtime-overrides-generated.c

libhpcrun_mpi_la_SOURCES = 		\
	$(top_builddir)/src/tool/hpcrun/mpi-overrides.c

libibm_xlomp_wrappers_a_SOURCES =              \
        plugins/ibm-xlomp-wrappers.c

libhpctoolkit_la_SOURCES = 		\
	hpctoolkit.c

libhpctoolkit_a_SOURCES = 		\
	hpctoolkit.c


#-----------------------------------------------------------
# cppflags
#-----------------------------------------------------------

libhpcrun_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

if OPT_BGQ_BACKEND
  libhpcrun_la_CPPFLAGS += -I$(srcdir)/utilities/bgq-cnk
endif

libhpcrun_o_CPPFLAGS =			\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

if OPT_BGQ_BACKEND
  libhpcrun_o_CPPFLAGS += -I$(srcdir)/utilities/bgq-cnk
endif

libhpcrun_wrap_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_ga_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_ga_wrap_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_io_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_io_wrap_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_memleak_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_memleak_wrap_a_CPPFLAGS =	\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_pthread_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_pthread_wrap_a_CPPFLAGS =	\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_gpu_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_gpu_wrap_a_CPPFLAGS =	\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_mpi_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

$(top_builddir)/src/tool/hpcrun/mpi-overrides.c: sample-sources/mpi.protos
	$(PYTHON) $(srcdir)/sample-sources/make-wrappers.py \
		--f77symbol $(F77_SYMBOLS) $(srcdir)/sample-sources/mpi.protos

CUDA_TABLES = gpu_blame-cuda-driver-table.c gpu_blame-cuda-runtime-table.c
CUDA_FILES = sample-sources/gpu_blame.c sample-sources/gpu_ctxt_actions.c $(CUDA_TABLES)

if OPT_ENABLE_CUDA

generated_file_base = .
generated_file_src_path = $(srcdir)/sample-sources

generated_cuda_driver_files = $(addprefix $(generated_file_base)/, gpu_blame-cuda-driver-header.h gpu_blame-cuda-driver-table.c gpu_blame-driver-overrides-generated.c)
generated_cuda_driver_api_files = $(addprefix $(generated_file_base)/, cuda_driver_apis.h)
generated_cuda_runtime_files = $(addprefix $(generated_file_base)/, gpu_blame-cuda-runtime-header.h gpu_blame-cuda-runtime-table.c gpu_blame-runtime-overrides-generated.c)
generated_cuda_runtime_api_files = $(addprefix $(generated_file_base)/, cuda_runtime_apis.h)

BUILT_SOURCES = $(generated_cuda_driver_files) $(generated_cuda_driver_api_files) $(generated_cuda_runtime_files) $(generated_cuda_runtime_api_files)

CLEANFILES  =  $(generated_cuda_driver_files) $(generated_cuda_driver_api_files) $(generated_cuda_runtime_files) $(generated_cuda_runtime_api_files)

$(generated_cuda_runtime_api_files) : $(generated_file_src_path)/cuda_runtime_dummy.c
	$(CC) $(OPT_CUDA_IFLAGS) -E -B -c $(generated_file_src_path)/cuda_runtime_dummy.c  -o  $(generated_file_base)/cuda_runtime_apis.h 

$(generated_cuda_runtime_files) : $(generated_file_src_path)/make-cuda-wrappers.py $(generated_cuda_runtime_api_files)
	$(PYTHON) $(generated_file_src_path)/make-cuda-wrappers.py runtime $(generated_cuda_runtime_api_files) $(generated_cuda_runtime_files)

$(generated_file_base)/gpu_blame-cuda-runtime-header.h:  $(generated_file_base)/gpu_blame-cuda-runtime-table.c

$(generated_file_base)/gpu_blame-cuda-runtime-table.c: $(generated_file_base)/gpu_blame-runtime-overrides-generated.c

$(generated_cuda_driver_api_files) :  $(generated_file_src_path)/cuda_driver_dummy.c
	$(CC) $(OPT_CUDA_IFLAGS)  -E -B -c $(generated_file_src_path)/cuda_driver_dummy.c -o $(generated_file_base)/cuda_driver_apis.h

$(generated_cuda_driver_files) : $(generated_file_src_path)/make-cuda-wrappers.py $(generated_cuda_driver_api_files) 
	$(PYTHON) $(generated_file_src_path)/make-cuda-wrappers.py driver $(generated_cuda_driver_api_files)  $(generated_cuda_driver_files)

$(generated_file_base)/gpu_blame-cuda-driver-header.h : $(generated_file_base)/gpu_blame-cuda-driver-table.c

$(generated_file_base)/gpu_blame-cuda-driver-table.c : $(generated_file_base)/gpu_blame-driver-overrides-generated.c

endif

libhpctoolkit_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpctoolkit_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)


#-----------------------------------------------------------
# cflags
#-----------------------------------------------------------

libhpcrun_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_o_CFLAGS  = $(CFLAGS) $(HOST_CFLAGS)

libhpcrun_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_ga_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_ga_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_io_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_io_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_memleak_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_memleak_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_pthread_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_pthread_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_gpu_la_CFLAGS = -g $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_gpu_wrap_a_CFLAGS = -g $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_mpi_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)


#-----------------------------------------------------------
# ldflags
#-----------------------------------------------------------

libhpcrun_la_LIBADD = $(HPCLIB_ProfLean) $(HPCLIB_SupportLean)
libhpcrun_o_LDADD   = $(HPCLIB_ProfLean) $(HPCLIB_SupportLean)

if OPT_BGQ_BACKEND
  libhpcrun_la_LIBADD += utilities/bgq-cnk/libhardware-thread-id.la
  libhpcrun_o_LDADD   += utilities/bgq-cnk/libhardware-thread-id.la
endif

libhpcrun_la_LDFLAGS = -Wl,-Bsymbolic 	\
	-L$(LIBMONITOR_LIB) -lmonitor -lpthread -lrt \
        $(OPT_LIBUNWIND_LDFLAGS)

libhpcrun_ga_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_io_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_memleak_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_pthread_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_gpu_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_mpi_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_o_LDFLAGS = $(OPT_LIBUNWIND_LDFLAGS)


#-----------------------------------------------------------
# whirled peas
#-----------------------------------------------------------

if HOST_OS_LINUX
  libhpcrun_la_SOURCES += $(MY_LINUX_DYNAMIC_FILES)
endif

MY_AGENT_INCLUDE_DIRS = $(MY_INCLUDE_DIRS)

if HOST_CPU_MIPS
  libhpcrun_la_SOURCES  += $(MY_MIPS_FILES)
  libhpcrun_o_SOURCES   += $(MY_MIPS_FILES)
  libhpcrun_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_o_CPPFLAGS  += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_ga_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_gpu_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_gpu_wrap_a_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
  libhpcrun_mpi_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
endif

# Note: setting CCASFLAGS here is a no-op hack with the side effect of
# prefixing the tramp.s file names so they will be compiled separately
# for .o and .so targets.  CFLAGS does this for the .c files, but
# CFLAGS doesn't apply to .s files.  See the automake docs section
# 8.3.9.2, Objects created with both libtool and without.

if HOST_CPU_PPC
  libhpcrun_la_SOURCES  += $(MY_PPC_FILES)
  libhpcrun_o_SOURCES   += $(MY_PPC_FILES)
  libhpcrun_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_o_CPPFLAGS  += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_ga_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_gpu_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_gpu_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_mpi_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
  libhpcrun_o_CCASFLAGS  = $(AM_CCASFLAGS)
endif

if HOST_CPU_X86_FAMILY
  libhpcrun_la_SOURCES  += $(MY_X86_FILES)
  libhpcrun_o_SOURCES   += $(MY_X86_FILES)
  libhpcrun_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_o_CPPFLAGS  += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
  libhpcrun_o_CCASFLAGS  = $(AM_CCASFLAGS)
  libhpcrun_la_LDFLAGS  += $(XED2_HPCRUN_LIBS)
  libhpcrun_o_LDFLAGS   += $(XED2_HPCLINK_LIBS)
  libhpcrun_ga_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_gpu_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_gpu_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_mpi_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  MY_AGENT_INCLUDE_DIRS += $(MY_X86_INCLUDE_DIRS)
endif

if HOST_CPU_IA64
  libhpcrun_la_SOURCES  += $(MY_IA64_FILES)
  libhpcrun_o_SOURCES   += $(MY_IA64_FILES)
  libhpcrun_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_o_CPPFLAGS  += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_ga_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
  libhpcrun_mpi_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
endif

if OPT_PAPI_DYNAMIC
  libhpcrun_la_SOURCES  += $(MY_PAPI_FILES)
  libhpcrun_la_CPPFLAGS += $(PAPI_INC_FLGS)
  libhpcrun_la_LDFLAGS  += $(PAPI_LD_FLGS)
endif

if OPT_PAPI_CUPTI
libhpcrun_la_CPPFLAGS += $(CUPTI_INC_FLGS)
endif

if OPT_PAPI_STATIC
  libhpcrun_o_SOURCES   += $(MY_PAPI_FILES)
  libhpcrun_o_CPPFLAGS  += $(PAPI_INC_FLGS)
  libhpcrun_o_LDFLAGS   += $(PAPI_LD_FLGS)
endif

if OPT_ENABLE_UPC
  libhpcrun_la_SOURCES  += $(MY_UPC_FILES)
  libhpcrun_o_SOURCES   += $(MY_UPC_FILES)
  libhpcrun_la_CPPFLAGS += $(OPT_UPC_IFLAGS)
  libhpcrun_o_CPPFLAGS  += $(OPT_UPC_IFLAGS)
  libhpcrun_la_LDFLAGS  += $(OPT_UPC_LDFLAGS)
endif

if OPT_ENABLE_LUSH_PTHREADS
  libhpcrun_la_CPPFLAGS += -DLUSH_PTHREADS
  libhpcrun_o_CPPFLAGS  += -DLUSH_PTHREADS
endif


#-----------------------------------------------------------
# source files, part 2.3
#-----------------------------------------------------------

if UNW_X86
  UNW_SOURCE_FILES = $(UNW_X86_FILES)
  UNW_INCLUDE_DIRS = $(UNW_X86_INCLUDE_DIRS)
  UNW_DYNAMIC_LD_FLAGS = $(XED2_HPCRUN_LIBS)
  UNW_STATIC_LD_FLAGS =  $(XED2_HPCLINK_LIBS)
endif

if UNW_PPC64
  UNW_SOURCE_FILES = $(UNW_PPC64_FILES)
  UNW_INCLUDE_DIRS = $(UNW_PPC64_INCLUDE_DIRS)
  UNW_DYNAMIC_LD_FLAGS = $(UNW_PPC64_LD_FLAGS)
  UNW_STATIC_LD_FLAGS =  $(UNW_PPC64_LD_FLAGS)
endif

if OPT_ENABLE_CUDA
  libhpcrun_la_SOURCES += $(CUDA_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_CUDA
  libhpcrun_la_CFLAGS  += $(OPT_CUDA_IFLAGS) -g
#  libhpcrun_la_LDFLAGS += $(OPT_CUDA_LDFLAGS)

  libhpcrun_gpu_la_CPPFLAGS += -DENABLE_CUDA
  libhpcrun_gpu_la_CFLAGS  += $(OPT_CUDA_IFLAGS)
  libhpcrun_gpu_la_LDFLAGS += $(OPT_CUDA_LDFLAGS)

  libhpcrun_gpu_wrap_a_CPPFLAGS += -DENABLE_CUDA
  libhpcrun_gpu_wrap_a_CFLAGS  += $(OPT_CUDA_IFLAGS)

endif

if UNW_LIBUNW
  UNW_SOURCE_FILES = $(UNW_LIBUNW_FILES)
  UNW_INCLUDE_DIRS = $(UNW_LIBUNW_INCLUDE_DIRS)
  UNW_DYNAMIC_LD_FLAGS = $(UNW_LIBUNW_LD_FLAGS)
  UNW_STATIC_LD_FLAGS =  $(UNW_LIBUNW_LD_FLAGS)
endif

libhpcrun_la_SOURCES  +=  $(UNW_SOURCE_FILES)
libhpcrun_o_SOURCES   +=  $(UNW_SOURCE_FILES)
libhpcrun_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_o_CPPFLAGS  += $(UNW_INCLUDE_DIRS)
libhpcrun_ga_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_ga_wrap_a_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_io_wrap_a_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_memleak_wrap_a_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_pthread_wrap_a_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_gpu_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_gpu_wrap_a_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_la_LDFLAGS  += $(UNW_DYNAMIC_LD_FLAGS)
libhpcrun_o_LDFLAGS   += $(UNW_STATIC_LD_FLAGS)
MY_AGENT_INCLUDE_DIRS += $(UNW_INCLUDE_DIRS)

libhpcrun_la_SOURCES += utilities/last_func.c
libhpcrun_o_SOURCES  += utilities/last_func.c


#-----------------------------------------------------------
# whirled peas, LUSH-style
#-----------------------------------------------------------

MY_AGENT_RTSUPPORT = lush/lush-support-rt.h lush/lush-support-rt.c

MY_AGENT_CILK_SOURCES = \
	lush-agents/agent-cilk.h lush-agents/agent-cilk.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_CILK_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(OPT_CILK_IFLAGS) \
	$(MY_AGENT_INCLUDE_DIRS)


MY_AGENT_PTHREAD_SOURCES = \
	lush-agents/agent-pthread.h lush-agents/agent-pthread.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_PTHREAD_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(MY_AGENT_INCLUDE_DIRS)


MY_AGENT_TBB_SOURCES = \
	lush-agents/agent-tbb.h lush-agents/agent-tbb.c \
	$(MY_AGENT_RTSUPPORT)

MY_AGENT_TBB_CFLAGS = \
	$(CFLAGS) $(HOST_CFLAGS) \
	$(MY_AGENT_INCLUDE_DIRS)


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


#-----------------------------------------------------------
# local hooks
#-----------------------------------------------------------

# Install the plugin config files.

install-data-hook:
	mkdir -p "$(DESTDIR)$(plugin_instdir)"
	-for file in $(PLUGIN_CONFIG_FILES) ; do \
	    cp -f "$(plugin_srcdir)/$$file" "$(DESTDIR)$(plugin_instdir)/$$file" ; \
	done


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
