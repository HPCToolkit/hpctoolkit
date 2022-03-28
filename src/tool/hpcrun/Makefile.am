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
## Copyright ((c)) 2002-2022, Rice University
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
pkglib_LIBRARIES =
pkglib_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES =

if OPT_BUILD_FRONT_END
  pkglibexec_SCRIPTS += scripts/hpcsummary
  pkglibexec_SCRIPTS += scripts/hpclog
  include_HEADERS += hpctoolkit.h
endif

if OPT_ENABLE_HPCRUN_DYNAMIC
  noinst_LTLIBRARIES = libhpcrun.la
  pkglib_LTLIBRARIES += libhpcrun_fake_audit.la
  pkglib_LTLIBRARIES += libhpcrun_audit.la
  pkglib_LTLIBRARIES += libhpcrun_dlmopen.la
  pkglib_LTLIBRARIES += libhpcrun_ga.la
  pkglib_LTLIBRARIES += libhpcrun_gprof.la
  pkglib_LTLIBRARIES += libhpcrun_io.la
  pkglib_LTLIBRARIES += libhpcrun_memleak.la
  pkglib_LTLIBRARIES += libhpcrun_pthread.la
  pkglib_LTLIBRARIES += libhpctoolkit.la
  bin_SCRIPTS += scripts/hpcrun
endif

if OPT_ENABLE_HPCRUN_STATIC
  noinst_PROGRAMS  = libhpcrun.o
  pkglib_LIBRARIES += libhpcrun_wrap.a
  pkglib_LIBRARIES += libhpcrun_ga_wrap.a
  pkglib_LIBRARIES += libhpcrun_gprof_wrap.a
  pkglib_LIBRARIES += libhpcrun_io_wrap.a
  pkglib_LIBRARIES += libhpcrun_memleak_wrap.a
  pkglib_LIBRARIES += libhpcrun_pthread_wrap.a
  pkglib_LIBRARIES += libhpctoolkit.a
  bin_SCRIPTS += scripts/hpclink
endif

if OPT_ENABLE_MPI_WRAP
  pkglib_LTLIBRARIES += libhpcrun_mpi.la
endif


#-----------------------------------------------------------
# source files, part 1
#-----------------------------------------------------------

LIBELF_INC   = @LIBELF_INC@
LIBELF_LIB   = @LIBELF_LIB@
LIBMONITOR_INC = @LIBMONITOR_INC@
LIBMONITOR_LIB = @LIBMONITOR_LIB@
PAPI_INC_FLGS =  @OPT_PAPI_IFLAGS@ 
PAPI_LD_FLGS =   @OPT_PAPI_LDFLAGS@
XED2_INC = @XED2_INC@
XED2_HPCRUN_LIBS =  @XED2_HPCRUN_LIBS@
XED2_HPCLINK_LIBS = @XED2_HPCLINK_LIBS@
CUPTI_INC_FLGS = @OPT_CUPTI_IFLAGS@

ROCM_INC_FLGS = @OPT_ROCM_IFLAGS@

OPENCL_IFLAGS = @OPT_OPENCL_IFLAGS@
CUPTI_LD_FLGS = @OPT_CUPTI_LDFLAGS@
CUPTI_BASE = @OPT_CUPTI@

MPI_INC = @MPI_INC@
MPI_PROTO_FILE = @MPI_PROTO_FILE@

ZLIB_LIB = @ZLIB_LIB@
ZLIB_HPCLINK_LIB = @ZLIB_HPCLINK_LIB@
ZLIB_INC = @ZLIB_INC@

LZMA_LDFLAGS_DYN=@LZMA_LDFLAGS_DYN@
LZMA_LDFLAGS_STAT=@LZMA_LDFLAGS_STAT@

HPCFNBOUNDS_INC = $(top_srcdir)/src/tool/hpcfnbounds

PLUGIN_CONFIG_FILES = ga io memleak pthread

# Disable the ibm-xlomp plugin in favor of the new openmp tools
# interface.
if FALSE
  PLUGIN_CONFIG_FILES += ibm-xlomp
  pkglib_LIBRARIES += libibm_xlomp_wrappers.a
  libibm_xlomp_wrappers_a_SOURCES = plugins/ibm-xlomp-wrappers.c
endif

UNW_UNIV_FILES = \
	unwind/common/backtrace.c	\
        unwind/common/unw-throw.c

