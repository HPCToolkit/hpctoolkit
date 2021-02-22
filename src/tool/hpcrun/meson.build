# * BeginRiceCopyright *****************************************************
#
# $HeadURL$
# $Id$
#
# --------------------------------------------------------------------------
# Part of HPCToolkit (hpctoolkit.org)
#
# Information about sources of support for research and development of
# HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
# --------------------------------------------------------------------------
#
# Copyright ((c)) 2002-2021, Rice University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Rice University (RICE) nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# This software is provided by RICE and contributors "as is" and any
# express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular
# purpose are disclaimed. In no event shall RICE or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or
# business interruption) however caused and on any theory of liability,
# whether in contract, strict liability, or tort (including negligence
# or otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#
# ******************************************************* EndRiceCopyright *

_d_srcset = sourceset.source_set()
_s_srcset = sourceset.source_set()
_srcset = sourceset.source_set()
_config = configuration_data()

_srcset.add([
  files(
    'utilities/first_func.c',

    'main.c',
    'disabled.c',
    'closure-registry.c',
    'cct_insert_backtrace.c',
    'cct_backtrace_finalize.c',
    'env.c',
    'epoch.c',
    'files.c',
    'handling_sample.c',
    'hpcrun-initializers.c',
    'hpcrun_options.c',
    'hpcrun_stats.c',
    'loadmap.c',
    'metrics.c',
    'name.c',
    'rank.c',
    'sample_event.c',
    'sample_prob.c',
    'sample_sources_all.c',
    'sample-sources/blame-shift/blame-shift.c',
    'sample-sources/blame-shift/blame-map.c',
    'sample-sources/blame-shift/directed.c',
    'sample-sources/blame-shift/undirected.c',
    'sample-sources/omp-mutex.c',
    'sample-sources/omp-idle.c',
    'sample-sources/common.c',
    'sample-sources/display.c',
    'sample-sources/ga.c',
    'sample-sources/io.c',
    'sample-sources/itimer.c',
    'sample-sources/idle.c',
    'sample-sources/memleak.c',
    'sample-sources/pthread-blame.c',
    'sample-sources/none.c',
    'sample-sources/retcnt.c',
    'sample-sources/sync.c',
    'sample_sources_registered.c',
    'sample-sources/sample-filters.c',
    'segv_handler.c',
    'start-stop.c',
    'term_handler.c',
    'thread_data.c',
    'thread_use.c',
    'thread_finalize.c',
    'control-knob.c',
    'device-finalizers.c',
    'device-initializers.c',
    'module-ignore-map.c',
    'threadmgr.c',
    'trace.c',
    'weak.c',
    'write_data.c',

    'cct/cct_bundle.c',
    'cct/cct_ctxt.c',
    'cct/cct.c',
    'cct/cct-node-vector.c',

    'cct2metrics.c',

    'lush/lush-backtrace.c',
    'lush/lush.c',
    'lush/lush-pthread.c',
    'lush/lush-support-rt.c',

    'lush/lushi-cb.c',

    'fnbounds/fnbounds_common.c',

    'memory/mem.c',
    'memory/mmap.c',

    'messages/debug-flag.c',
    'messages/messages-sync.c',
    'messages/messages-async.c',
    'messages/fmt.c',
    'hpcrun-placeholders.c',
    'gpu/gpu-activity.c',
    'gpu/gpu-activity-channel.c',
    'gpu/gpu-activity-process.c',
    'gpu/gpu-application-thread-api.c',
    'gpu/gpu-channel-item-allocator.c',
    'gpu/gpu-context-id-map.c',
    'gpu/gpu-correlation.c',
    'gpu/gpu-correlation-channel.c',
    'gpu/gpu-correlation-channel-set.c',
    'gpu/gpu-correlation-id.c',
    'gpu/gpu-correlation-id-map.c',
    'gpu/gpu-event-id-map.c',
    'gpu/gpu-function-id-map.c',
    'gpu/gpu-host-correlation-map.c',
    'gpu/gpu-metrics.c',
    'gpu/gpu-monitoring.c',
    'gpu/gpu-monitoring-thread-api.c',
    'gpu/gpu-op-placeholders.c',
    'gpu/gpu-operation-item.c',
    'gpu/gpu-operation-item-process.c',
    'gpu/gpu-operation-channel.c',
    'gpu/gpu-operation-channel-set.c',
    'gpu/gpu-operation-multiplexer.c',
    'gpu/gpu-splay-allocator.c',
    'gpu/gpu-stream-id-map.c',
    'gpu/gpu-trace.c',
    'gpu/gpu-trace-channel.c',
    'gpu/gpu-trace-item.c',
    'gpu/gpu-trace-channel-set.c',
    'gpu/gpu-trace-demultiplexer.c',

    'ompt/ompt-callstack.c',
    'ompt/ompt-defer.c',
    'ompt/ompt-device.c',
    'ompt/ompt-defer-write.c',
    'ompt/ompt-interface.c',
    'ompt/ompt-queues.c',
    'ompt/ompt-region.c',
    'ompt/ompt-region-debug.c',
    'ompt/ompt-placeholders.c',
    'ompt/ompt-device-map.c',
    'ompt/ompt-task.c',
    'ompt/ompt-thread.c',

    'extern-real/dl-iterate.c',
    'extern-real/mmap.c',

    'syscalls/poll.c',
    'syscalls/ppoll.c',
    'syscalls/select.c',

    'utilities/executable-path.c',
    'utilities/hpcrun-nanotime.c',
    'utilities/ip-normalized.c',
    'utilities/line_wrapping.c',
    'utilities/timer.c',
    'utilities/tokenize.c',
    'utilities/unlink.c',

    host_machine.cpu_family() == 'ppc64' ? 'trampoline/common/trampoline_eager.c'
                                         : 'trampoline/common/trampoline_lazy.c',

    'unwind/common/backtrace.c',
    'unwind/common/unw-throw.c',
    'unwind/common/binarytree_uwi.c',
    'unwind/common/interval_t.c',
    'unwind/common/libunw_intervals.c',
    'unwind/common/stack_troll.c',
    'unwind/common/uw_hash.c',
    'unwind/common/uw_recipe_map.c',
  ),
  declare_dependency(
    include_directories: main_includes + [include_directories(
      '..', 'messages', 'fnbounds', 'memory', 'os/linux', 'cct', 'ompt',
      'unwind/common', 'utilities',
    )],
    compile_args: [
      '-D_GNU_SOURCE', '-DINLINE_FN=1', '-DLOCAL_BUILD=1',
      '-D__HIP_PLATFORM_HCC__=1',
    ],
  ),
])

