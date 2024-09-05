# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

libs = []

# Shim to create a private link namespace, to hold libunwind and any other libraries in
# need of protection from the application (e.g. C++).
libs += shared_library(
  'hpcrun_private_ns',
  'audit/private-ns.c',
  'audit/exports.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  link_language: 'c',
  install: true,
)

# Shim to load and access libunwind in a private link namespace.
libs += shared_library(
  'hpcrun_unwind',
  'unwind/common/libunwind-pvt.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [libunwind_dep],
  link_language: 'c',
  install: true,
)

# Main library for hpcrun, this is where most everything happens.

srcs = files(
  'audit/binding.c',
  'cct/cct-node-vector.c',
  'cct/cct.c',
  'cct/cct_bundle.c',
  'cct/cct_ctxt.c',
  'cct2metrics.c',
  'cct_backtrace_finalize.c',
  'cct_insert_backtrace.c',
  'closure-registry.c',
  'control-knob.c',
  'custom-init-dynamic.c',
  'device-finalizers.c',
  'device-initializers.c',
  'disabled.c',
  'env.c',
  'epoch.c',
  'files.c',
  'fnbounds/fnbounds.c',
  'fnbounds/fnbounds_common.c',
  'fnbounds/fnbounds_dynamic.c',
  'fnbounds/scan.c',
  'foil/client.c',
  'foil/common.c',
  'foil/ga.c',
  'foil/libc.c',
  'foil/mpi.c',
  'foil/ompt.c',
  'gpu/activity/correlation/gpu-correlation-channel.c',
  'gpu/activity/correlation/gpu-host-correlation-map.c',
  'gpu/activity/gpu-activity-channel.c',
  'gpu/activity/gpu-activity-process.c',
  'gpu/activity/gpu-activity.c',
  'gpu/activity/gpu-event-id-map.c',
  'gpu/activity/gpu-op-ccts-map.c',
  'gpu/activity/gpu-op-placeholders.c',
  'gpu/api/common/gpu-binary.c',
  'gpu/api/common/gpu-cct.c',
  'gpu/api/common/gpu-channel-item-allocator.c',
  'gpu/api/common/gpu-flush-alarm.c',
  'gpu/api/common/gpu-instrumentation.c',
  'gpu/api/common/gpu-kernel-table.c',
  'gpu/api/ompt/ompt-activity-translate.c',
  'gpu/api/ompt/ompt-gpu-api.c',
  'gpu/blame-shifting/active-kernels-map.c',
  'gpu/blame-shifting/blame-helper.c',
  'gpu/blame-shifting/blame-kernel-map.c',
  'gpu/blame-shifting/blame-queue-map.c',
  'gpu/common/base-channel-set.c',
  'gpu/common/gpu-monitoring.c',
  'gpu/gpu-application-thread-api.c',
  'gpu/gpu-metrics.c',
  'gpu/gpu-monitoring-thread-api.c',
  'gpu/gpu-splay-allocator.c',
  'gpu/operation/gpu-operation-channel-set.c',
  'gpu/operation/gpu-operation-channel.c',
  'gpu/operation/gpu-operation-item-process.c',
  'gpu/operation/gpu-operation-item.c',
  'gpu/operation/gpu-operation-multiplexer.c',
  'gpu/trace/gpu-context-id-map.c',
  'gpu/trace/gpu-stream-id-map.c',
  'gpu/trace/gpu-trace-api.c',
  'gpu/trace/gpu-trace-channel-set.c',
  'gpu/trace/gpu-trace-channel.c',
  'gpu/trace/gpu-trace-demultiplexer.c',
  'gpu/trace/gpu-trace-item.c',
  'handling_sample.c',
  'hpcrun-initializers.c',
  'hpcrun_options.c',
  'hpcrun_signals.c',
  'hpcrun_stats.c',
  'libmonitor/callback.c',
  'libmonitor/fork.c',
  'libmonitor/main.c',
  'libmonitor/mpi.c',
  'libmonitor/mpi_comm_c.c',
  'libmonitor/mpi_comm_f0.c',
  'libmonitor/mpi_comm_f1.c',
  'libmonitor/mpi_comm_f2.c',
  'libmonitor/mpi_final_c.c',
  'libmonitor/mpi_final_f0.c',
  'libmonitor/mpi_final_f1.c',
  'libmonitor/mpi_final_f2.c',
  'libmonitor/mpi_init_c.c',
  'libmonitor/mpi_init_f0.c',
  'libmonitor/mpi_init_f1.c',
  'libmonitor/mpi_init_f2.c',
  'libmonitor/mpi_init_thread_c.c',
  'libmonitor/mpi_init_thread_f0.c',
  'libmonitor/mpi_init_thread_f1.c',
  'libmonitor/mpi_init_thread_f2.c',
  'libmonitor/pmpi.c',
  'libmonitor/pthread.c',
  'libmonitor/signal.c',
  'libmonitor/utils.c',
  'loadmap.c',
  'logical/common.c',
  'lush/lush-backtrace.c',
  'lush/lush-pthread.c',
  'lush/lush-support-rt.c',
  'lush/lush.c',
  'lush/lushi-cb.c',
  'main.c',
  'memory/mem.c',
  'memory/mmap.c',
  'messages/debug-flag.c',
  'messages/errors.c',
  'messages/fmt.c',
  'messages/messages-async.c',
  'messages/messages-sync.c',
  'metrics.c',
  'module-ignore-map.c',
  'monitor-exts/openmp.c',
  'name.c',
  'ompt/ompt-callstack.c',
  'ompt/ompt-defer-write.c',
  'ompt/ompt-defer.c',
  'ompt/ompt-device-map.c',
  'ompt/ompt-device.c',
  'ompt/ompt-interface.c',
  'ompt/ompt-queues.c',
  'ompt/ompt-region-debug.c',
  'ompt/ompt-region.c',
  'ompt/ompt-task.c',
  'ompt/ompt-thread.c',
  'rank.c',
  'safe-sampling.c',
  'sample-sources/blame-shift/blame-map.c',
  'sample-sources/blame-shift/blame-shift.c',
  'sample-sources/blame-shift/directed.c',
  'sample-sources/blame-shift/undirected.c',
  'sample-sources/common.c',
  'sample-sources/display.c',
  'sample-sources/exclude.c',
  'sample-sources/ga-overrides.c',
  'sample-sources/ga.c',
  'sample-sources/idle.c',
  'sample-sources/io-over.c',
  'sample-sources/io.c',
  'sample-sources/itimer.c',
  'sample-sources/memleak-overrides.c',
  'sample-sources/memleak.c',
  'sample-sources/none.c',
  'sample-sources/omp-idle.c',
  'sample-sources/omp-mutex.c',
  'sample-sources/pthread-blame-overrides.c',
  'sample-sources/pthread-blame.c',
  'sample-sources/retcnt.c',
  'sample-sources/sample-filters.c',
  'sample-sources/sync.c',
  'sample_event.c',
  'sample_prob.c',
  'sample_sources_all.c',
  'sample_sources_registered.c',
  'segv_handler.c',
  'start-stop.c',
  'term_handler.c',
  'thread_data.c',
  'thread_finalize.c',
  'thread_use.c',
  'threadmgr.c',
  'trace.c',
  'unwind/common/backtrace.c',
  'unwind/common/binarytree_uwi.c',
  'unwind/common/interval_t.c',
  'unwind/common/libunw_intervals.c',
  'unwind/common/libunwind-interface.c',
  'unwind/common/stack_troll.c',
  'unwind/common/unw-throw.c',
  'unwind/common/uw_hash.c',
  'unwind/common/uw_recipe_map.c',
  'utilities/executable-path.c',
  'utilities/first_func.c',
  'utilities/hpcrun-nanotime.c',
  'utilities/ip-normalized.c',
  'utilities/last_func.c',
  'utilities/line_wrapping.c',
  'utilities/linuxtimer.c',
  'utilities/timer.c',
  'utilities/tokenize.c',
  'utilities/unlink.c',
  'weak.c',
  'write_data.c',
)

