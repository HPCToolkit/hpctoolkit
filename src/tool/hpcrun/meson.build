_hpcrun_libs = []

_unw_common_files = files(
  'unwind/common/backtrace.c',
  'unwind/common/unw-throw.c',
  'unwind/common/binarytree_uwi.c',
  'unwind/common/interval_t.c',
  'unwind/common/libunw_intervals.c',
  'unwind/common/libunwind-interface.c',
  'unwind/common/stack_troll.c',
  'unwind/common/uw_hash.c',
  'unwind/common/uw_recipe_map.c',
)

_unw_x86_files = _unw_common_files + files(
  'unwind/x86-family/x86-addsub.c',
  'unwind/x86-family/x86-and.c',
  'unwind/x86-family/x86-build-intervals.c',
  'unwind/x86-family/x86-call.c',
  'unwind/x86-family/x86-canonical.c',
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
  'unwind/x86-family/amd-xop.c',
  'unwind/x86-family/x86-cold-path.c',
  'unwind/x86-family/x86-validate-retn-addr.c',
  'unwind/x86-family/x86-unwind-interval.c',
  'unwind/x86-family/x86-unwind-interval-fixup.c',
  'unwind/x86-family/x86-unwind.c',
  'unwind/x86-family/x86-unwind-support.c',
  'unwind/x86-family/manual-intervals/x86-gcc-adjust.c',
  'unwind/x86-family/manual-intervals/x86-gcc-main64.c',
  'unwind/x86-family/manual-intervals/x86-linux-dlresolver.c',
  'unwind/x86-family/manual-intervals/x86-intel11-f90main.c',
  'unwind/x86-family/manual-intervals/x86-intel-align32.c',
  'unwind/x86-family/manual-intervals/x86-intel-align64.c',
  'unwind/x86-family/manual-intervals/x86-intel-composer2013-mic.c',
  'unwind/x86-family/manual-intervals/x86-32bit-main.c',
  'unwind/x86-family/manual-intervals/x86-32bit-icc-variant.c',
  'unwind/x86-family/manual-intervals/x86-fail-intervals.c',
  'unwind/x86-family/manual-intervals/x86-pgi-mp_pexit.c',
)
_unw_x86_include_dirs_deps = [xed_dep]

_unw_ppc64_files = _unw_common_files + files(
  'unwind/ppc64/ppc64-unwind.c',
  'unwind/ppc64/ppc64-unwind-interval.c',
  'unwind/common/default_validation_summary.c',
)

_unw_libunw_files = _unw_common_files + files(
  'unwind/generic-libunwind/libunw-unwind.c',
  'unwind/common/default_validation_summary.c',
)

_my_cpp_defines = [
  '-D_GNU_SOURCE',
  '-D__HIP_PLATFORM_HCC__=1',
  '-D__HIP_PLATFORM_AMD__=1',
]