_d_srcset.add(
  files(
    'fnbounds/fnbounds_client.c',
    'fnbounds/fnbounds_dynamic.c',
    'monitor-exts/openmp.c',
    'custom-init-dynamic.c',
  ),
  dependency('libelf', required: get_option('hpcrun')),
  dependency('libmonitor', required: get_option('hpcrun')),
)
_s_srcset.add(
  files(
    'fnbounds/fnbounds_static.c',
    'custom-init-static.c',
  ),
  declare_dependency(compile_args: '-DHPCRUN_STATIC_LINK'),
  dependency('libelf', required: get_option('hpclink'), static: true)
    .partial_dependency(compile_args: true, includes: true),
  dependency('libmonitor', required: get_option('hpclink'), static: true)
    .partial_dependency(compile_args: true, includes: true),
)

if host_machine.system() == 'linux'
  _d_srcset.add(files('os/linux/dylib.c'))
endif

_config.set('LUSH_PTHREADS', get_option('lush-pthreads'))
_srcset.add(when: 'LUSH_PTHREADS', if_true: declare_dependency(
  compile_args: ['-DLUSH_PTHREADS']))

_config.set('BLUE_GENE', false)  # TODO: Remove, we no longer support this
_srcset.add(when: 'BLUE_GENE', if_true: declare_dependency(
  compile_args: ['-DUSE_HW_THREAD_ID', '-DNONZERO_THRESHOLD']))