UNW_COMMON_FILES = \
        $(UNW_UNIV_FILES)				\
	unwind/common/binarytree_uwi.c			\
	unwind/common/interval_t.c			\
	unwind/common/libunw_intervals.c		\
	unwind/common/stack_troll.c			\
	unwind/common/uw_hash.c			\
	unwind/common/uw_recipe_map.c

UNW_X86_FILES = \
       $(UNW_COMMON_FILES) \
	unwind/x86-family/x86-all.c			\
	unwind/x86-family/amd-xop.c                     \
	unwind/x86-family/x86-cold-path.c		\
	unwind/x86-family/x86-validate-retn-addr.c	\
	unwind/x86-family/x86-unwind-interval.c		\
	unwind/x86-family/x86-unwind-interval-fixup.c	\
	unwind/x86-family/x86-unwind.c		        \
	unwind/x86-family/x86-unwind-support.c		\
	unwind/x86-family/manual-intervals/x86-gcc-adjust.c \
	unwind/x86-family/manual-intervals/x86-gcc-main64.c \
	unwind/x86-family/manual-intervals/x86-linux-dlresolver.c \
	unwind/x86-family/manual-intervals/x86-intel11-f90main.c \
	unwind/x86-family/manual-intervals/x86-intel-align32.c \
	unwind/x86-family/manual-intervals/x86-intel-align64.c \
	unwind/x86-family/manual-intervals/x86-intel-composer2013-mic.c \
	unwind/x86-family/manual-intervals/x86-32bit-main.c \
	unwind/x86-family/manual-intervals/x86-32bit-icc-variant.c \
	unwind/x86-family/manual-intervals/x86-fail-intervals.c \
	unwind/x86-family/manual-intervals/x86-pgi-mp_pexit.c

# unwind/x86-family/x86-libunwind.c  # presently unused on x86

UNW_X86_INCLUDE_DIRS = \
	-I$(srcdir)/unwind/x86-family	\
	-I$(XED2_INC)

UNW_PPC64_FILES = \
        $(UNW_COMMON_FILES) \
	unwind/ppc64/ppc64-unwind.c \
	unwind/ppc64/ppc64-unwind-interval.c \
	unwind/common/default_validation_summary.c

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
       $(UNW_COMMON_FILES) \
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

