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
## Copyright ((c)) 2002-2023, Rice University
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
pkglib_LTLIBRARIES += libhpcrun.la
pkglib_LTLIBRARIES += libhpcrun_unwind.la
pkglib_LTLIBRARIES += libhpcrun_fake_audit.la
pkglib_LTLIBRARIES += libhpcrun_private_ns.la
pkglib_LTLIBRARIES += libhpcrun_audit.la
pkglib_LTLIBRARIES += libhpcrun_dlmopen.la
pkglib_LTLIBRARIES += libhpcrun_ga.la
pkglib_LTLIBRARIES += libhpcrun_gprof.la
pkglib_LTLIBRARIES += libhpcrun_io.la
pkglib_LTLIBRARIES += libhpcrun_memleak.la
pkglib_LTLIBRARIES += libhpcrun_pthread.la
pkglib_LTLIBRARIES += libhpctoolkit.la
bin_SCRIPTS += hpcrun
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
CUPTI_INC_FLGS = @OPT_CUPTI_IFLAGS@

ROCM_INC_FLGS = @OPT_ROCM_IFLAGS@

OPENCL_IFLAGS = @OPT_OPENCL_IFLAGS@
OCL_BH_IFLAGS = @OPT_OCL_BH_IFLAGS@
OCL_BH_LDFLGS = @OPT_OCL_BH_LDFLAGS@
CUPTI_LD_FLGS = @OPT_CUPTI_LDFLAGS@
CUPTI_BASE = @OPT_CUPTI@

MPI_INC = @MPI_INC@
MPI_PROTO_FILE = @MPI_PROTO_FILE@

ZLIB_LIB = @ZLIB_LIB@
ZLIB_INC = @ZLIB_INC@

LZMA_LDFLAGS_DYN=@LZMA_LDFLAGS_DYN@
LZMA_LDFLAGS_STAT=@LZMA_LDFLAGS_STAT@

UNW_UNIV_FILES = \
	unwind/common/backtrace.c	\
	unwind/common/unw-throw.c

UNW_COMMON_FILES = \
	$(UNW_UNIV_FILES)				\
	unwind/common/binarytree_uwi.c			\
	unwind/common/interval_t.c			\
	unwind/common/libunw_intervals.c		\
	unwind/common/libunwind-interface.c		\
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
	-I$(XED2_INC)

UNW_PPC64_FILES = \
	$(UNW_COMMON_FILES) \
	unwind/ppc64/ppc64-unwind.c \
	unwind/ppc64/ppc64-unwind-interval.c \
	unwind/common/default_validation_summary.c

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

UNW_LIBUNW_LD_FLAGS =

UNW_MIPS_FILES = \
	$(UNW_COMMON_FILES) \
	unwind/mips/mips-unwind.c \
	unwind/mips/mips-unwind-interval.c

UNW_MIPS_LD_FLAGS =

MY_CPP_DEFINES =			\
	-D_GNU_SOURCE			\
	-DINLINE_FN=1 \
	-DLOCAL_BUILD=1 \
	-D__HIP_PLATFORM_HCC__=1 \
	-fvisibility=hidden