_perf_srcset = sourceset.source_set()
_perf_srcset.add(files(
  'sample-sources/perf/event_custom.c',
  'sample-sources/perf/linux_perf.c',
  'sample-sources/perf/perf_event_open.c',
  'sample-sources/perf/perf-util.c',
  'sample-sources/perf/perf_mmap.c',
  'sample-sources/perf/perf_skid.c',
), declare_dependency(compile_args: ['-DHPCRUN_SS_LINUX_PERF']))
_config.set('LINUX_4_3', not get_option('kernel-blocking').disabled() and cc.compiles('''
    #include <linux/perf_event.h>
    #include <linux/version.h>
    static struct perf_event_attr attr;
    int main() {
      #if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
        attr.context_switch = 1;
      #else
        #error Kernel is too old!
      #endif
    }
  ''', name: 'kernel blocking perf event support'))
if get_option('kernel-blocking').enabled() and not _config.get('LINUX_4_3')
  warning('Kernel blocked event support does not appear to be available, but enabling as requested.')
  _config.set('LINUX_4_3', true)
endif
_perf_srcset.add(when: 'LINUX_4_3',
  if_true: files('sample-sources/perf/kernel_blocking.c'),
  if_false: files('sample-sources/perf/kernel_blocking_stub.c'))
_perfmon_true = [
  files('sample-sources/perf/perfmon-util.c'),
  declare_dependency(compile_args: ['-DENABLE_PERFMON']),
]
_perfmon_false = files('sample-sources/perf/perfmon-util-dummy.c')
_perf_d_srcset = sourceset.source_set()
_perf_d_srcset.add(when: dependency('perfmon', required: false),
                   if_true: _perfmon_true, if_false: _perfmon_false)
_perf_s_srcset = sourceset.source_set()
_perf_s_srcset.add(when: dependency('perfmon', required: false, static: true),
                   if_true: _perfmon_true, if_false: _perfmon_false)
_config.set('PERF_AVAILABLE', not get_option('perf-events').disabled() and cc.compiles('''
    #include <linux/perf_event.h>
    static struct perf_event_attr attr;
    int main() {
      attr.sample_id_all = 1;
    }
  ''', name: 'kernel perf event support'))
if get_option('perf-events').enabled() and not _config.get('PERF_AVAILABLE')
  warning('Kernel perf event suport does not appear to be available, but enabling as requested.')
  _config.set('PERF_AVAILABLE', true)
endif
_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_srcset)
_d_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_d_srcset)
_s_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_s_srcset)

_papi_srcset = sourceset.source_set()
_papi_dep = dependency('papi', required: false)
_config.set('PAPI_C', not get_option('papi-c').disabled()
  and cc.has_header_symbol('papi.h', 'PAPI_get_eventset_component',
                           dependencies: _papi_dep))
if get_option('papi-c').enabled()
  assert(_config.get('PAPI_C'), 'papi-c enabled but not available!')
endif
_papi_srcset.add(when: 'PAPI_C',
  if_true: files(
    'sample-sources/papi-c.c',
    'sample-sources/papi-c-extended-info.c',
  ), if_false: files(
    'sample-sources/papi.c',
  ))
_config.set('PAPI_C_CUPTI', not get_option('papi-c-cupti').disabled()
  and cc.has_function('CUDA_init_component',
                      dependencies: _papi_dep))
if get_option('papi-c-cupti').enabled()
  assert(_config.get('PAPI_C_CUPTI'), 'papi-c-cupti enabled but not available!')
endif
_papi_srcset.add(when: ['PAPI_C', 'PAPI_C_CUPTI'],
  if_true: [
    files('sample-sources/papi-c-cupti.c'),
    declare_dependency(compile_args: '-DHPCRUN_SS_PAPI_C_CUPTI'),
  ])
_d_srcset.add_all(when: _papi_dep, if_true: _papi_srcset)
_s_srcset.add_all(when: dependency('papi', required: false, static: true), if_true: _papi_srcset)

_d_srcset.add(when: dependency('CUDA', required: get_option('cuda-monitoring')), if_true: [
    files(
      'sample-sources/nvidia.c',
      'gpu/nvidia/cubin-hash-map.c',
      'gpu/nvidia/cubin-id-map.c',
      'gpu/nvidia/cubin-symbols.c',
      'gpu/nvidia/cuda-api.c',
      'gpu/nvidia/cuda-device-map.c',
      'gpu/nvidia/cupti-activity-translate.c',
      'gpu/nvidia/cupti-analysis.c',
      'gpu/nvidia/cupti-api.c',
      'gpu/nvidia/cupti-gpu-api.c',
    ),
    declare_dependency(compile_args: ['-DENABLE_CUDA', '-DHPCRUN_SS_NVIDIA'])
  ])