MY_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DINLINE_FN=1 \
	-DLOCAL_BUILD=1 \
	-D__HIP_PLATFORM_HCC__=1

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
	tool_state.c			\
	sample-sources/blame-shift/blame-shift.c \
	sample-sources/blame-shift/blame-map.c   \
	sample-sources/blame-shift/directed.c    \
	sample-sources/blame-shift/undirected.c    \
	sample-sources/omp-mutex.c      \
	sample-sources/omp-idle.c       \
        sample-sources/common.c         \
	sample-sources/display.c		\
	sample-sources/ga.c		\
	sample-sources/io.c 		\
	sample-sources/itimer.c		\
	sample-sources/idle.c		\
	sample-sources/memleak.c	\
	sample-sources/pthread-blame.c  \
	sample-sources/none.c           \
        sample-sources/retcnt.c         \
        sample-sources/sync.c           \
	sample_sources_registered.c	\
	sample-sources/sample-filters.c \
	segv_handler.c			\
	start-stop.c			\
	term_handler.c			\
	thread_data.c			\
	thread_use.c			\
	thread_finalize.c		\
	control-knob.c   \
	control-knob.h   \
	device-finalizers.c \
	gpu-monitors.c		\
	device-initializers.c \
	module-ignore-map.c \
	threadmgr.c			\
	trace.c				\
	weak.c				\
	write_data.c		        \
	\
	cct/cct_bundle.c		\
	cct/cct_ctxt.c			\
	cct/cct.c               	\
	cct/cct-node-vector.c     \
	\
	cct2metrics.c                   \
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
	hpcrun-placeholders.c 		\
	gpu/gpu-activity.c 		\
	gpu/gpu-activity-channel.c 	\
	gpu/gpu-activity-process.c 	\
	gpu/gpu-application-thread-api.c \
	gpu/gpu-channel-item-allocator.c \
	gpu/gpu-context-id-map.c	\
	gpu/gpu-correlation.c 		\
	gpu/gpu-correlation-channel.c 	\
	gpu/gpu-correlation-channel-set.c \
	gpu/gpu-correlation-id.c	\
	gpu/gpu-correlation-id-map.c 	\
	gpu/gpu-event-id-map.c \
	gpu/gpu-function-id-map.c \
	gpu/gpu-host-correlation-map.c 	\
	gpu/gpu-metrics.c 		\
	gpu/gpu-monitoring.c 		\
	gpu/gpu-monitoring-thread-api.c \
	gpu/gpu-op-placeholders.c 	\
	gpu/gpu-operation-item.c			\
	gpu/gpu-operation-item-process.c   \
	gpu/gpu-operation-channel.c \
	gpu/gpu-operation-channel-set.c \
	gpu/gpu-operation-multiplexer.c 	\
	gpu/gpu-splay-allocator.c	\
	gpu/gpu-stream-id-map.c		\
	gpu/gpu-trace.c			\
	gpu/gpu-trace-channel.c		\
	gpu/gpu-trace-item.c		\
	gpu/gpu-trace-channel-set.c	\
	gpu/gpu-trace-demultiplexer.c	\
	\
	gpu/ompt/ompt-gpu-api.c 			\
	gpu/ompt/ompt-activity-translate.c 		\
	sample-sources/openmp-target.c                  \
	\
	ompt/ompt-callstack.c           \
	ompt/ompt-defer.c               \
	ompt/ompt-device.c              \
	ompt/ompt-defer-write.c         \
	ompt/ompt-interface.c           \
	ompt/ompt-queues.c              \
	ompt/ompt-region.c              \
	ompt/ompt-region-debug.c        \
	ompt/ompt-placeholders.c        \
	ompt/ompt-device-map.c          \
	ompt/ompt-task.c                \
	ompt/ompt-thread.c              \
	\
	extern-real/dl-iterate.c	\
	extern-real/mmap.c		\
	\
	syscalls/poll.c	\
	syscalls/ppoll.c \
	syscalls/select.c \
	syscalls/sysv_signal.c \
	\
	utilities/executable-path.h utilities/executable-path.c \
	utilities/hpcrun-nanotime.h  utilities/hpcrun-nanotime.c  \
	utilities/ip-normalized.h utilities/ip-normalized.c \
	utilities/line_wrapping.c	\
	utilities/linuxtimer.c		\
	utilities/timer.c		\
	utilities/tokenize.h utilities/tokenize.c \
	utilities/unlink.h utilities/unlink.c

# 	gpu/gpu-api.c
#	trampoline/common/trampoline.c

if HOST_CPU_PPC
MY_BASE_FILES +=			\
        trampoline/common/trampoline_eager.c
else
MY_BASE_FILES +=			\
        trampoline/common/trampoline_lazy.c
endif

if OPT_ENABLE_PERF_EVENT
MY_BASE_FILES +=  \
	sample-sources/perf/event_custom.c  \
	sample-sources/perf/linux_perf.c    \
	sample-sources/perf/perf_event_open.c     \
	sample-sources/perf/perf-util.c     \
	sample-sources/perf/perf_mmap.c     \
	sample-sources/perf/perf_skid.c

MY_CPP_DEFINES  += -DHPCRUN_SS_LINUX_PERF

if OPT_PERFMON
MY_BASE_FILES += sample-sources/perf/perfmon-util.c
else
MY_BASE_FILES += sample-sources/perf/perfmon-util-dummy.c
endif

if OPT_ENABLE_KERNEL_4_3
MY_BASE_FILES += sample-sources/perf/kernel_blocking.c
else
MY_BASE_FILES += sample-sources/perf/kernel_blocking_stub.c
endif
endif

## if OPT_ENABLE_MPI_WRAP
## MY_BASE_FILES +=  			\
## 	sample-sources/mpi.c
## endif

MY_DYNAMIC_FILES = 			\
	fnbounds/fnbounds_client.c	\
	fnbounds/fnbounds_dynamic.c	\
	monitor-exts/openmp.c		\
        custom-init-dynamic.c

MY_STATIC_FILES = 			\
	fnbounds/fnbounds_static.c      \
        custom-init-static.c

MY_LINUX_DYNAMIC_FILES = 		\
	os/linux/dylib.c


MY_MIPS_FILES = \
	unwind/common/default_validation_summary.c

MY_PPC_FILES = \
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