if host_machine.cpu_family() in ['x86', 'x86_64']
  srcs += files(
    'trampoline/x86-family/x86-tramp.S',
    'unwind/x86-family/amd-xop.c',
    'unwind/x86-family/manual-intervals/x86-32bit-icc-variant.c',
    'unwind/x86-family/manual-intervals/x86-32bit-main.c',
    'unwind/x86-family/manual-intervals/x86-fail-intervals.c',
    'unwind/x86-family/manual-intervals/x86-gcc-adjust.c',
    'unwind/x86-family/manual-intervals/x86-gcc-main64.c',
    'unwind/x86-family/manual-intervals/x86-intel-align32.c',
    'unwind/x86-family/manual-intervals/x86-intel-align64.c',
    'unwind/x86-family/manual-intervals/x86-intel-composer2013-mic.c',
    'unwind/x86-family/manual-intervals/x86-intel11-f90main.c',
    'unwind/x86-family/manual-intervals/x86-linux-dlresolver.c',
    'unwind/x86-family/manual-intervals/x86-pgi-mp_pexit.c',
    'unwind/x86-family/x86-addsub.c',
    'unwind/x86-family/x86-and.c',
    'unwind/x86-family/x86-build-intervals.c',
    'unwind/x86-family/x86-call.c',
    'unwind/x86-family/x86-canonical.c',
    'unwind/x86-family/x86-cold-path.c',
    'unwind/x86-family/x86-debug.c',
    'unwind/x86-family/x86-decoder.c',
    'unwind/x86-family/x86-enter.c',
    'unwind/x86-family/x86-jump.c',
    'unwind/x86-family/x86-lea.c',
    'unwind/x86-family/x86-leave.c',
    'unwind/x86-family/x86-move.c',
    'unwind/x86-family/x86-process-inst.c',
    'unwind/x86-family/x86-push.c',
    'unwind/x86-family/x86-return.c',
    'unwind/x86-family/x86-unwind-interval-fixup.c',
    'unwind/x86-family/x86-unwind-interval.c',
    'unwind/x86-family/x86-unwind-support.c',
    'unwind/x86-family/x86-unwind.c',
    'unwind/x86-family/x86-validate-retn-addr.c',
    'utilities/arch/x86-family/x86-context-pc.c',
  )