MY_BASE_FILES =				\
	utilities/first_func.c		\
	\
	main.h main.c			\
	foil/foil.c			\
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
	hpcrun_signals.c		\
	hpcrun_stats.c			\
	loadmap.c			\
	metrics.c			\
	name.c				\
	rank.c				\
	safe-sampling.c			\
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
	sample-sources/display.c		\
	sample-sources/exclude.c	\
	sample-sources/ga.c		\
	sample-sources/ga-overrides.c	\
	sample-sources/io.c 		\
	sample-sources/io-over.c	\
	sample-sources/itimer.c		\
	sample-sources/idle.c		\
	sample-sources/memleak.c	\
	sample-sources/memleak-overrides.c	\
	sample-sources/pthread-blame.c  \
	sample-sources/pthread-blame-overrides.c	\
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
	device-initializers.c \
	module-ignore-map.c \
	threadmgr.c			\
	trace.c				\
	weak.c				\
	write_data.c		        \
	\
	audit/binding.c			\
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
	logical/common.c \
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
	messages/errors.c	        \
	gpu/gpu-activity.c 		\
	gpu/gpu-activity-channel.c 	\
	gpu/gpu-activity-process.c 	\
	gpu/gpu-application-thread-api.c \
	gpu/gpu-binary.c \
	gpu/gpu-cct.c \
	gpu/gpu-channel-item-allocator.c \
	gpu/gpu-context-id-map.c	\
	gpu/gpu-correlation.c 		\
	gpu/gpu-correlation-channel.c 	\
	gpu/gpu-correlation-channel-set.c \
	gpu/gpu-correlation-id.c	\
	gpu/gpu-correlation-id-map.c 	\
	gpu/gpu-event-id-map.c \
	gpu/gpu-flush-alarm.c \
	gpu/gpu-function-id-map.c \
	gpu/gpu-host-correlation-map.c 	\
	gpu/gpu-instrumentation.c 	\
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
	gpu/gpu-kernel-table.c		\
	gpu/gpu-trace.c			\
	gpu/gpu-trace-channel.c		\
	gpu/gpu-trace-item.c		\
	gpu/gpu-trace-channel-set.c	\
	gpu/gpu-trace-demultiplexer.c	\
	\
	gpu/ompt/ompt-gpu-api.c 			\
	gpu/ompt/ompt-activity-translate.c 		\
	\
	ompt/ompt-callstack.c           \
	ompt/ompt-defer.c               \
	ompt/ompt-device.c              \
	ompt/ompt-defer-write.c         \
	ompt/ompt-interface.c           \
	ompt/ompt-queues.c              \
	ompt/ompt-region.c              \
	ompt/ompt-region-debug.c        \
	ompt/ompt-device-map.c          \
	ompt/ompt-task.c                \
	ompt/ompt-thread.c              \
	\
	utilities/executable-path.h utilities/executable-path.c \
	utilities/hpcrun-nanotime.h  utilities/hpcrun-nanotime.c  \
	utilities/ip-normalized.h utilities/ip-normalized.c \
	utilities/line_wrapping.c	\
	utilities/linuxtimer.c		\
	utilities/timer.c		\
	utilities/tokenize.h utilities/tokenize.c \
	utilities/unlink.h utilities/unlink.c \
	\
	gpu/blame-shifting/blame.c \
	gpu/blame-shifting/blame-helper.c \
	gpu/blame-shifting/blame-queue-map.c	\
	gpu/blame-shifting/blame-kernel-map.c	\
	gpu/blame-shifting/active-kernels-map.c

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
	custom-init-dynamic.c		\
	foil/hpctoolkit-preload.c	\
	foil/monitor-preload.c		\
	foil/ompt-preload.c		\
	foil/syscalls/poll-preload.c	\
	foil/syscalls/ppoll-preload.c	\
	foil/syscalls/select-preload.c	\
	foil/syscalls/sysv_signal-preload.c

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
MY_CPP_DEFINES += -DHPCRUN_SS_PAPI_C_INTEL
MY_PAPI_FILES = \
	sample-sources/papi-c.c \
	sample-sources/papi-c-extended-info.c \
	sample-sources/papi-c-intel.c
else
MY_PAPI_FILES = sample-sources/papi.c
endif

MY_PAPI_FILES += \
	gpu/opencl/intel/papi/papi-kernel-map.c	\
	gpu/opencl/intel/papi/papi-metric-collector.c

if OPT_PAPI_CUPTI
MY_PAPI_FILES += sample-sources/papi-c-cupti.c
endif

if OPT_ENABLE_CUPTI
MY_CUPTI_FILES = sample-sources/nvidia.c	\
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
	gpu/opencl/opencl-context-map.c \
	gpu/opencl/intel/optimization-check.c \
	gpu/opencl/opencl-kernel-loadmap-map.c \
	gpu/opencl/intel/maps/queue-context-map.c \
	gpu/opencl/intel/maps/kernel-context-map.c	\
	gpu/opencl/intel/maps/kernel-param-map.c	\
	gpu/opencl/intel/maps/buffer-map.c	\
	gpu/opencl/intel/maps/device-map.c	\
	gpu/blame-shifting/blame-kernel-cleanup-map.c \
	gpu/blame-shifting/opencl/opencl-blame.c