_my_base_files = files(
  'audit/binding.c',
  'cct_backtrace_finalize.c',
  'cct_insert_backtrace.c',
  'cct/cct_bundle.c',
  'cct/cct_ctxt.c',
  'cct/cct-node-vector.c',
  'cct/cct.c',
  'cct2metrics.c',
  'closure-registry.c',
  'control-knob.c',
  'device-finalizers.c',
  'device-initializers.c',
  'disabled.c',
  'env.c',
  'epoch.c',
  'files.c',
  'fnbounds/fnbounds_common.c',
  'foil/foil.c',
  'gpu/blame-shifting/active-kernels-map.c',
  'gpu/blame-shifting/blame-helper.c',
  'gpu/blame-shifting/blame-kernel-map.c',
  'gpu/blame-shifting/blame-queue-map.c',
  'gpu/blame-shifting/blame.c',
  'gpu/gpu-activity-channel.c',
  'gpu/gpu-activity-process.c',
  'gpu/gpu-activity.c',
  'gpu/gpu-application-thread-api.c',
  'gpu/gpu-binary.c',
  'gpu/gpu-cct.c',
  'gpu/gpu-channel-item-allocator.c',
  'gpu/gpu-context-id-map.c',
  'gpu/gpu-correlation-channel-set.c',
  'gpu/gpu-correlation-channel.c',
  'gpu/gpu-correlation-id-map.c',
  'gpu/gpu-correlation-id.c',
  'gpu/gpu-correlation.c',
  'gpu/gpu-event-id-map.c',
  'gpu/gpu-flush-alarm.c',
  'gpu/gpu-function-id-map.c',
  'gpu/gpu-host-correlation-map.c',
  'gpu/gpu-instrumentation.c',
  'gpu/gpu-kernel-table.c',
  'gpu/gpu-metrics.c',
  'gpu/gpu-monitoring-thread-api.c',
  'gpu/gpu-monitoring.c',
  'gpu/gpu-op-placeholders.c',
  'gpu/gpu-operation-channel-set.c',
  'gpu/gpu-operation-channel.c',
  'gpu/gpu-operation-item-process.c',
  'gpu/gpu-operation-item.c',
  'gpu/gpu-operation-multiplexer.c',
  'gpu/gpu-splay-allocator.c',
  'gpu/gpu-stream-id-map.c',
  'gpu/gpu-trace-channel-set.c',
  'gpu/gpu-trace-channel.c',
  'gpu/gpu-trace-demultiplexer.c',
  'gpu/gpu-trace-item.c',
  'gpu/gpu-trace.c',
  'gpu/ompt/ompt-activity-translate.c',
  'gpu/ompt/ompt-gpu-api.c',
  'handling_sample.c',
  'hpcrun_options.c',
  'hpcrun_signals.c',
  'hpcrun_stats.c',
  'hpcrun-initializers.c',
  'libmonitor/callback.c',
  'libmonitor/common.c',
  'libmonitor/fork.c',
  'libmonitor/main.c',
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
  'libmonitor/mpi.c',
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
  'sample_event.c',
  'sample_prob.c',
  'sample_sources_all.c',
  'sample_sources_registered.c',
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
  'segv_handler.c',
  'start-stop.c',
  'term_handler.c',
  'thread_data.c',
  'thread_finalize.c',
  'thread_use.c',
  'threadmgr.c',
  'trace.c',
  'utilities/executable-path.c',
  'utilities/first_func.c',
  'utilities/hpcrun-nanotime.c',
  'utilities/ip-normalized.c',
  'utilities/line_wrapping.c',
  'utilities/linuxtimer.c',
  'utilities/timer.c',
  'utilities/tokenize.c',
  'utilities/unlink.c',
  'weak.c',
  'write_data.c',
)

if host_machine.cpu_family() == 'ppc64'
  _my_base_files += files('trampoline/common/trampoline_eager.c')
else
  _my_base_files += files('trampoline/common/trampoline_lazy.c')
endif

if cc.has_type('struct perf_event_attr', prefix: ['#include <linux/perf_event.h>', '#include <linux/hw_breakpoint.h>', '#include <sys/syscall.h>', '#include <unistd.h>'])
  _my_base_files += files(
    'sample-sources/perf/event_custom.c',
    'sample-sources/perf/linux_perf.c',
    'sample-sources/perf/perf_event_open.c',
    'sample-sources/perf/perf-util.c',
    'sample-sources/perf/perf_mmap.c',
    'sample-sources/perf/perf_skid.c',
  )
  _my_cpp_defines += '-DHPCRUN_SS_LINUX_PERF'

  if perfmon_dep.found()
    _my_base_files += files('sample-sources/perf/perfmon-util.c')
  else
    _my_base_files += files('sample-sources/perf/perfmon-util-dummy.c')
  endif

  _kernel_43_code = '''
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
#else
#error Kernel version too low, less than 4.3
#endif
'''
  if cc.compiles(_kernel_43_code, name: 'Linux >= 4.3')
    _my_base_files += files('sample-sources/perf/kernel_blocking.c')
  else
    _my_base_files += files('sample-sources/perf/kernel_blocking_stub.c')
  endif
endif