MY_AARCH64_FILES = \
	trampoline/aarch64/aarch64-tramp.c \
	utilities/arch/libunwind/libunwind-context-pc.c

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

if OPT_PAPI_ROCM
    MY_PAPI_FILES += sample-sources/papi-c-rocm.c
endif


if OPT_ENABLE_CUPTI
MY_CUPTI_FILES = sample-sources/nvidia.c	\
	gpu/nvidia/cubin-hash-map.c		\
	gpu/nvidia/cubin-id-map.c		\
	gpu/nvidia/cubin-symbols.c		\
	gpu/nvidia/cuda-api.c			\
	gpu/nvidia/cuda-device-map.c		\
	gpu/nvidia/cupti-activity-translate.c	\
	gpu/nvidia/cupti-analysis.c		\
	gpu/nvidia/cupti-api.c			\
	gpu/nvidia/cupti-gpu-api.c		
endif

if OPT_ENABLE_OPENCL
MY_OPENCL_FILES = \
  sample-sources/opencl.c \
	gpu/opencl/opencl-api.c \
	gpu/opencl/opencl-memory-manager.c \
	gpu/opencl/opencl-activity-translate.c \
	gpu/opencl/opencl-h2d-map.c \
	gpu/opencl/opencl-queue-map.c \
	gpu/opencl/opencl-context-map.c

pkglib_LTLIBRARIES += libhpcrun_opencl.la
libhpcrun_opencl_la_CPPFLAGS  = -I$(top_srcdir)/src $(OPENCL_IFLAGS)
libhpcrun_opencl_la_SOURCES   = gpu/opencl/opencl-api-wrappers.c
libhpcrun_opencl_la_LDFLAGS   = -Wl,-Bsymbolic
endif

if OPT_ENABLE_GTPIN
MY_GTPIN_FILES = \
	gpu/instrumentation/kernel-data-map.c \
	gpu/instrumentation/gtpin-instrumentation.c \
	gpu/instrumentation/gtpin-correlation-id-map.c
endif

if OPT_ENABLE_ROCM
MY_ROCM_FILES =\
	sample-sources/amd.c \
	sample-sources/amd-rocprofiler.c \
	gpu/amd/hip-api.c \
	gpu/amd/roctracer-activity-translate.c \
	gpu/amd/roctracer-api.c \
	gpu/amd/rocprofiler-api.c \
	gpu/amd/rocm-binary-processing.c
endif

if OPT_ENABLE_LEVEL0
MY_LEVEL0_FILES=\
	sample-sources/level0.c \
	gpu/level0/level0-api.c \
	gpu/level0/level0-command-list-context-map.c \
	gpu/level0/level0-command-list-map.c \
	gpu/level0/level0-command-process.c \
	gpu/level0/level0-data-node.c \
	gpu/level0/level0-event-map.c \
	gpu/level0/level0-handle-map.c

pkglib_LTLIBRARIES += libhpcrun_level0.la
libhpcrun_level0_la_SOURCES   = gpu/level0/level0-api-wrappers.c
libhpcrun_level0_la_LDFLAGS   = -Wl,-Bsymbolic

endif

MY_UPC_FILES = sample-sources/upc.c


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
	$(LIBUNWIND_IFLAGS)		\
	-I$(HPCFNBOUNDS_INC)		\
        $(OPT_CUDA_IFLAGS)             \
        $(OPT_CUPTI_IFLAGS)             \
		$(ROCM_INC_FLGS) \
	-I$(LIBELF_INC)			\
	-I$(LIBMONITOR_INC)	\
	$(GOTCHA_IFLAGS)	\
	$(OPENCL_IFLAGS)

MY_MIPS_INCLUDE_DIRS = \
	-I$(srcdir)/unwind/mips

MY_PPC_INCLUDE_DIRS = 		\
        -I$(srcdir)/utilities/arch/ppc64

MY_X86_INCLUDE_DIRS =		\
        -I$(srcdir)/utilities/arch/x86-family

MY_IA64_INCLUDE_DIRS =         \
        -I$(srcdir)/utilities/arch/ia64

MY_AARCH64_INCLUDE_DIRS = \
	-I$(srcdir)/utilities/arch/aarch64

libhpcrun_la_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_DYNAMIC_FILES)

libhpcrun_fake_audit_la_SOURCES =	\
	audit/fake-auditor.c

libhpcrun_audit_la_SOURCES =		\
	audit/auditor.c

