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
# Copyright ((c)) 2002-2020, Rice University
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

# TODO: Front-end scripts (hpcsummary + hpclog)
# TODO: MPI wrapper

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
    'gpu/gpu-splay-allocator.c',
    'gpu/gpu-stream-id-map.c',
    'gpu/gpu-trace.c',
    'gpu/gpu-trace-channel.c',
    'gpu/gpu-trace-item.c',

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
    'hpcrun_dlfns.c',
    'custom-init-dynamic.c',
  ),
  libelf_dep, libmonitor_dep, gotcha_dep,
)
_s_srcset.add(
  files(
    'fnbounds/fnbounds_static.c',
    'custom-init-static.c',
  ),
  declare_dependency(compile_args: '-DHPCRUN_STATIC_LINK'),
  libelf_dep.partial_dependency(compile_args: true, includes: true),
  libmonitor_static_dep,
)

if host_machine.system() == 'linux'
  _d_srcset.add(files('os/linux/dylib.c'))
endif

_config.set('BLUE_GENE', false)  # TODO: Change out for a Blue Gene option
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
_config.set('LINUX_4_3', true)  # TODO: Swap out for a linux >=4.3 test/option
_perf_srcset.add(when: 'LINUX_4_3',
  if_true: files('sample-sources/perf/kernel_blocking.c'),
  if_false: files('sample-sources/perf/kernel_blocking_stub.c'))
_perfmon_true = [
  files('sample-sources/perf/perfmon-util.c'),
  declare_dependency(compile_args: ['-DENABLE_PERFMON']),
]
_perfmon_false = files('sample-sources/perf/perfmon-util-dummy.c')
_perf_d_srcset = sourceset.source_set()
_perf_d_srcset.add(when: perfmon_dep, if_true: _perfmon_true, if_false: _perfmon_false)
_perf_s_srcset = sourceset.source_set()
_perf_s_srcset.add(when: perfmon_static_dep, if_true: _perfmon_true, if_false: _perfmon_false)
_config.set('PERF_AVAILABLE', true)  # TODO: Change to perf non-paranoid test/option
_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_srcset)
_d_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_d_srcset)
_s_srcset.add_all(when: 'PERF_AVAILABLE', if_true: _perf_s_srcset)

_papi_srcset = sourceset.source_set()
_config.set('PAPI_C', true)  # TODO: Change this to match --enable-papi-c
_papi_srcset.add(when: 'PAPI_C',
  if_true: files(
      'sample-sources/papi-c.c',
      'sample-sources/papi-c-extended-info.c',
  ), if_false: files(
    'sample-sources/papi.c',
  ))
_config.set('PAPI_C_CUPTI', false)  # TODO: --enable-papi-c-cupti
_papi_srcset.add(when: ['PAPI_C', 'PAPI_C_CUPTI'],
  if_true: [
    files('sample-sources/papi-c-cupti.c'),
    declare_dependency(compile_args: '-DHPCRUN_SS_PAPI_C_CUPTI'),
  ])
_d_srcset.add_all(when: papi_dep, if_true: _papi_srcset)
_s_srcset.add_all(when: papi_static_dep, if_true: _papi_srcset)

_srcset.add(when: cuda_dep, if_true: [
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

_d_srcset.add(when: rocm_headers_dep, if_true: [
    files(
      'sample-sources/amd.c',
      'gpu/amd/roctracer-activity-translate.c',
      'gpu/amd/roctracer-api.c',
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

# TODO: Adjust this to vaugely match the unwinder logic in the configure.ac
if true  # UNW_X86
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
  ), declare_dependency(include_directories: include_directories('unwind/x86-family')))
  _d_srcset.add(xed_dep)
  _s_srcset.add(xed_static_dep)
elif false  # UNW_PPC64
  _srcset.add(files(
    'unwind/ppc64/ppc64-unwind.c',
    'unwind/ppc64/ppc64-unwind-interval.c',
    'unwind/common/default_validation_summary.c',
  ), declare_dependency(include_directories: include_directories('unwind/ppc64')))
elif false  # UNW_LIBUNW
  _srcset.add(files(
    'unwind/generic-libunwind/libunw-unwind.c',
    'unwind/common/default_validation_summary.c',
  ), declare_dependency(include_directories: include_directories('unwind/generic-libunwind')))
endif  # NOTE: Dead UNW_MIPS files

_libunwind_srcset = sourceset.source_set()
_libunwind_srcset.add(declare_dependency(compile_args: '-DUSE_LIBUNWIND'))
_d_srcset.add_all(when: libunwind_dep, if_true: _libunwind_srcset)
_s_srcset.add_all(when: libunwind_static_dep, if_true: _libunwind_srcset)

_srcset.add(files('utilities/last_func.c'))

# hpcrun requires an hpcfnbounds in order to function. Make sure we have one.
_has_hpcfnbounds = not is_disabler(hpcfnbounds_exe) or not is_disabler(hpcfnbounds2_exe)

_d_srcset.add_all(_srcset)
_d_srcs = _d_srcset.apply(_config)
_d_ddep = []
foreach dep : _d_srcs.dependencies()
  if not dep.found()
    _d_ddep = [disabler()]
  endif
endforeach

libhpcrun_lib = shared_library('hpcrun', _d_srcs.sources(),
  dependencies: _d_srcs.dependencies() + _d_ddep
    + [cc.find_library('dl'), cc.find_library('rt'), dependency('threads')],
  link_with: [prof_lean_lib, support_lean_lib],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('hpcrun', not is_disabler(libhpcrun_lib) and _has_hpcfnbounds, section: 'Applications', bool_yn: true)
all_applibs += libhpcrun_lib

_s_srcset.add_all(_srcset)
_s_srcs = _s_srcset.apply(_config)
_s_ddep = []
foreach dep : _s_srcs.dependencies()
  if not dep.found()
    _s_ddep = [disabler()]
  endif
endforeach

libhpcrun_static_lib = static_library('hpcrun_static', _s_srcs.sources(),
  dependencies: _s_srcs.dependencies() + _s_ddep
    + [cc.find_library('dl'), dependency('threads')],
  link_with: [prof_lean_static_lib, support_lean_static_lib],
  install: true, install_dir: get_option('libdir') / meson.project_name(),
)
summary('hpclink', not is_disabler(libhpcrun_static_lib) and _has_hpcfnbounds, section: 'Applications', bool_yn: true)
all_applibs += libhpcrun_static_lib

# Check that the static case is actually linkable by compiling a small
# hello-world style application with it linked in.
executable('hpcrun_static_test', files('hello-world.c'),
  link_with: [libhpcrun_static_lib])

# If the front-end bits have been requested, use them.
# TODO: Swap out for a !--enable-backend option
if true
  all_apps += configure_file(
    input: files('scripts/hpclog'), output: '@PLAINNAME@', copy: true,
    install: true, install_dir: get_option('libexecdir') / 'hpctoolkit',
    install_mode: 'rwxr-xr-x')
  all_apps += configure_file(
    input: files('scripts/hpcsummary.in'), output: '@BASENAME@',
    configuration: {'PACKAGE_VERSION': meson.project_version()},
    install: true, install_dir: get_option('libexecdir') / 'hpctoolkit',
    install_mode: 'rwxr-xr-x')
  install_headers(files('hpctoolkit.h'))
endif