_my_dynamic_files = files(
  'fnbounds/fnbounds_client.c',
  'fnbounds/fnbounds_dynamic.c',
  'monitor-exts/openmp.c',
  'custom-init-dynamic.c',
  'foil/hpctoolkit-preload.c',
  'foil/ompt-preload.c',
  'foil/process-preload.c',
  'foil/signal-preload.c',
  'foil/thread-preload.c',
  'foil/mpi-preload.c',
  'foil/syscalls/poll-preload.c',
  'foil/syscalls/ppoll-preload.c',
  'foil/syscalls/select-preload.c',
  'foil/syscalls/sysv_signal-preload.c',
)
_my_linux_dynamic_files = files(
  'fnbounds/fnbounds_client.c',
  'fnbounds/fnbounds_dynamic.c',
  'monitor-exts/openmp.c',
  'custom-init-dynamic.c',
  'foil/hpctoolkit-preload.c',
  'foil/ompt-preload.c',
  'foil/process-preload.c',
  'foil/signal-preload.c',
  'foil/thread-preload.c',
  'foil/mpi-preload.c',
  'foil/syscalls/poll-preload.c',
  'foil/syscalls/ppoll-preload.c',
  'foil/syscalls/select-preload.c',
  'foil/syscalls/sysv_signal-preload.c',
)
_my_mips_files = files(
  'unwind/common/default_validation_summary.c',
)
_my_ppc_files = files(
  'trampoline/ppc64/ppc64-tramp.s',
  'utilities/arch/ppc64/ppc64-context-pc.c',
)
_my_x86_files = files(
  'trampoline/x86-family/x86-tramp.S',
  'utilities/arch/x86-family/x86-context-pc.c',
)
_my_ia64_files = files(
  'trampoline/ia64/ia64-tramp.s',
  'utilities/arch/ia64/ia64-context-pc.c',
)
_my_aarch64_files = files(
  'trampoline/aarch64/aarch64-tramp.c',
  'utilities/arch/libunwind/libunwind-context-pc.c',
)

_my_papi_files = []
if papi_dep.found() and cc.has_header_symbol('papi.h', 'PAPI_get_eventset_component', dependencies: papi_dep)
  _my_cpp_defines += '-DHPCRUN_SS_PAPI_C_INTEL'
  _my_papi_files = files(
    'sample-sources/papi-c.c',
    'sample-sources/papi-c-extended-info.c',
    'sample-sources/papi-c-intel.c',
  )
  if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
    _my_papi_files += files('sample-sources/papi-c-cupti.c')
  endif
else
  _my_papi_files += files(
    'sample-sources/papi.c'
  )
endif

_my_papi_files += files(
  'gpu/opencl/intel/papi/papi-kernel-map.c',
  'gpu/opencl/intel/papi/papi-metric-collector.c',
)

if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
  _my_papi_files += files('sample-sources/papi-c-cupti.c')
endif

if cupti_dep.found()
  _my_cupti_files = files(
    'sample-sources/nvidia.c',
    'gpu/nvidia/cubin-id-map.c',
    'gpu/nvidia/cubin-symbols.c',
    'gpu/nvidia/cuda-api.c',
    'gpu/nvidia/cuda-device-map.c',
    'gpu/nvidia/cupti-activity-translate.c',
    'gpu/nvidia/cupti-analysis.c',
    'gpu/nvidia/cupti-api.c',
    'gpu/nvidia/cupti-gpu-api.c',
  )
endif

if opencl_dep.found()
  _my_opencl_files = files(
    'sample-sources/opencl.c',
    'gpu/opencl/opencl-api.c',
    'gpu/opencl/opencl-memory-manager.c',
    'gpu/opencl/opencl-activity-translate.c',
    'gpu/opencl/opencl-h2d-map.c',
    'gpu/opencl/opencl-queue-map.c',
    'gpu/opencl/opencl-context-map.c',
    'gpu/opencl/intel/optimization-check.c',
    'gpu/opencl/opencl-kernel-loadmap-map.c',
    'gpu/opencl/intel/maps/queue-context-map.c',
    'gpu/opencl/intel/maps/kernel-context-map.c',
    'gpu/opencl/intel/maps/kernel-param-map.c',
    'gpu/opencl/intel/maps/buffer-map.c',
    'gpu/opencl/intel/maps/device-map.c',
    'gpu/blame-shifting/blame-kernel-cleanup-map.c',
    'gpu/blame-shifting/opencl/opencl-blame.c',
  )
  _my_cpp_defines += '-DHPCRUN_SS_OPENCL'

  _hpcrun_libs += shared_module('hpcrun_opencl', config_h,
    'foil/opencl-preload.c',
    gnu_symbol_visibility: 'hidden',
    link_language: 'c',
    dependencies: opencl_dep,
    install: true, install_dir: 'lib' / meson.project_name())