pkglib_LTLIBRARIES += libhpcrun_opencl.la
libhpcrun_opencl_la_CPPFLAGS  = $(OPENCL_IFLAGS)
libhpcrun_opencl_la_SOURCES   = foil/opencl-preload.c
libhpcrun_opencl_la_LDFLAGS   = -Wl,-Bsymbolic
libhpcrun_opencl_la_LDFLAGS   += $(OPT_OCL_BH_LDFLAGS)
endif

if OPT_ENABLE_GTPIN

MY_DYNAMIC_FILES += gpu/intel/gtpin/gtpin-shim.c
pkglib_LTLIBRARIES += libhpcrun_gtpin_cxx.la
libhpcrun_gtpin_cxx_la_CPPFLAGS  = -g $(MY_INCLUDE_DIRS)
libhpcrun_gtpin_cxx_la_CXXFLAGS  = @HOST_CXXFLAGS@

INTEL_GPU_SYMBOLS = gpu/intel/binaries/patchTokenSymbols.c gpu/intel/binaries/symbolVector.c gpu/intel/binaries/zebinSymbols.c

libhpcrun_gtpin_cxx_la_SOURCES   = gpu/intel/gtpin/gtpin-instrumentation.cpp $(INTEL_GPU_SYMBOLS)
libhpcrun_gtpin_cxx_la_LDFLAGS   = -Wl,-Bsymbolic
libhpcrun_gtpin_cxx_la_LIBADD = $(PROF_LEAN_A)  -L$(LIBELF_LIB) -lelf -L$(TBB_LIB_DIR) -ltbb -ltbbmalloc -ldl

endif

if OPT_ENABLE_ROCM
MY_ROCM_FILES =\
	sample-sources/amd.c \
	sample-sources/amd-rocprofiler.c \
	sample-sources/openmp-target.c \
	gpu/amd/hip-api.c \
	gpu/amd/roctracer-activity-translate.c \
	gpu/amd/roctracer-api.c \
	gpu/amd/rocprofiler-api.c \
	foil/rocm-preload.c
endif

if OPT_ENABLE_LEVEL0
MY_LEVEL0_FILES=\
	sample-sources/level0.c \
	gpu/intel/level0/level0-api.c \
	gpu/intel/level0/level0-binary.c \
	gpu/intel/level0/level0-command-list-context-map.c \
	gpu/intel/level0/level0-command-list-map.c \
	gpu/intel/level0/level0-command-process.c \
	gpu/intel/level0/level0-data-node.c \
	gpu/intel/level0/level0-event-map.c \
	gpu/intel/level0/level0-handle-map.c \
	gpu/intel/level0/level0-kernel-module-map.c \
	gpu/intel/level0/level0-fence-map.c \
	gpu/intel/level0/level0-command-queue-map.c

pkglib_LTLIBRARIES += libhpcrun_level0.la
libhpcrun_level0_la_SOURCES   = gpu/intel/level0/level0-api-wrappers.c
libhpcrun_level0_la_CFLAGS    = $(OPT_LEVEL0_IFLAGS)
libhpcrun_level0_la_LDFLAGS   = -Wl,-Bsymbolic

endif

MY_UPC_FILES = sample-sources/upc.c


#
# BG/Q backend requires special treatment to avoid deadlocks
#
if OPT_BGQ_BACKEND
MY_CPP_DEFINES += -DUSE_HW_THREAD_ID -DNONZERO_THRESHOLD
endif

MY_INCLUDE_DIRS =   \
	$(LIBUNWIND_IFLAGS)            \
	$(OPT_GTPIN_IFLAGS)            \
	$(ROCM_INC_FLGS)               \
	-I$(LIBELF_INC)                \
	-I$(LIBMONITOR_INC)            \
	$(GOTCHA_IFLAGS)               \
	$(TBB_IFLAGS)                  \
	$(OPENCL_IFLAGS)               \
	$(OPT_CUDA_IFLAGS)             \
	$(OPT_CUPTI_IFLAGS)

libhpcrun_la_SOURCES = 			\
	$(MY_BASE_FILES)		\
	$(MY_DYNAMIC_FILES)

libhpcrun_unwind_la_SOURCES =	\
	unwind/common/libunwind-pvt.c

libhpcrun_fake_audit_la_SOURCES =	\
	audit/fake-auditor.c

libhpcrun_private_ns_la_SOURCES =	\
	audit/private-ns.c