_d_srcset.add(when: dependency('opencl-headers', required: false), if_true: [
    files(
      'sample-sources/opencl.c',
      'gpu/opencl/opencl-api.c',
      'gpu/opencl/opencl-memory-manager.c',
      'gpu/opencl/opencl-activity-translate.c',
      'gpu/opencl/opencl-h2d-map.c',
      'gpu/opencl/opencl-queue-map.c',
      'gpu/opencl/opencl-context-map.c',
    ),
    declare_dependency(compile_args: ['-DENABLE_OPENCL', '-DHPCRUN_SS_OPENCL'])
  ])

_d_srcset.add(when: dependency('gtpin', required: false), if_true: [
    files(
      'gpu/instrumentation/kernel-data-map.c',
      'gpu/instrumentation/gtpin-instrumentation.c',
      'gpu/instrumentation/gtpin-correlation-id-map.c',
    ),
    declare_dependency(compile_args: ['-DENABLE_GTPIN', '-DHPCRUN_SS_GTPIN', '-DGTPIN_LIBDIR=/nonexistant'])
  ])

_d_srcset.add(when: dependency('level0-headers', required: false), if_true: [
    files(
      'sample-sources/level0.c',
      'gpu/level0/level0-api.c',
      'gpu/level0/level0-command-list-context-map.c',
      'gpu/level0/level0-command-list-map.c',
      'gpu/level0/level0-command-process.c',
      'gpu/level0/level0-data-node.c',
      'gpu/level0/level0-event-map.c',
      'gpu/level0/level0-handle-map.c',
    ),
    declare_dependency(compile_args: ['-DENABLE_LEVEL0', '-DHPCRUN_SS_LEVEL0'])
  ])

_d_srcset.add(when: dependency('rocm-headers', required: false), if_true: [
    files(
      'sample-sources/amd.c',
      'gpu/amd/roctracer-activity-translate.c',
      'gpu/amd/roctracer-api.c',
      'gpu/amd/rocm-debug-api.c',
      'gpu/amd/rocm-binary-processing.c',
    ),
    declare_dependency(compile_args: ['-DENABLE_ROCM', '-DHPCRUN_SS_AMD'])
  ])

if host_machine.cpu_family() == 'mips64'
  _srcset.add(files(
    'unwind/common/default_validation_summary.c',
  ), declare_dependency(include_directories: include_directories('unwind/mips')))
elif host_machine.cpu_family() == 'ppc64'
  _srcset.add(files(
    'trampoline/ppc64/ppc64-tramp.s',
    'utilities/arch/ppc64/ppc64-context-pc.c',
  ), declare_dependency(include_directories: include_directories('utilities/arch/ppc64')))
elif host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
  _srcset.add(files(
    'trampoline/x86-family/x86-tramp.S',
    'utilities/arch/x86-family/x86-context-pc.c',
  ), declare_dependency(include_directories: include_directories('utilities/arch/x86-family')))
elif host_machine.cpu_family() == 'ia64'
  _srcset.add(files(
    'trampoline/ia64/ia64-tramp.s',
    'utilities/arch/ia64/ia64-context-pc.c',
  ), declare_dependency(include_directories: include_directories('utilities/arch/ia64')))
elif host_machine.cpu_family() == 'aarch64'
  _srcset.add(files(
    'trampoline/aarch64/aarch64-tramp.c',
    'utilities/arch/libunwind/libunwind-context-pc.c',
  ), declare_dependency(include_directories: include_directories('utilities/arch/aarch64')))
endif

if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
  _srcset.add(files(
    'unwind/x86-family/x86-all.c',
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
  ), declare_dependency(
    include_directories: include_directories('unwind/x86-family'),
    compile_args: ['-DHPCRUN_HAVE_CUSTOM_UNWINDER=1'],
  ))
  _d_srcset.add(dependency('xed', required: get_option('hpcrun')))
  _s_srcset.add(dependency('xed', required: get_option('hpcrun'), static: true))