elif host_machine.cpu_family() == 'ppc64'
  srcs += files(
    'trampoline/ppc64/ppc64-tramp.s',
    'unwind/common/default_validation_summary.c',
    'unwind/ppc64/ppc64-unwind-interval.c',
    'unwind/ppc64/ppc64-unwind.c',
    'utilities/arch/ppc64/ppc64-context-pc.c',
  )
elif host_machine.cpu_family() == 'aarch64'
  srcs += files(
    'trampoline/aarch64/aarch64-tramp.c',
    'unwind/common/default_validation_summary.c',
    'unwind/generic-libunwind/libunw-unwind.c',
    'utilities/arch/libunwind/libunwind-context-pc.c',
  )
elif host_machine.cpu_family() == 'mips'
  srcs += files('unwind/common/default_validation_summary.c')
elif host_machine.cpu_family() == 'ia64'
  srcs += files(
    'trampoline/ia64/ia64-tramp.s',
    'unwind/common/default_validation_summary.c',
    'unwind/generic-libunwind/libunw-unwind.c',
    'utilities/arch/ia64/ia64-context-pc.c',
  )
endif

if host_machine.cpu_family() == 'ppc64'
  srcs += files('trampoline/common/trampoline_eager.c')
else
  srcs += files('trampoline/common/trampoline_lazy.c')
endif

if has_perf_events
  srcs += files(
    'sample-sources/perf/event_custom.c',
    'sample-sources/perf/event_name_parser.c',
    'sample-sources/perf/linux_perf.c',
    'sample-sources/perf/perf-util.c',
    'sample-sources/perf/perf_event_open.c',
    'sample-sources/perf/perf_mmap.c',
    'sample-sources/perf/perf_skid.c',
  )

  if perfmon_dep.found()
    srcs += files('sample-sources/perf/perfmon-util.c')
  else
    srcs += files('sample-sources/perf/perfmon-util-dummy.c')
  endif

  _kernel_43_code = '''
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
#else
#error Kernel version too low, less than 4.3
#endif
'''
  if cc.compiles(_kernel_43_code, name: 'Linux >= 4.3')
    srcs += files('sample-sources/perf/kernel_blocking.c')
  else
    srcs += files('sample-sources/perf/kernel_blocking_stub.c')
  endif
endif

if papi_dep.found()
  if papi_has_components
    srcs += files(
      'sample-sources/papi-c-extended-info.c',
      'sample-sources/papi-c-intel.c',
      'sample-sources/papi-c.c',
    )
    if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
      srcs += files('sample-sources/papi-c-cupti.c')
    endif
  else
    srcs += files('sample-sources/papi.c')
  endif

  srcs += files(
    'gpu/api/opencl/intel/papi/papi-kernel-map.c',
    'gpu/api/opencl/intel/papi/papi-metric-collector.c',
  )

  if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
    srcs += files('sample-sources/papi-c-cupti.c')
  endif
endif

if python_inst.found()
  srcs += files('foil/python.c', 'logical/python.c')
endif