libhpcrun_audit_la_SOURCES =		\
	audit/auditor.c			\
	audit/private-ns.c


libhpcrun_ga_la_SOURCES = foil/ga-preload.c

libhpcrun_gprof_la_SOURCES = foil/gprof-preload.c

libhpcrun_io_la_SOURCES = foil/io-preload.c

libhpcrun_memleak_la_SOURCES = foil/memleak-preload.c

libhpcrun_pthread_la_SOURCES = foil/pthread-blame-preload.c

libhpcrun_mpi_la_SOURCES = 		\
	./mpi-overrides.c

libhpctoolkit_la_SOURCES = 		\
	hpctoolkit.c


#-----------------------------------------------------------
# cppflags
#-----------------------------------------------------------

libhpcrun_la_CPPFLAGS =			\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_unwind_la_CPPFLAGS =	\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_fake_audit_la_CPPFLAGS =	\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_private_ns_la_CPPFLAGS =	\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_audit_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_ga_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_gprof_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_io_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_memleak_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_pthread_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	$(MY_INCLUDE_DIRS)

libhpcrun_mpi_la_CPPFLAGS =		\
	$(MY_CPP_DEFINES)		\
	-I$(MPI_INC)			\
	$(MY_INCLUDE_DIRS)

libhpctoolkit_la_CPPFLAGS =		\
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

libhpcrun_unwind_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_fake_audit_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_private_ns_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_audit_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_ga_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_gprof_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_io_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_memleak_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
libhpcrun_pthread_la_CFLAGS = $(CFLAGS) $(HOST_CFLAGS)
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

libhpcrun_la_LDFLAGS = -Wl,-Bsymbolic -ldl -Wl,-rpath='$$ORIGIN'

libhpcrun_unwind_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_fake_audit_la_LDFLAGS = -Wl,-Bsymbolic -ldl -Wl,-rpath='$$ORIGIN'

libhpcrun_private_ns_la_LDFLAGS = -Wl,-Bsymbolic -ldl -Wl,-rpath='$$ORIGIN'

libhpcrun_audit_la_LDFLAGS = -Wl,-Bsymbolic -ldl -Wl,-rpath='$$ORIGIN'

libhpcrun_ga_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_gprof_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_io_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_memleak_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_pthread_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_mpi_la_LDFLAGS = -Wl,-Bsymbolic

libhpcrun_la_LIBADD =  \
	$(PROF_LEAN_A)  \
	$(SUPPORT_LEAN_A) \
	$(LZMA_LDFLAGS_DYN)

libhpcrun_la_LDFLAGS +=  \
	-Wl,--exclude-libs=ALL \
	-L$(LIBMONITOR_LIB) -lmonitor  \
	-lpthread -lrt  \
	-L$(LIBELF_LIB) -lelf  \
	$(PERFMON_LDFLAGS_DYN)  \
	$(OPT_ROCM_LDFLAGS)

libhpcrun_unwind_la_LDFLAGS +=	\
	$(LIBUNWIND_LDFLAGS_DYN)

if HOST_CPU_X86_FAMILY
libhpcrun_la_LIBADD += $(XED2_HPCRUN_LIBS)
endif


#-----------------------------------------------------------
# whirled peas
#-----------------------------------------------------------

if HOST_OS_LINUX
libhpcrun_la_SOURCES += $(MY_LINUX_DYNAMIC_FILES)
endif

MY_AGENT_INCLUDE_DIRS = $(MY_INCLUDE_DIRS)

if HOST_CPU_MIPS
libhpcrun_la_SOURCES  += $(MY_MIPS_FILES)
libhpcrun_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
libhpcrun_ga_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(MY_MIPS_INCLUDE_DIRS)
endif

# Note: setting CCASFLAGS here is a no-op hack with the side effect of
# prefixing the tramp.s file names so they will be compiled separately
# for .o and .so targets.  CFLAGS does this for the .c files, but
# CFLAGS doesn't apply to .s files.  See the automake docs section
# 8.3.9.2, Objects created with both libtool and without.

if HOST_CPU_PPC
libhpcrun_la_SOURCES  += $(MY_PPC_FILES)
libhpcrun_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_ga_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_gprof_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(MY_PPC_INCLUDE_DIRS)
libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
endif