libhpcrun_o_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_STATIC_FILES)


libhpcrun_wrap_a_SOURCES =		\
	monitor-exts/openmp.c

libhpcrun_ga_la_SOURCES = 		\
	sample-sources/ga-overrides.c

libhpcrun_ga_wrap_a_SOURCES = 		\
	sample-sources/ga-overrides.c

libhpcrun_gprof_la_SOURCES = gprof.c

libhpcrun_gprof_wrap_a_SOURCES = gprof.c

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

libhpcrun_mpi_la_SOURCES = 		\
	./mpi-overrides.c

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

libhpcrun_fake_audit_la_CPPFLAGS =	\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_audit_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

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

libhpcrun_gprof_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_gprof_wrap_a_CPPFLAGS =	\
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

libhpcrun_mpi_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	-I$(MPI_INC)			\
	$(MY_INCLUDE_DIRS)

libhpctoolkit_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpctoolkit_a_CPPFLAGS =		\
	-DHPCRUN_STATIC_LINK		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

if OPT_ENABLE_MPI_WRAP

BUILT_SOURCES += mpi-overrides.c
CLEANFILES += mpi-overrides.c

mpi-overrides.c: $(srcdir)/sample-sources/$(MPI_PROTO_FILE)
	$(PYTHON) $(srcdir)/sample-sources/make-wrappers.py \
		--f77symbol $(F77_SYMBOLS) $(srcdir)/sample-sources/$(MPI_PROTO_FILE)

endif


#-----------------------------------------------------------
# cflags
#-----------------------------------------------------------

libhpcrun_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS) $(PERFMON_CFLAGS)
libhpcrun_o_CFLAGS  = $(CFLAGS) $(HOST_CFLAGS) $(PERFMON_CFLAGS)

libhpcrun_fake_audit_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_audit_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_ga_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_ga_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_gprof_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_gprof_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_io_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_io_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_memleak_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_memleak_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_pthread_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_pthread_wrap_a_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_mpi_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)


#-----------------------------------------------------------
# Dependency Flags
#-----------------------------------------------------------

# The noinst_LTLIBRARIES always build with -fPIC and can be linked
# into both libhpcrun.o and libhpcrun.so.  For strange technical
# reasons, the .la version doesn't seem to work (libtool doesn't obey
# the given order).  This only applies to hpcrun, not struct or prof.

PROF_LEAN_A = $(top_builddir)/src/lib/prof-lean/.libs/libHPCprof-lean.a
SUPPORT_LEAN_A = $(top_builddir)/src/lib/support-lean/.libs/libHPCsupport-lean.a

# Modified versions of the external archives.  The static case works
# with or without -fPIC, but the dynamic case requies -fPIC.

OUR_LIBUNWIND_A = $(top_builddir)/src/extern/libunwind/libunwind.a
OUR_LZMA_A = $(top_builddir)/src/extern/lzma/liblzma.a

libhpcrun_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_fake_audit_la_LDFLAGS = -Wl,-Bsymbolic -ldl

libhpcrun_audit_la_LDFLAGS = -Wl,-Bsymbolic -ldl

libhpcrun_ga_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_gprof_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_io_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_memleak_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_pthread_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_mpi_la_LDFLAGS = -Wl,-Bsymbolic

# Note: (1) for dynamic libhpcrun.so, use LIBADD for (.a) archives
# with fPIC (libunwind, etc) that are linked in and hidden at link
# time, and LDFLAGS (-L, -l) for other dependency libraries.
#
# (2) for static libhpcrun.o, use LDADD for archives that are linked
# and hidden into libhpcrun.o.  Other dependencies go into hpclink.
# Don't use LDFLAGS for static case.

libhpcrun_la_LIBADD =  \
	$(PROF_LEAN_A)  \
	$(SUPPORT_LEAN_A)  \
	$(MBEDTLS_LIBS)  \
	$(OUR_LIBUNWIND_A)  \
	$(OUR_LZMA_A)

libhpcrun_la_LDFLAGS +=  \
	-L$(LIBMONITOR_LIB) -lmonitor  \
	-lpthread -lrt  \
	-L$(LIBELF_LIB) -lelf  \
	$(PERFMON_LDFLAGS_DYN)  \
	$(OPT_ROCM_LDFLAGS)