if cupti_dep.found()
  srcs += files(
    'foil/nvidia.c',
    'gpu/api/nvidia/cubin-id-map.c',
    'gpu/api/nvidia/cubin-symbols.c',
    'gpu/api/nvidia/cuda-api.c',
    'gpu/api/nvidia/cuda-correlation-id-map.c',
    'gpu/api/nvidia/cuda-device-map.c',
    'gpu/api/nvidia/cuda-kernel-data-map.c',
    'gpu/api/nvidia/cupti-activity-translate.c',
    'gpu/api/nvidia/cupti-analysis.c',
    'gpu/api/nvidia/cupti-api.c',
    'gpu/api/nvidia/cupti-gpu-api.c',
    'gpu/api/nvidia/gpu-function-id-map.c',
    'sample-sources/nvidia.c',
  )

  # Access to CUDA/CUPTI is relegated through this dlopen'd foil
  libs += shared_library(
    'hpcrun_dlopen_nvidia',
    'foil/nvidia-dlopen.c',
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    dependencies: [cupti_dep],
    install: true,
  )
endif

if rocm_dep.found()
  srcs += files(
    'foil/rocm-hip.c',
    'foil/rocm-hsa.c',
    'foil/rocprofiler.c',
    'foil/roctracer.c',
    'gpu/api/amd/hip-api.c',
    'gpu/api/amd/rocprofiler-api.c',
    'gpu/api/amd/roctracer-activity-translate.c',
    'gpu/api/amd/roctracer-api.c',
    'sample-sources/amd-rocprofiler.c',
    'sample-sources/amd.c',
    'sample-sources/openmp-target.c',
  )
endif

if opencl_dep.found()
  srcs += files(
    'foil/opencl.c',
    'gpu/api/opencl/intel/maps/buffer-map.c',
    'gpu/api/opencl/intel/maps/device-map.c',
    'gpu/api/opencl/intel/maps/kernel-context-map.c',
    'gpu/api/opencl/intel/maps/kernel-param-map.c',
    'gpu/api/opencl/intel/maps/queue-context-map.c',
    'gpu/api/opencl/intel/optimization-check.c',
    'gpu/api/opencl/opencl-activity-translate.c',
    'gpu/api/opencl/opencl-api.c',
    'gpu/api/opencl/opencl-context-map.c',
    'gpu/api/opencl/opencl-h2d-map.c',
    'gpu/api/opencl/opencl-kernel-loadmap-map.c',
    'gpu/api/opencl/opencl-memory-manager.c',
    'gpu/api/opencl/opencl-queue-map.c',
    'gpu/blame-shifting/blame-kernel-cleanup-map.c',
    'gpu/blame-shifting/blame.c',
    'gpu/blame-shifting/opencl/opencl-blame.c',
    'sample-sources/opencl.c',
  )
endif

if level0_dep.found()
  srcs += files(
    'foil/level0.c',
    'gpu/api/intel/level0/level0-api.c',
    'gpu/api/intel/level0/level0-binary.c',
    'gpu/api/intel/level0/level0-command-list-context-map.c',
    'gpu/api/intel/level0/level0-command-list-map.c',
    'gpu/api/intel/level0/level0-command-process.c',
    'gpu/api/intel/level0/level0-command-queue-map.c',
    'gpu/api/intel/level0/level0-data-node.c',
    'gpu/api/intel/level0/level0-debug.c',
    'gpu/api/intel/level0/level0-event-map.c',
    'gpu/api/intel/level0/level0-fence-map.c',
    'gpu/api/intel/level0/level0-handle-map.c',
    'gpu/api/intel/level0/level0-kernel-module-map.c',
    'sample-sources/level0.c',
  )
endif

if gtpin_dep.found()
  srcs += files('gpu/api/intel/gtpin/gtpin-shim.c')

  libs += shared_library(
    'hpcrun_gtpin_cxx',
    'foil/gtpin.cpp',
    'gpu/api/intel/gtpin/gtpin-instrumentation.cpp',
    'gpu/api/intel/binaries/patchTokenSymbols.c',
    'gpu/api/intel/binaries/symbolVector.c',
    'gpu/api/intel/binaries/zebinSymbols.c',
    common_lean_srcs,
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    dependencies: [
      common_lean_deps,
      dl_dep,
      gtpin_dep.partial_dependency(compile_args: true, includes: true),
      libelf_dep,
      libunwind_dep,
      tbb_malloc_dep,
      threads_dep,
    ],
    install: true,
  )

  # Access to GTPin is relegated through this dlopen'd foil
  libs += shared_library(
    'hpcrun_dlopen_gtpin',
    'foil/gtpin-dlopen.cpp',
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    dependencies: [gtpin_dep],
    install: true,
  )
endif