if HOST_CPU_X86_FAMILY
libhpcrun_la_SOURCES  += $(MY_X86_FILES)
libhpcrun_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
if OPT_ENABLE_GTPIN
libhpcrun_gtpin_cxx_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
endif
libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
libhpcrun_ga_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
libhpcrun_gprof_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(MY_X86_INCLUDE_DIRS)
MY_AGENT_INCLUDE_DIRS += $(MY_X86_INCLUDE_DIRS)
endif

if HOST_CPU_IA64
libhpcrun_la_SOURCES  += $(MY_IA64_FILES)
libhpcrun_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
libhpcrun_ga_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(MY_IA64_INCLUDE_DIRS)
endif

if HOST_CPU_AARCH64
libhpcrun_la_SOURCES  += $(MY_AARCH64_FILES)
libhpcrun_la_CCASFLAGS = $(AM_CCASFLAGS)
endif

if OPT_PAPI_DYNAMIC
libhpcrun_la_SOURCES  += $(MY_PAPI_FILES)
libhpcrun_la_CPPFLAGS += $(PAPI_INC_FLGS)
libhpcrun_la_LDFLAGS  += $(PAPI_LD_FLGS)

MY_CPP_DEFINES  += -DHPCRUN_SS_PAPI
endif

if OPT_ENABLE_CUPTI
libhpcrun_la_SOURCES  += $(MY_CUPTI_FILES)
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

if OPT_ENABLE_UPC
libhpcrun_la_SOURCES  += $(MY_UPC_FILES)
libhpcrun_la_CPPFLAGS += $(OPT_UPC_IFLAGS)
libhpcrun_la_LDFLAGS  += $(OPT_UPC_LDFLAGS)
endif

if OPT_ENABLE_LUSH_PTHREADS
libhpcrun_la_CPPFLAGS += -DLUSH_PTHREADS
endif


#-----------------------------------------------------------
# source files, part 2.3
#-----------------------------------------------------------

if UNW_X86
UNW_SOURCE_FILES = $(UNW_X86_FILES)
UNW_INCLUDE_DIRS = $(UNW_X86_INCLUDE_DIRS)
endif

if UNW_PPC64
UNW_SOURCE_FILES = $(UNW_PPC64_FILES)
UNW_DYNAMIC_LD_FLAGS = $(UNW_PPC64_LD_FLAGS)
endif

if OPT_ENABLE_PYTHON
libhpcrun_la_SOURCES += logical/python.c
libhpcrun_la_CFLAGS += @OPT_PYTHON_IFLAGS@
endif

if OPT_ENABLE_CUDA
libhpcrun_la_SOURCES += $(MY_CUDA_FILES)
libhpcrun_la_CPPFLAGS += -DENABLE_CUDA
libhpcrun_la_CFLAGS += $(OPT_CUDA_IFLAGS)
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
libhpcrun_la_CPPFLAGS += $(OPT_OCL_BH_IFLAGS)
libhpcrun_la_CFLAGS   += $(OPT_OPENCL_IFLAGS)

MY_CPP_DEFINES  += -DHPCRUN_SS_OPENCL
endif

if UNW_LIBUNW
UNW_SOURCE_FILES = $(UNW_LIBUNW_FILES)
UNW_DYNAMIC_LD_FLAGS = $(UNW_LIBUNW_LD_FLAGS)
endif

libhpcrun_dlmopen_la_SOURCES   = dlmopen/dlmopen.c
libhpcrun_dlmopen_la_LDFLAGS   = -ldl

libhpcrun_la_CFLAGS += $(GOTCHA_IFLAGS)
libhpcrun_la_LDFLAGS += $(GOTCHA_LDFLAGS)
libhpcrun_la_SOURCES  +=  $(UNW_SOURCE_FILES)
libhpcrun_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_ga_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_io_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_memleak_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_pthread_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_mpi_la_CPPFLAGS += $(UNW_INCLUDE_DIRS)
libhpcrun_la_LDFLAGS  += $(UNW_DYNAMIC_LD_FLAGS)
MY_AGENT_INCLUDE_DIRS += $(UNW_INCLUDE_DIRS)

libhpcrun_la_SOURCES += utilities/last_func.c


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


#############################################################################
# Common rules
#############################################################################

include $(top_srcdir)/src/Makeinclude.rules