elif host_machine.cpu_family() == 'ppc64'
  _srcset.add(files(
    'unwind/ppc64/ppc64-unwind.c',
    'unwind/ppc64/ppc64-unwind-interval.c',
    'unwind/common/default_validation_summary.c',
  ), declare_dependency(
    include_directories: include_directories('unwind/ppc64'),
    compile_args: ['-DHPCRUN_HAVE_CUSTOM_UNWINDER=1'],
  ))
else  # NOTE: Dead UNW_MIPS case and files
  _srcset.add(files(
    'unwind/generic-libunwind/libunw-unwind.c',
    'unwind/common/default_validation_summary.c',
  ), declare_dependency(
    include_directories: include_directories('unwind/generic-libunwind'),
  ))
endif

_libunwind_srcset = sourceset.source_set()
_libunwind_srcset.add(declare_dependency(compile_args: '-DUSE_LIBUNWIND'))
_d_srcset.add_all(when: dependency('internalized libunwind', required: false), if_true: _libunwind_srcset)
_s_srcset.add_all(when: dependency('internalized libunwind', required: false, static: true), if_true: _libunwind_srcset)

_srcset.add(files('utilities/last_func.c'))

# Install some bits useful for writing applications or analyzing data
all_scripts += configure_file(
  input: files('scripts/hpclog'), output: '@PLAINNAME@', copy: true,
  install: true, install_dir: get_option('libexecdir') / meson.project_name(),
  install_mode: 'rwxr-xr-x')
# For the man page generation
hpcsummary_script = files('scripts/hpcsummary.in')
all_scripts += configure_file(
  input: hpcsummary_script, output: '@BASENAME@',
  configuration: {'PACKAGE_VERSION': meson.project_version()},
  install: true, install_dir: get_option('libexecdir') / meson.project_name(),
  install_mode: 'rwxr-xr-x')
install_headers(files('hpctoolkit.h'))
libhpctoolkit_lib = both_libraries('hpctoolkit',
  files('hpctoolkit.c'),
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)

# hpcrun/link require an hpcfnbounds in order to function. Make sure we have one.
if is_disabler(hpcfnbounds_exe) and is_disabler(hpcfnbounds2_exe)
  summary('hpcrun', false, section: 'Applications', bool_yn: true)
  summary('hpclink', false, section: 'Applications', bool_yn: true)
  subdir_done()
endif

##
## Dynamic case (hpcrun)
##

_d_srcset.add_all(_srcset)
_d_srcs = _d_srcset.apply(_config)
_d_partial_deps = []
_d_ddep = []
foreach dep : _d_srcs.dependencies()
  if not dep.found()
    _d_ddep = [disabler()]
  endif
  _d_partial_deps += dep.partial_dependency(compile_args: true, includes: true, sources: true)
endforeach