main_hpcrun_lib = shared_library(
  'hpcrun',
  srcs,
  common_lean_srcs,
  gnu_symbol_visibility: 'hidden',
  link_language: 'c',
  implicit_include_directories: false,
  dependencies: [
    common_lean_deps,
    cupti_dep.partial_dependency(compile_args: true, includes: true),
    dl_dep,
    gtpin_dep.partial_dependency(compile_args: true, includes: true),
    level0_dep.partial_dependency(compile_args: true, includes: true),
    libdw_dep,
    libelf_dep,
    libunwind_dep,
    lzma_dep,
    math_dep,
    opencl_dep,
    papi_dep,
    perfmon_dep,
    rocm_dep.partial_dependency(compile_args: true, includes: true),
    rt_dep,
    xed_dep,
    (python_inst.found() ? python_inst.dependency() : dependency('', required: false)).partial_dependency(
      compile_args: true,
      includes: true,
    ),
  ],
  install: true,
)
libs += main_hpcrun_lib

# Libraries needed to support the "audit" interface for collecting loaded libraries
libs += shared_library(
  'hpcrun_audit',
  'audit/auditor.c',
  'audit/private-ns.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  link_language: 'c',
  install: true,
)

# Symbol interceptions are handled via this LD_PRELOAD'd foil
libs += shared_library(
  'hpcrun_preload',
  'foil/common-preload.c',
  'foil/client-preload.c',
  'foil/libc-preload.c',
  'foil/mpi-preload.c',
  'foil/ompt-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  install: true,
)

# Optional symbol interception LD_PRELOAD'd foils for Glibc
libs += shared_library(
  'hpcrun_preload_libc_io',
  'foil/common-preload.c',
  'foil/libc-io-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  install: true,
)
libs += shared_library(
  'hpcrun_preload_libc_alloc',
  'foil/common-preload.c',
  'foil/libc-alloc-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  install: true,
)
libs += shared_library(
  'hpcrun_preload_libc_sync',
  'foil/common-preload.c',
  'foil/libc-sync-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  install: true,
)

# Optional symbol interception LD_PRELOAD'd foils for libdl
libs += shared_library(
  'hpcrun_preload_libdl',
  'foil/libdl-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  link_language: 'c',
  install: true,
)
libs += shared_library(
  'hpcrun_preload_libdl_dlmopen',
  'foil/libdl-dlmopen-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep],
  link_language: 'c',
  install: true,
)

# Optional symbol interception LD_PRELOAD'd foils for smaller features
libs += shared_library(
  'hpcrun_preload_gprof',
  'foil/gprof-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  install: true,
)
libs += shared_library(
  'hpcrun_preload_ga',
  'foil/common-preload.c',
  'foil/ga-preload.c',
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  dependencies: [dl_dep, threads_dep],
  install: true,
)

# Symbols needed for integration with ROCProfiler etc. are defined by this foil
if rocm_dep.found()
  libs += shared_library(
    'hpcrun_dlopen_rocm',
    'foil/rocm-hip-dlopen.c',
    'foil/rocm-hsa-dlopen.c',
    'foil/rocprofiler-dlopen.c',
    'foil/roctracer-dlopen.c',
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    link_with: main_hpcrun_lib,
    dependencies: [rocm_dep],
    install: true,
  )
endif

# Optional symbol interception LD_PRELOAD'd foil for OpenCL
if opencl_dep.found()
  libs += shared_library(
    'hpcrun_preload_opencl',
    'foil/common-preload.c',
    'foil/opencl-preload.c',
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    dependencies: [dl_dep, threads_dep, opencl_dep],
    install: true,
  )
endif

# Optional symbol interception LD_PRELOAD'd foil for Level Zero
if level0_dep.found()
  libs += shared_library(
    'hpcrun_preload_level0',
    'foil/common-preload.c',
    'foil/level0-preload.c',
    gnu_symbol_visibility: 'hidden',
    implicit_include_directories: false,
    dependencies: [dl_dep, threads_dep, level0_dep],
    install: true,
  )
endif

# Configure and install the hpcrun launcher
_prefixes_cfg = configuration_data()
_prefixes_cfg.set_quoted(
  'HPCRUN_INSTALL_LIBDIR',
  get_option('prefix') / get_option('libdir'),
)
_prefixes_h = configure_file(
  output: 'hpcrun-install-prefixes.h',
  configuration: _prefixes_cfg,
)
hpcrun = executable('hpcrun', 'launch.cpp', version_cpp, _prefixes_h, install: true)

# Build and install the client API
install_headers('hpctoolkit.h')
hpctoolkit_client_lib = library('hpctoolkit', 'hpctoolkit.c', install: true)

# Settings needed for dependencies
hpcrun_test_depends = libs
test_depends += hpcrun_test_depends