endif
_my_include_dirs_deps = [
  libunwind_dep,
  gtpin_dep.partial_dependency(compile_args: true, includes: true),
  rocm_dep.partial_dependency(compile_args: true, includes: true),
  libelf_dep,
  cupti_dep.partial_dependency(compile_args: true, includes: true),
]

if gtpin_dep.found()
  _my_dynamic_files += files('gpu/intel/gtpin/gtpin-shim.c')

  _hpcrun_libs += shared_library('hpcrun_gtpin_cxx', config_h,
    'gpu/intel/gtpin/gtpin-instrumentation.cpp',
    'gpu/intel/binaries/patchTokenSymbols.c',
    'gpu/intel/binaries/symbolVector.c',
    'gpu/intel/binaries/zebinSymbols.c',
    gnu_symbol_visibility: 'hidden',
    link_with: [prof_lean_lib],
    dependencies: [libelf_dep, libunwind_dep, tbb_malloc_dep, gtpin_dep, dl_dep],
    install: true, install_dir: 'lib' / meson.project_name())
endif

if rocm_dep.found()
  _my_rocm_files = files(
    'sample-sources/amd.c',
    'sample-sources/amd-rocprofiler.c',
    'sample-sources/openmp-target.c',
    'gpu/amd/hip-api.c',
    'gpu/amd/roctracer-activity-translate.c',
    'gpu/amd/roctracer-api.c',
    'gpu/amd/rocprofiler-api.c',
    'foil/rocm-preload.c',
  )
endif

if level0_dep.found()
  _my_level0_files = files(
    'sample-sources/level0.c',
    'gpu/intel/level0/level0-api.c',
    'gpu/intel/level0/level0-binary.c',
    'gpu/intel/level0/level0-command-list-context-map.c',
    'gpu/intel/level0/level0-command-list-map.c',
    'gpu/intel/level0/level0-command-process.c',
    'gpu/intel/level0/level0-data-node.c',
    'gpu/intel/level0/level0-event-map.c',
    'gpu/intel/level0/level0-handle-map.c',
    'gpu/intel/level0/level0-kernel-module-map.c',
    'gpu/intel/level0/level0-fence-map.c',
    'gpu/intel/level0/level0-command-queue-map.c',
  )

  _hpcrun_libs += shared_module('hpcrun_level0', config_h,
    'foil/level0-preload.c',
    gnu_symbol_visibility: 'hidden',
    link_language: 'c',
    dependencies: level0_dep,
    install: true, install_dir: 'lib' / meson.project_name()
  )
endif

if papi_dep.found()
  _my_cpp_defines += '-DHPCRUN_SS_PAPI'
endif
if cupti_dep.found()
  _my_cpp_defines += '-DHPCRUN_SS_NVIDIA'
endif
if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
  _my_cpp_defines += '-DHPCRUN_SS_PAPI_C_CUPTI'
endif
if false  # get_option('papi_rocm')  # --enable-papi-c-rocm
  _my_cpp_defines += '-DHPCRUN_SS_PAPI_C_ROCM'
endif

if host_machine.cpu_family() in ['x86', 'x86_64']  # UNW_X86
  _unw_source_files = _unw_x86_files
endif

if host_machine.cpu_family() in ['ppc64']  # UNW_PPC64
  _unw_source_files = _unw_ppc64_files
endif

if host_machine.cpu_family() in ['aarch64', 'ia64']  # UNW_LIBUNW
  _unw_source_files = _unw_libunw_files
endif

if rocm_dep.found()
  _my_cpp_defines += ['-DHPCRUN_SS_AMD']
endif

if level0_dep.found()
  _my_cpp_defines += ['-DHPCRUN_SS_LEVEL0']
endif