libhpcrun_o_LDADD =  \
	$(PROF_LEAN_A)  \
	$(SUPPORT_LEAN_A)  \
	$(PERFMON_LDFLAGS_STAT)  \
	$(MBEDTLS_LIBS)  \
	$(OUR_LIBUNWIND_A)  \
	$(OUR_LZMA_A)


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
  libhpcrun_gprof_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_gprof_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
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
  libhpcrun_o_LDADD     += $(XED2_HPCLINK_LIBS) 
  libhpcrun_ga_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_gprof_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_gprof_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
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

if HOST_CPU_AARCH64
  libhpcrun_la_SOURCES  += $(MY_AARCH64_FILES)
  libhpcrun_o_SOURCES   += $(MY_AARCH64_FILES)
  libhpcrun_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_o_CPPFLAGS  += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
  libhpcrun_o_CCASFLAGS  = $(AM_CCASFLAGS)
  libhpcrun_ga_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_ga_wrap_a_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_gprof_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_gprof_wrap_a_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_io_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_io_wrap_a_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_memleak_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_memleak_wrap_a_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_pthread_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_pthread_wrap_a_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
  libhpcrun_mpi_la_CPPFLAGS += $(MY_AARCH64_INCLUDE_DIRS)
endif

if OPT_PAPI_DYNAMIC
  libhpcrun_la_SOURCES  += $(MY_PAPI_FILES)
  libhpcrun_la_CPPFLAGS += $(PAPI_INC_FLGS)
  libhpcrun_la_LDFLAGS  += $(PAPI_LD_FLGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_PAPI
endif

if OPT_ENABLE_CUPTI
  libhpcrun_la_SOURCES  += $(MY_CUPTI_FILES)
  libhpcrun_o_SOURCES   += $(MY_CUPTI_FILES)
  libhpcrun_la_CPPFLAGS += $(CUPTI_INC_FLGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_NVIDIA
endif

if OPT_PAPI_CUPTI
libhpcrun_la_CPPFLAGS += $(CUPTI_INC_FLGS)
MY_CPP_DEFINES += -DHPCRUN_SS_PAPI_C_CUPTI
endif

if OPT_PAPI_ROCM
libhpcrun_la_CPPFLAGS   += $(ROCM_INC_FLGS)
MY_CPP_DEFINES += -DHPCRUN_SS_PAPI_C_ROCM
endif

if OPT_PAPI_STATIC
  libhpcrun_o_SOURCES   += $(MY_PAPI_FILES)
  libhpcrun_o_CPPFLAGS  += $(PAPI_INC_FLGS)
  libhpcrun_o_LDADD     += $(OPT_PAPI_LIBS_STAT)
  
  MY_CPP_DEFINES  += -DHPCRUN_SS_PAPI
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
  libhpcrun_la_SOURCES += $(MY_CUDA_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_CUDA
  libhpcrun_la_CFLAGS += $(OPT_CUDA_IFLAGS)
  libhpcrun_o_SOURCES += $(MY_CUDA_FILES)
endif

if OPT_ENABLE_ROCM
  libhpcrun_la_SOURCES  += $(MY_ROCM_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_ROCM
  libhpcrun_la_CFLAGS   += $(ROCM_INC_FLGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_AMD
endif

if OPT_ENABLE_LEVEL0
  libhpcrun_la_SOURCES  += $(MY_LEVEL0_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_LEVEL0
  libhpcrun_la_CFLAGS   += $(OPT_LEVEL0_IFLAGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_LEVEL0
endif

if OPT_ENABLE_OPENCL
  libhpcrun_la_SOURCES  += $(MY_OPENCL_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_OPENCL
  libhpcrun_la_CFLAGS   += $(OPT_OPENCL_IFLAGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_OPENCL
endif 

if OPT_ENABLE_GTPIN
  libhpcrun_la_SOURCES  += $(MY_GTPIN_FILES)
  libhpcrun_la_CPPFLAGS += -DENABLE_GTPIN -DGTPIN_LIBDIR=$(OPT_GTPIN_LIBDIR)
  libhpcrun_la_CFLAGS   += $(OPT_GTPIN_IFLAGS)

  MY_CPP_DEFINES  += -DHPCRUN_SS_GTPIN
endif

if UNW_LIBUNW
  UNW_SOURCE_FILES = $(UNW_LIBUNW_FILES)
  UNW_INCLUDE_DIRS = $(UNW_LIBUNW_INCLUDE_DIRS)
  UNW_DYNAMIC_LD_FLAGS = $(UNW_LIBUNW_LD_FLAGS)
  UNW_STATIC_LD_FLAGS =  $(UNW_LIBUNW_LD_FLAGS)
endif

libhpcrun_dlmopen_la_SOURCES   = dlmopen/dlmopen.c
libhpcrun_dlmopen_la_LDFLAGS   = -ldl

libhpcrun_la_CFLAGS += $(GOTCHA_IFLAGS)
libhpcrun_la_LDFLAGS += $(GOTCHA_LDFLAGS)
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
libhpcrun_mpi_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_la_LDFLAGS  += $(UNW_DYNAMIC_LD_FLAGS)
libhpcrun_o_LDADD     += $(UNW_STATIC_LD_FLAGS)
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
# Manual install targets
#-----------------------------------------------------------

# Take over the last stage of building and installing libhpcrun.so and
# libhpcrun.o.  Objcopy works to hide (localize) symbol names, but
# only on object files (.o and .a), not on shared (.so) libraries.
#
# We change libhpcrun to no-install targets and automake builds an
# (.a) archive of (.o) files with fPIC.  Then, we manually unpack the
# archive, link in the 3rd-party libraries (libunwind, etc, with
# fPIC), hide the symbols, and then link as a shared library.
#
# Note: (1) for dynamic libhpcrun.so, use LIBADD for (.a) archives
# with fPIC (libunwind, etc) that are linked in and hidden at link
# time, and LDFLAGS (-L, -l) for other dependency libraries.
#
# (2) for static libhpcrun.o, use LDADD for archives that are linked
# and hidden into libhpcrun.o.  Other dependencies go into hpclink.
# Don't use LDFLAGS for static case.

MONITOR_NAMES = -G 'monitor_*'
HPCRUN_NAMES =  -G 'hpcrun_*' -G 'hpctoolkit_*' -G 'OnLoadToolProp' -G 'OnLoad' -G 'OnUnloadTool'
MISC_NAMES =    -G 'debug_flag_*' -G 'messages_*' -G ompt_start_tool
OPENCL_NAMES = -G 'clBuildProgram' -G 'clCreate*' -G 'clEnqueue*' -G 'clSetKernelArg' -G 'cl*Event*'

DYN_SYSCALL_LIST = poll ppoll pselect select __sysv_signal
DYN_SYSCALL_NAMES = $(foreach name,$(DYN_SYSCALL_LIST),-G $(name))

STAT_WRAP_NAMES = -w -G '__wrap_*'

WORK_DYN = work-dyn
WORK_STAT = work-stat

install-exec-hook:
if OPT_ENABLE_HPCRUN
	@echo "building and installing libhpcrun.so ..."
	rm -rf $(WORK_DYN)
	mkdir $(WORK_DYN)
	cd $(WORK_DYN) && $(AR) x ../.libs/libhpcrun.a
	$(LD) -r $(WORK_DYN)/*.o $(libhpcrun_la_LIBADD) -o $(WORK_DYN)/part1.o
	$(OBJCOPY) -w $(MONITOR_NAMES) $(HPCRUN_NAMES) $(DYN_SYSCALL_NAMES) $(MISC_NAMES) $(OPENCL_NAMES) \
		$(WORK_DYN)/part1.o $(WORK_DYN)/part2.o
	$(CC) -shared $(WORK_DYN)/part2.o $(libhpcrun_la_LDFLAGS) -o $(WORK_DYN)/libhpcrun.so
	$(INSTALL) -m 0755 $(WORK_DYN)/libhpcrun.so $(DESTDIR)$(pkglibdir)
else
	@echo "skipping libhpcrun.so"
endif
if OPT_ENABLE_HPCRUN_STATIC
	@echo "building and installing libhpcrun.o ..."
	rm -rf $(WORK_STAT)
	mkdir $(WORK_STAT)
	$(OBJCOPY) -w $(MONITOR_NAMES) $(HPCRUN_NAMES) $(STAT_WRAP_NAMES) $(MISC_NAMES)  \
		libhpcrun.o $(WORK_STAT)/libhpcrun.o
	$(INSTALL) -m 0644 $(WORK_STAT)/libhpcrun.o $(DESTDIR)$(pkglibdir)
else
	@echo "skipping libhpcrun.o"
endif

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