libhpcrun_lib = shared_library('hpcrun', _d_srcs.sources(),
  dependencies: _d_srcs.dependencies() + _d_ddep
    + [cc.find_library('dl'), cc.find_library('rt'), dependency('threads')],
  link_with: [prof_lean_lib, support_lean_lib],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
libhpcrun_fake_audit_lib = shared_module('hpcrun_fake_audit',
  files('audit/fake-auditor.c'),
  dependencies: _d_partial_deps + _d_ddep + [cc.find_library('dl')],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
libhpcrun_audit_lib = shared_module('hpcrun_audit',
  files('audit/auditor.c'),
  dependencies: _d_partial_deps + _d_ddep + [cc.find_library('dl')],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
libhpcrun_dlmopen_lib = shared_module('hpcrun_dlmopen',
  files('dlmopen/dlmopen.c'),
  dependencies: _d_partial_deps + _d_ddep + [cc.find_library('dl')],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
if get_option('hpcrun').enabled()
  assert(not is_disabler(libhpcrun_lib)
         and not is_disabler(libhpcrun_fake_audit_lib)
         and not is_disabler(libhpcrun_audit_lib)
         and not is_disabler(libhpcrun_dlmopen_lib),
         'hpcrun required but disabled')
endif
summary('hpcrun', not is_disabler(libhpcrun_lib)
                  and not is_disabler(libhpcrun_fake_audit_lib)
                  and not is_disabler(libhpcrun_audit_lib)
                  and not is_disabler(libhpcrun_dlmopen_lib),
        section: 'Applications', bool_yn: true)

if not is_disabler(libhpcrun_lib)
  # Set for bin/meson.build
  hpcrun_script = files('scripts/hpcrun.meson.in')
  configure_file(input: hpcrun_script, output: 'hpcrun',
    configuration: launch_config,
    install: true, install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x')
endif

libhpcrun_ga_lib = shared_module('hpcrun_ga',
  files('sample-sources/ga-overrides.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('Global Arrays (hpcrun)', not is_disabler(libhpcrun_ga_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_io_lib = shared_module('hpcrun_io',
  files('sample-sources/io-over.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('I/O Counters (hpcrun)', not is_disabler(libhpcrun_io_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_memleak_lib = shared_module('hpcrun_memleak',
  files('sample-sources/memleak-overrides.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('Memory Leak Detection (hpcrun)', not is_disabler(libhpcrun_memleak_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_pthread_lib = shared_module('hpcrun_pthread',
  files('sample-sources/pthread-blame-overrides.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('PThread Blame Shifting (hpcrun)', not is_disabler(libhpcrun_pthread_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_gprof_lib = shared_module('hpcrun_gprof',
  files('gprof.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('GProf Disabling Wrapper (hpcrun)', not is_disabler(libhpcrun_gprof_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_opencl_lib = shared_module('hpcrun_opencl',
  files('gpu/opencl/opencl-api-wrappers.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep
     + [dependency('opencl-headers', required: false, disabler: true)],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('OpenCL Wrappers (hpcrun)', not is_disabler(libhpcrun_opencl_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_level0_lib = shared_module('hpcrun_level0',
  files('gpu/level0/level0-api-wrappers.c'),
  dependencies: _d_srcs.dependencies() + _d_ddep
    + [dependency('level0-headers', required: false, disabler: true)],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('Level0 Wrappers (hpcrun)', not is_disabler(libhpcrun_level0_lib),
        section: 'Plugins', bool_yn: true)


# TODO: libhpcrun_mpi
# TODO: LUSH agents

##
## Static case (hpclink)
##

_s_srcset.add_all(_srcset)
_s_srcs = _s_srcset.apply(_config)
_s_deps = _s_srcs.dependencies()
# These are needed to since the pkgconfig module doesn't recursively search deps for static libs
_s_deps += [
  dependency('libelf', required: get_option('hpclink'), disabler: true, static: true),
  dependency('liblzma', required: get_option('hpclink'), disabler: true, static: true),
  dependency('mbedtls', required: get_option('hpclink'), disabler: true, static: true),
]
# This is used to ensure we only generate bits if libmonitor_wrap is available
_s_deps += [
  dependency('libmonitor', required: get_option('hpclink'), disabler: true, static: true)
    .partial_dependency()
]
# These are required by hpcrun_static or libmonitor
_s_deps += [cc.find_library('dl'), cc.find_library('rt'), dependency('threads')]

_s_partial_ddeps = []
_s_full_ddeps = []
foreach dep : _s_deps
  if not dep.found()
    _s_partial_ddeps = [disabler()]
    _s_full_ddeps = [disabler()]
    break
  else
    _s_partial_ddeps += dep.partial_dependency(compile_args: true, includes: true, sources: true)
    _s_full_ddeps += dep
  endif
endforeach

libhpcrun_static_lib = static_library('hpcrun_static', _s_srcs.sources(),
  dependencies: _s_partial_ddeps,
  link_with: [prof_lean_static_lib, support_lean_static_lib],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
libhpcrun_wrap_lib = static_library('hpcrun_wrap',
  files('monitor-exts/openmp.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
libhpcrun_deps_lib = static_library('hpcrun_deps',
  dependencies: _s_full_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('hpclink', not is_disabler(libhpcrun_static_lib)
  and not is_disabler(libhpcrun_wrap_lib)
  and not is_disabler(libhpcrun_deps_lib),
  section: 'Applications', bool_yn: true)
if get_option('hpclink').enabled()
  assert(not is_disabler(libhpcrun_static_lib)
         and not is_disabler(libhpcrun_wrap_lib)
         and not is_disabler(libhpcrun_deps_lib),
         'hpclink required but disabled')
endif

if not is_disabler(libhpcrun_static_lib) and not is_disabler(libhpcrun_wrap_lib)
  pkgconfig.generate(libhpcrun_static_lib)
  pkgconfig.generate(libhpcrun_wrap_lib)
  pkgconfig.generate(libhpcrun_deps_lib)

  # The hpclink script needs an explicit pkgconfig for libmonitor as well.
  # Since libmonitor doesn't generate one, we handle that part.
  pkgconfig.generate(
    name: 'monitor_wrap', description: 'libmonitor: wrap library',
    libraries: ex_libs['libmonitor'],
    install_dir: get_option('libdir') / meson.project_name() / 'pkgconfig',
  )

  # Set for bin/meson.build
  hpclink_script = files('scripts/hpclink.meson.in')
  configure_file(input: hpclink_script, output: 'hpclink',
    configuration: launch_config,
    install: true, install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x')
endif

libhpcrun_ga_wrap_lib = static_library('hpcrun_ga_wrap',
  files('sample-sources/ga-overrides.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
_vars = [
  'wrapnames=pnga_create:pnga_brdcst:pnga_gop:pnga_sync:pnga_zero'
  + ':pnga_get:pnga_put:pnga_acc:pnga_nbget:pnga_nbput:pnga_nbacc',
  'undefnames=pnga_put',
]
pkgconfig.generate(libhpcrun_ga_wrap_lib,
  variables: _vars, uninstalled_variables: _vars)
summary('Global Arrays (hpclink)', not is_disabler(libhpcrun_ga_wrap_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_io_wrap_lib = static_library('hpcrun_io_wrap',
  files('sample-sources/io-over.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
_vars = [
  'wrapnames=read:write:fread:fwrite',
  'undefnames=fwrite',
]
pkgconfig.generate(libhpcrun_io_wrap_lib,
  variables: _vars, uninstalled_variables: _vars)
summary('I/O Counters (hpclink)', not is_disabler(libhpcrun_io_wrap_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_memleak_wrap_lib = static_library('hpcrun_memleak_wrap',
  files('sample-sources/memleak-overrides.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
_vars = [
  'wrapnames=malloc:calloc:free:realloc:memalign:posix_memalign:valloc',
  'undefnames=malloc',
]
pkgconfig.generate(libhpcrun_memleak_wrap_lib,
  variables: _vars, uninstalled_variables: _vars)
summary('Memory Leak Detection (hpclink)', not is_disabler(libhpcrun_memleak_wrap_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_pthread_wrap_lib = static_library('hpcrun_pthread_wrap',
  files('sample-sources/pthread-blame-overrides.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
_vars = [
  'wrapnames=pthread_cond_timedwait:pthread_cond_wait:pthread_cond_broadcast'
  + ':pthread_cond_signal:pthread_mutex_lock:pthread_mutex_unlock'
  + ':pthread_mutex_timedlock:pthread_spin_lock:pthread_spin_unlock:sched_yield'
  + ':sem_wait:sem_post:sem_timedwait',
]
pkgconfig.generate(libhpcrun_pthread_wrap_lib,
  variables: _vars, uninstalled_variables: _vars)
summary('PThread Blame Shifting (hpclink)', not is_disabler(libhpcrun_pthread_wrap_lib),
        section: 'Plugins', bool_yn: true)

libhpcrun_gprof_wrap_lib = static_library('hpcrun_gprof_wrap',
  files('gprof.c'),
  dependencies: _s_partial_ddeps,
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
_vars = [
  'wrapnames=__monstartup:_mcleanup:mcount:_mcount',
  'undefnames=__monstartup'
]
pkgconfig.generate(libhpcrun_gprof_wrap_lib,
  variables: _vars, uninstalled_variables: _vars)
summary('GProf Disabling Wrapper (hpclink)', not is_disabler(libhpcrun_gprof_wrap_lib),
        section: 'Plugins', bool_yn: true)