_hpcrun_libs += shared_module('hpcrun', config_h,
  _my_base_files,
  _my_dynamic_files,
  host_machine.system() == 'linux' ? _my_linux_dynamic_files : [],
  host_machine.cpu_family() == 'mips' ? _my_mips_files : [],
  host_machine.cpu_family() == 'ppc64' ? _my_ppc_files : [],
  host_machine.cpu_family() in ['x86', 'x86_64'] ? _my_x86_files : [],
  host_machine.cpu_family() == 'ia64' ? _my_ia64_files : [],
  host_machine.cpu_family() == 'aarch64' ? _my_aarch64_files : [],
  papi_dep.found() ? _my_papi_files : [],
  python_inst.found() ? ['logical/python.c'] : [],
  cupti_dep.found() ? _my_cupti_files : [],
  rocm_dep.found() ? _my_rocm_files : [],
  level0_dep.found() ? _my_level0_files : [],
  opencl_dep.found() ? _my_opencl_files : [],
  _unw_source_files,
  'utilities/last_func.c',
  gnu_symbol_visibility: 'hidden',
  link_language: 'c',
  c_args: (
    _my_cpp_defines
    + (cupti_dep.found() ? ['-DENABLE_CUDA'] : [])
    + (opencl_dep.found() ? ['-DENABLE_OPENCL'] : [])
    + (level0_dep.found() ? ['-DUSE_LEVEL0'] : [])
  ),
  link_with: [prof_lean_lib, support_lean_lib],
  dependencies: [
    _my_include_dirs_deps,
    math_dep,
    dl_dep,
    rt_dep,
    lzma_dep,
    libelf_dep,
    perfmon_dep,
    xed_dep,
    papi_dep,
    rocm_dep.partial_dependency(compile_args: true, includes: true),
    cupti_dep.partial_dependency(compile_args: true, includes: true),
    (python_inst.found() ? python_inst.dependency() : dependency('', required: false)).partial_dependency(compile_args: true, includes: true),
    opencl_dep,
    level0_dep.partial_dependency(compile_args: true, includes: true),
  ],
  build_rpath: '$ORIGIN', install_rpath: '$ORIGIN',
  install: true, install_dir: 'lib' / meson.project_name())

_hpcrun_libs += shared_library('hpcrun_unwind', config_h,
  'unwind/common/libunwind-pvt.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [libunwind_dep],
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_module('hpcrun_fake_audit', config_h,
  'audit/fake-auditor.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [dl_dep, threads_dep],
  build_rpath: '$ORIGIN', install_rpath: '$ORIGIN',
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_library('hpcrun_private_ns', config_h,
  'audit/private-ns.c',
  'audit/exports.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [dl_dep, threads_dep],
  build_rpath: '$ORIGIN', install_rpath: '$ORIGIN',
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_library('hpcrun_audit', config_h,
  'audit/auditor.c',
  'audit/private-ns.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [dl_dep, threads_dep],
  build_rpath: '$ORIGIN', install_rpath: '$ORIGIN',
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_library('hpcrun_dlmopen', config_h,
  'dlmopen/dlmopen.c',
  gnu_symbol_visibility: 'hidden',
  dependencies: [dl_dep],
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())

_hpcrun_libs += shared_module('hpcrun_ga', config_h,
  'foil/ga-preload.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [dl_dep],
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_module('hpcrun_gprof', config_h,
  'foil/gprof-preload.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_module('hpcrun_io', config_h,
  'foil/io-preload.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  dependencies: [dl_dep],
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_module('hpcrun_memleak', config_h,
  'foil/memleak-preload.c',
  gnu_symbol_visibility: 'hidden',
  c_args: _my_cpp_defines,
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())
_hpcrun_libs += shared_module('hpcrun_pthread', config_h,
  'foil/pthread-blame-preload.c',
  gnu_symbol_visibility: 'hidden',
  dependencies: [dl_dep],
  c_args: _my_cpp_defines,
  link_language: 'c',
  install: true, install_dir: 'lib' / meson.project_name())


# Configure and install the hpcrun launcher
hpcrun = executable('hpcrun', config_h, 'launch.cpp',
  install: true)

# Make sure the hpcrun launcher is available in devenv
_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
_devenv.set('HPCRUN_FNBOUNDS_CMD', hpcfnbounds.full_path())
meson.add_devenv(_devenv)

# Build and install the client API
install_headers(
  'hpctoolkit.h',
)
hpctoolkit_client_lib = library('hpctoolkit',
  'hpctoolkit.c',
  c_args: _my_cpp_defines,
  install: true, install_dir: 'lib' / meson.project_name())

# Settings needed for dependencies
hpcrun_test_depends = [hpcfnbounds] + _hpcrun_libs
test_depends += hpcrun_test_depends
_env = {'HPCRUN_FNBOUNDS_CMD': hpcfnbounds.full_path()}
hpcrun_test_env = environment(_env)
foreach k,v : _env
  test_env.set(k, v)
endforeach
