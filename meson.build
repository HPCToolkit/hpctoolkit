project(
  'hpctoolkit',
  'c',
  'cpp',
  version: '2023.08.99-next',
  meson_version: '>=1.1.0',
  license: 'BSD-3-Clause',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu11',
    'cpp_std=gnu++17',
    'cuda_std=c++14',
    'b_ndebug=if-release',
  ],
)

fs = import('fs')
pymod = import('python')
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')


# Find Python. This is a hard requirement since we use Python for the regression test suite as well.
python = pymod.find_installation('python3')
assert(
  python.language_version().version_compare('>=3.8'),
  'Need Python >=3.8, got @0@'.format(python.language_version()),
)
python_dep = python.dependency(required: get_option('python'))


# Collect all the dependencies we require for HPCToolkit
boost_dep = dependency('boost', 'Boost', modules: ['graph'], version: '>=1.70.0')
bzip_dep = dependency('bzip2', 'BZip2', version: '>=1.0.0')
tbb_dep = dependency('tbb', 'TBB', components: ['tbb'])
tbb_malloc_dep = dependency('tbbmalloc', 'TBB', components: ['tbb', 'tbbmalloc'])
libelf_dep = dependency('libelf', version: '>=0.186')
libdw_dep = dependency('libdw', version: '>=0.186')
libunwind_dep = dependency('libunwind', version: '>=1.6.2')
if meson.version().version_compare('>=1.4.0')
  xerces_dep = dependency('xerces-c', version: '>=3.2.3')
else
  # Prior Meson versions don't provide versions for `method = cmake` wraps
  xerces_dep = dependency('xerces-c')
  if xerces_dep.type_name() != 'internal' and not xerces_dep.version().version_compare('>=3.2.3')
    xerces_dep = dependency('xerces-c', method: 'system')  # Force fallback to wrap
  endif
endif
lzma_dep = dependency('liblzma', version: '>=5.2.0')
zlib_dep = dependency('zlib')
xxhash_dep = dependency('libxxhash', version: '>=0.8.1')
if meson.version().version_compare('>=1.4.0')
  yaml_cpp_dep = dependency('yaml-cpp', version: '>=0.7.0')
else
  # Prior Meson versions don't provide versions for `method = cmake` wraps
  yaml_cpp_dep = dependency('yaml-cpp')
  if yaml_cpp_dep.type_name() != 'internal' and not yaml_cpp_dep.version().version_compare('>=0.7.0')
    yaml_cpp_dep = dependency('yaml-cpp', method: 'system')  # Force fallback to wrap
  endif
endif
papi_dep = dependency('papi', required: get_option('papi'), version: '>=6.0.0')


# Dyninst ships config files, but up through 12.3 they aren't actually usable. We have a Find*
# script that fiddles with the exposed targets until it works, so if we can't get what we want
# directly use the find script to do so.
_dyninst_opts = ['openmp=enabled', 'debuginfod=disabled']
dyninst_dep = dependency('Dyninst',
  components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
  modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
  version: '>=12.3', required: false, default_options: _dyninst_opts)
if not dyninst_dep.found()
  dyninst_dep = dependency('Dyninst', method: 'cmake',
    cmake_module_path: 'cmake',
    components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
    modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
    version: '>=12.3', default_options: _dyninst_opts)
endif


# These dependencies don't ship any dependency files, so we use CMake by default and fall back to
# compiler checks as a robustness case. The final fallback to dependency() in each stanza is to
# provide a better error message and enable wrap fallbacks.
libmonitor_dep = dependency('Libmonitor', method: 'cmake', cmake_module_path: 'cmake', required: false)
if not libmonitor_dep.found()
  _libmonitor_lib = cc.find_library('monitor', has_headers: ['monitor.h'], required: false)
  if _libmonitor_lib.found()
    libmonitor_dep = declare_dependency(dependencies: _libmonitor_lib, version: 'unknown')
  endif
endif
if not libmonitor_dep.found()
  libmonitor_dep = dependency('libmonitor', method: 'system')
endif

libiberty_dep = dependency('Libiberty', method: 'cmake', cmake_module_path: 'cmake', required: false)
if not libiberty_dep.found() and (cc.has_header('libiberty/demangle.h') or cc.has_header('demangle.h'))
  _libiberty_lib = cc.find_library('iberty', required: false)
  if _libiberty_lib.found()
    libiberty_dep = declare_dependency(dependencies: _libiberty_lib, version: 'unknown')
  endif
endif
if not libiberty_dep.found()
  libiberty_dep = dependency('libiberty', method: 'system')
endif

xed_dep = dependency('', required: false)
if host_machine.cpu_family() in ['x86', 'x86_64']
  xed_dep = dependency('Xed', method: 'cmake', cmake_module_path: 'cmake', version: '>=2022.08.11', required: false)
  if not xed_dep.found() and (cc.has_header('xed/xed-interface.h') or cc.has_header('xed-interface.h'))
    _xed_lib = cc.find_library('xed', required: false)
    if _xed_lib.found()
      _xed_ver = cc.get_define('XED_VERSION', prefix: '''
        #if __has_include(<xed/xed-interface.h>)
        #include <xed/xed-interface.h>
        #else
        #include <xed-interface.h>
        #endif
      ''', dependencies: _xed_lib).strip('"')
      if _xed_ver.startswith('v')
        _xed_ver = _xed_ver.substring(1)
      endif
      xed_dep = declare_dependency(
        dependencies: _xed_lib,
        version: _xed_ver,
      )
    endif
  endif
  if not xed_dep.found()
    xed_dep = dependency('xed', method: 'system', version: '>=2022.08.11')
  endif
endif

perfmon_dep = dependency('Perfmon2', method: 'cmake', cmake_module_path: 'cmake', version: '>=4.0', required: false)
if not perfmon_dep.found()
  _perfmon_lib = cc.find_library('pfm', has_headers: ['perfmon/pfmlib.h'], required: false)
  if _perfmon_lib.found()
    perfmon_dep = declare_dependency(
      dependencies: _perfmon_lib,
      version: cc.get_define('LIBPFM_VERSION', prefix: '#include <perfmon/pfmlib.h>', dependencies: _perfmon_lib),
    )
  endif
endif
if not perfmon_dep.found()
  perfmon_dep = dependency('libpfm', method: 'system', version: '>=4.0')
endif


# Find MPI. We try different methods to handle the quirks of different systems.
mpi_dep = dependency('', required: false)
if get_option('hpcprof_mpi').allowed()
  mpi_dep = dependency('MPI', language: 'cpp', required: false)

  # Meson currently only properly supports OpenMPI. Search for other implementations via pkg-config.
  if not mpi_dep.found()
    mpi_dep = dependency('mpich-cxx', 'mvapich2', method: 'pkg-config', required: false)
  endif

  # Use a custom Python script to decode the MPI compiler's various flags
  if not mpi_dep.found()
    _mpicxx = find_program('mpicxx', 'mpic++', required: false)
    if _mpicxx.found()
      _decode_mpi = find_program('meson/decode-mpi', native: true)
      _decoded_mpi = run_command(_decode_mpi, '--machine-readable', _mpicxx, check: false, capture: true)
      if _decoded_mpi.returncode() == 0 and _decoded_mpi.stdout() != ''
        _mpi_parts = _decoded_mpi.stdout().split('\0\0')
        mpi_dep = declare_dependency(
          compile_args: _mpi_parts[0].split('\0'),
          link_args: _mpi_parts[1].split('\0'),
        )

        # Sanity check that the flags we get from the script do in fact work like they should.
        assert(cpp.links('''
        #include <mpi.h>
        int main() {
          MPI_Init(nullptr, nullptr);
          int x = 42;
          MPI_Bcast(&x, 1, MPI_INT, 0, MPI_COMM_WORLD);
          MPI_Finalize();
          return 0;
        }
        ''', name: 'MPI sanity check', dependencies: mpi_dep))
      endif
    endif
  endif

  if not mpi_dep.found() and get_option('hpcprof_mpi').enabled()
    error('hpcprof_mpi=enabled but unable to find MPI')
  endif
endif


# For testing hpcprof-mpi, we also need mpiexec to actually launch the tool.
mpiexec = find_program(
  'mpiexec',
  required: get_option('hpcprof_mpi').disabled() ? get_option('hpcprof_mpi') : false,
)
if mpiexec.found()
  mpiexec = [mpiexec.full_path(), '-host', 'localhost:1000', '-n']
else
  _sh = find_program('sh')
  mpiexec = [_sh.full_path(), '-c', 'exit 77']
endif


# Find support for CUDA performance monitoring
#
# NB: The CUDA dependency expects to be used with (implicit) `link_language: 'cuda'`, but we link
# it with `link_language: 'c'`. This causes link errors (`cannot find: -lcupti`) at build time.
# Fetching the dependency() before adding the 'cuda' language changes the search algorithm a bit and
# removes some fallbacks/checks, but it works when we link.
#
# There are a couple different Meson bugs and antifeatures at play here to cause this, but it works.
nvdisasm = find_program('nvdisasm', '/usr/local/cuda/bin/nvdisasm', required: get_option('cuda'))
cupti_dep = dependency('CUDA', modules: ['cupti'], required: get_option('cuda'), version: '>=11.2').partial_dependency(compile_args: true, includes: true)
if not add_languages('cuda', native: false, required: get_option('cuda'))
  cupti_dep = dependency('', required: false)
endif


# Find support for ROCm performance monitoring
hip_dep = dependency('hip', method: 'cmake', modules: ['hip::amdhip64'], required: get_option('rocm'), version: '>=5.1')
hsa_dep = dependency('hsa-runtime64', method: 'cmake', required: get_option('rocm'))

# Technically, ROCtracer and ROCprofiler don't ship any dependency files. It is extremely rare to
# see them outside of a complete "ROCm installation" that also includes hip and hsa (above), but for
# hardness we also support getting them via compiler checks.
roctracer_dep = dependency('', required: false)
if get_option('rocm').allowed()
  roctracer_dep = dependency('ROCtracer', method: 'cmake', cmake_module_path: 'cmake', required: false)
  if not roctracer_dep.found()
    _roctracer_lib = cc.find_library('roctracer64', has_headers: ['roctracer/roctracer_hip.h'], required: false)
    if _roctracer_lib.found()
      roctracer_dep = declare_dependency(
        dependencies: _roctracer_lib,
        version: '@0@.@1@'.format(
          cc.get_define('ROCTRACER_VERSION_MAJOR', prefix: '#include <roctracer/roctracer.h>', dependencies: _roctracer_lib),
          cc.get_define('ROCTRACER_VERSION_MINOR', prefix: '#include <roctracer/roctracer.h>', dependencies: _roctracer_lib)
        ),
      )
    endif
  endif
  if not roctracer_dep.found()
    roctracer_dep = dependency('ROCtracer', method: 'system', required: get_option('rocm'))
  endif
endif

rocprofiler_dep = dependency('', required: false)
if get_option('rocm').allowed()
  rocprofiler_dep = dependency('ROCprofiler', method: 'cmake', cmake_module_path: 'cmake', required: false)
  if not rocprofiler_dep.found()
    _rocprofiler_lib = cc.find_library('rocprofiler64', has_headers: ['rocprofiler/rocprofiler.h'], required: false)
    if _rocprofiler_lib.found()
      # As a very special case, we also need the path to a metrics.xml file in the ROCprofiler
      # installation. We don't have any paths at hand to find it otherwise, so we require the user
      # to specify the path via a project option. (We use an option to match the `c*_args` used for
      # the compiler checks, usually one would add these options to a machine file.)
      if get_option('rocprofiler_metrics_xml') == ''
        error('-Drocprofiler_metrics_xml option must be set to use compiler fallback for ROCprofiler')
      endif
      _metrics_xml = fs.expanduser(get_option('rocprofiler_metrics_xml'))
      if not fs.exists(_metrics_xml)
        error(f'-Drocprofiler_metrics_xml set to an invalid path: no such file: @_metrics_xml@')
      endif
      rocprofiler_dep = declare_dependency(
        dependencies: _rocprofiler_lib,
        variables: {'metrics_xml': _metrics_xml},
        version: '@0@.@1@'.format(
          cc.get_define('ROCPROFILER_VERSION_MAJOR', prefix: '#include <rocprofiler/rocprofiler.h>', dependencies: _rocprofiler_lib),
          cc.get_define('ROCPROFILER_VERSION_MINOR', prefix: '#include <rocprofiler/rocprofiler.h>', dependencies: _rocprofiler_lib)
        ),
      )
    endif
  endif
  if not rocprofiler_dep.found()
    rocprofiler_dep = dependency('ROCprofiler', method: 'system', required: get_option('rocm'))
  endif
endif

_rocm_subdeps = [rocprofiler_dep, roctracer_dep, hip_dep, hsa_dep]
rocm_dep = declare_dependency(
  dependencies: _rocm_subdeps,
  version: hip_dep.version(),
).partial_dependency(compile_args: true, includes: true)
foreach dep : _rocm_subdeps
  if not dep.found()
    rocm_dep = dependency('', required: false)
    break
  endif
endforeach

hipcc = disabler()
hipcc_cmd = disabler()
rocprofiler_path = disabler()
if rocm_dep.found()
  hipcc = find_program('hipcc', '/opt/rocm/bin/hipcc', required: true)
  hipcc_cmd = [hipcc, '-o', '@OUTPUT@', '@INPUT@']
  rocprofiler_path = fs.parent(rocprofiler_dep.get_variable(cmake: 'ROCprofiler_METRICS_XML', internal: 'metrics_xml'))
endif


# Find OpenCL support. We only need the headers, but there are many ways to acquire them.
opencl_dep = dependency('', required: false)
if get_option('opencl').allowed()
  # - As of OpenCL >=2023.02.06 (found in Debian >=12, Ubuntu >=23.04, and Fedora) the headers can
  #   be accessed via pkg-config as OpenCL-Headers.pc.
  # - Before that, since OpenCL >=2020.12.18 (found in Debian >=10, Ubuntu >=20.04, and RHEL >=9)
  #   the headers can be accessed via CMake as OpenCLHeaders-config.cmake.
  # - For older distros, CMake >=3.1 ships a FindOpenCL.cmake script, but it requires a full OpenCL
  #   installation and not just the headers.
  #
  # Try all three approaches and take it if it works.
  opencl_dep = dependency('OpenCL-Headers', 'OpenCLHeaders', 'OpenCL', required: false, version: '>=2.2').partial_dependency(includes: true, compile_args: true)

  if not opencl_dep.found() and cc.has_header('CL/cl.h', required: false)
    # For Spack builds and partial system installations, we also support the case where the header
    # is already available on the standard include paths.
    opencl_dep = declare_dependency()
  endif

  if not opencl_dep.found()
    # Fall back to dependency() to get a good error message and enable wrap fallbacks.
    opencl_dep = dependency('OpenCL-Headers', required: get_option('opencl'))
  endif
endif


# Find support for Intel performance monitoring via Level Zero.
level0_dep = dependency('', required: false)
if get_option('level0').allowed()
  level0_dep = dependency('level-zero', required: false, version: '>=1.0')
  if level0_dep.found() and level0_dep.type_name() != 'internal'
    # Level Zero ships a pkg-config file, but it's untrustworthy due to a bug
    # present since 1.0: https://github.com/oneapi-src/level-zero/issues/131.
    # We try to reconstruct what the library path "should have been" here, using
    # our own configuration as a reference.
    #
    # Note that we *always* manipulate the library path here. Compiler checks
    # would succeed if Level Zero is also installed at the system level, so we
    # can't trust them to determine if the Level Zero pkg-config file is borked.
    _l0_libdir = level0_dep.get_variable('libdir', default_value: '')
    _l0_prefix = level0_dep.get_variable('prefix', default_value: '')
    if _l0_libdir != '' and _l0_prefix != ''
      _l0_new_libdir = _l0_prefix / get_option('libdir')
      if _l0_libdir != _l0_new_libdir and fs.exists(_l0_new_libdir)
        # We use find_library here to produce an rpath for the build tree.
        _l0_lib = cc.find_library('ze_loader', dirs: [_l0_libdir, _l0_new_libdir], required: false)
        if _l0_lib.found()
          message('Replacing Level Zero libdir: \'@0@\' -> \'@1@\''.format(_l0_libdir, _l0_new_libdir))
          level0_dep = declare_dependency(
            dependencies: [level0_dep, _l0_lib],
            link_args: ['-L@0@'.format(_l0_new_libdir)],
            version: level0_dep.version(),
          )
        endif
      endif
    endif

    # Sanity-check the modified dependency before calling it done.
    if not cc.links('''
      #include <ze_api.h>
      int main() {
        zeInit(0);
        return 0;
      }
      ''', name: 'Level Zero sanity check', dependencies: level0_dep)
      error('''Failed to reconstruct usable Level Zero dependency! This could be for many reasons, consider:
- Checking meson-logs/meson-log.txt for the compiler error, and
- Manually fixing the libdir variable in the pkg-config file shipped by Level Zero (pkg-config --path level-zero), or
- Appending the flags to use Level Zero to (c|cpp)(_link)?_args, or
- Removing Level Zero from your PKG_CONFIG_PATH.''')
    endif
  endif

  if not level0_dep.found()
    # To work around certain incomplete modulefiles, we also support the case
    # where the library and headers are already available in compiler flags.
    _l0_lib = cc.find_library('ze_loader', has_headers: ['ze_api.h'], required: false)
    if _l0_lib.found()
      _l0_ver = cc.compute_int('ZE_API_VERSION_CURRENT',
        prefix: '#include <ze_api.h>', dependencies: _l0_lib,
        low: 0x10000, guess: 0x10008, high: 0x30000)
      level0_dep = declare_dependency(
        dependencies: _l0_lib,
        version: '@0@.@1@'.format(_l0_ver / 0x10000, _l0_ver % 0x10000),
      )
    endif
  endif

  if not level0_dep.found()
    # Fall back to dependency() to get a good error message and enable wrap fallbacks
    level0_dep = dependency('level-zero', required: get_option('level0'), version: '>=1.0')
  endif
endif

# Find support for instruction level collection, which requires IGC and IGA
_gtpin_f = get_option('gtpin').require(level0_dep.found(), error_message: 'gtpin is only available with level0')

igc_dep = dependency('', required: false)
if _gtpin_f.allowed()
  igc_dep = dependency('igc-opencl', required: false)

  if not igc_dep.found()
    # To work around certain incomplete deployments/modulefiles, we also support
    # the case where the headers are already available by full path in the
    # compiler flags. In this case IGC_INCLUDE_FULL_PATH is #defined and
    # downstream sources #include a longer header path when this is set.
    if (cc.has_header('igc/igc.opencl.h')
        and cc.has_header('igc/ocl_igc_shared/executable_format/patch_list.h')
        and cc.has_header('igc/ocl_igc_shared/device_enqueue/DeviceEnqueueInternalTypes.h'))
      igc_dep = declare_dependency(
        compile_args: ['-DIGC_INCLUDE_FULL_PATH'],
        version: 'unknown',
      )
    endif
  endif

  if not igc_dep.found()
    # Fall back to dependency() to get a good error message and enable wrap fallbacks
    igc_dep = dependency('igc-opencl', required: _gtpin_f)
  endif
endif
if igc_dep.found()
  add_project_arguments('-DENABLE_IGC', language: ['c', 'cpp'])
endif

iga_dep = dependency('', required: false)
if _gtpin_f.allowed()
  # IGA doesn't ship any dependency files, so we use a custom CMake script for the usual case.
  iga_dep = dependency('IGA', method: 'cmake', cmake_module_path: 'cmake', required: false)

  if not iga_dep.found()
    # To work around certain incomplete modulefiles, we also support the case where the
    # headers and library are already available in the compiler flags.
    _iga_lib = cc.find_library('iga64', has_headers: ['iga/iga.h'], required: false)
    if _iga_lib.found()
      iga_dep = declare_dependency(dependencies: _iga_lib, version: 'unknown')
    endif
  endif

  if not iga_dep.found()
    # Fall back to dependency() to get a good error message and enable wrap fallbacks
    iga_dep = dependency('IGA', method: 'cmake', cmake_module_path: 'cmake', required: _gtpin_f)
  endif
endif

# GTPin is a snowflake of a case, it only comes in a binary tarball with non-standard names for
# all the paths like `Include` and `Lib`. This means we can't use CMake to find it, the user
# *has* to provide it through `cpp_args` and `cpp_link_args`.
# But since that's way too much trouble in most cases, we also support pulling it from a wrap,
# when the feature is explicitly requested.
gtpin_dep = cpp.find_library('gtpin', has_headers: ['api/gtpin_api.h', 'ged.h'], required: false)
if gtpin_dep.found()
  # Wrap the result of find_library() as a dependency so we can query the nonexistent version later
  gtpin_dep = declare_dependency(dependencies: gtpin_dep, version: 'unknown')
else
  gtpin_dep = dependency('gtpin', method: 'system', required: get_option('gtpin'))
endif
if gtpin_dep.found()
  add_project_arguments('-DENABLE_GTPIN', language: ['c', 'cpp'])
endif


# Collect features from the compiler itself
add_project_dependencies(dependency('openmp'), language: ['c', 'cpp'])
dl_dep = dependency('dl')
add_project_dependencies(cc.find_library('rt'), language: ['c'])
_fs_code = '''
#include <filesystem>
int main() {
  std::filesystem::path p("/foo/bar");
  std::filesystem::remove(p);
  return 0;
}
'''
if not cpp.links(_fs_code)
  _found = false
  foreach trial : ['stdc++fs', 'c++fs']
    _dep = cpp.find_library(trial, required: false)
    if _dep.found()
      add_project_dependencies(_dep, language: ['c', 'cpp'])
      _found = true
      break
    endif
  endforeach
  if not _found
    error('Unable to find suitable flags for std::filesystem')
  endif
endif


# If Valgrind annotations aren't requested, inject an option to disable them.
if not get_option('valgrind_annotations')
  add_project_arguments('-DNVALGRIND', language: ['c', 'cpp'])
endif


# Enable a handful of useful non-default warnings
foreach lang : ['c', 'cpp']
  _warnings = meson.get_compiler(lang).get_supported_arguments(
      '-Wstack-usage=65536',  # Warn if any functions use a lot or unbounded stack
  )
  set_variable(f'_@lang@_warnings', _warnings)
  add_project_arguments(_warnings, language: lang)
endforeach


# Run a number of compile tests that are used for the various source files
cfg = configuration_data()
cfg.set_quoted('HPCTOOLKIT_INSTALL_PREFIX', get_option('prefix'),
        description: 'HPCToolkit install prefix')
cfg.set_quoted('HPCTOOLKIT_VERSION', meson.project_version(),
        description: 'HPCToolkit version')
cfg.set_quoted('HPCTOOLKIT_VERSION_STRING', 'A member of HPCToolkit, version @0@'.format(meson.project_version()),
        description: 'HPCToolkit version string')
cfg.set_quoted('HPCTOOLKIT_SPACK_SPEC', 'unknown (deprecated)',
        description: 'Spack version and variants, if known. (legacy)')
cfg.set('SIZEOF_VOIDP', cc.compute_int('sizeof(void*)', low: 1, high: 64, guess: 8))
cfg.set('HOST_CPU_x86', host_machine.cpu_family() == 'x86',
        description: 'HOST CPU: x86 (32-bit)')
cfg.set('HOST_CPU_x86_64', host_machine.cpu_family() == 'x86_64',
        description: 'HOST CPU: x86_64 (64-bit)')
cfg.set('HOST_CPU_IA64', host_machine.cpu_family() == 'ia64',
        description: 'HOST CPU: ia64 (itanium)')
cfg.set('HOST_CPU_PPC', host_machine.cpu_family() == 'ppc64',
        description: 'HOST CPU: PowerPC (ppc)')
cfg.set('HOST_CPU_ARM64', host_machine.cpu_family() == 'aarch64',
        description: 'HOST CPU: ARM 64 (aarch64)')
cfg.set('HOST_BIG_ENDIAN', host_machine.endian() == 'big',
        description: 'Host is big endian.')
cfg.set('HOST_LITTLE_ENDIAN', host_machine.endian() == 'little',
        description: 'Host is little endian.')
cfg.set('USE_SYSTEM_BYTESWAP', cc.has_header_symbol('byteswap.h', 'bswap_64'),
        description: 'Use system byteswap.h')
cfg.set('HAVE_FUTEX_H', cc.has_header_symbol('linux/futex.h', 'SYS_futex', prefix: ['#include <sys/syscall.h>', '#include <unistd.h>']),
        description: 'Whether futexes are available for use')
cfg.set('HOST_SYSTEM_IBM_BLUEGENE', false, description: 'IBM Blue Gene support (disabled, deprecated)')
cfg.set('ENABLE_OPENMP', true,
        description: 'ParseAPI supports openmp (for hpcstruct)')
cfg.set('ENABLE_OPENMP_SYMTAB', true,
        description: 'Symtab supports openmp (for fnbounds)')
cfg.set('ENABLE_LOGICAL_PYTHON', python_dep.found(),
        description: 'Support for Python logical unwinding')
cfg.set('ENABLE_XOP', false,
        description: 'Support for AMD XOP instructions (disabled)')
cfg.set('ENABLE_CLOCK_REALTIME', cc.has_header_symbol('time.h', 'CLOCK_REALTIME') and cc.has_header_symbol('signal.h', 'SIGEV_THREAD_ID'),
        description: 'Support for CLOCK_REALTIME and SIGEV_THREAD_ID')
cfg.set('ENABLE_CLOCK_CPUTIME', cfg.get('ENABLE_CLOCK_REALTIME') and cc.has_header_symbol('time.h', 'CLOCK_THREAD_CPUTIME_ID'),
        description: 'Support for CLOCK_THREAD_CPUTIME_ID')
cfg.set('OPT_HAVE_CUDA', cupti_dep.found(),
        description: 'Have support for cuda')
cfg.set_quoted('CUDA_NVDISASM_PATH', nvdisasm.found() ? nvdisasm.full_path() : '',
        description: 'Path to CUDA nvdisasm utility')
cfg.set_quoted('CUPTI_INSTALL_PREFIX', '/nonexistent',  # TODO
        description: 'CUPTI install prefix')
cfg.set('ENABLE_OPENCL', opencl_dep.found(),
        description: 'Enable OpenCL monitoring')
cfg.set('HAVE_ROCM_ACTIVITY_KERNEL_NAME', cc.has_member('roctracer_record_t', 'kernel_name', prefix: '#include <roctracer/roctracer_hip.h>', dependencies: rocm_dep),
        description: 'activity_record has kernel_name field')
cfg.set('USE_ROCM', rocm_dep.found(),
        description: 'use ROCM')
cfg.set('DATACENTRIC_TRACE', false, description: 'Data-centric tracing (disabled, deprecated)')

# Xed compiler checks
if xed_dep.type_name() == 'internal'
  assert(xed_dep.version() != 'undefined')
  cfg.set('XED_DISPLACEMENT_INT64', xed_dep.version().version_compare('>=2023.08.21'))
else
  cfg.set('XED_DISPLACEMENT_INT64',
    (cc.has_header_symbol('xed/xed-operand-values-interface.h', 'xed_operand_values_get_branch_displacement_int64', dependencies: xed_dep) or
     cc.has_header_symbol('xed-operand-values-interface.h', 'xed_operand_values_get_branch_displacement_int64', dependencies: xed_dep)),
          description: 'xed uses xed_operand_values_get_branch_displacement_int64')
endif

# Dyninst feature checks
if dyninst_dep.type_name() == 'internal'
  assert(dyninst_dep.version() != 'undefined')
  cfg.set('USE_XED_FOR_GAPS', dyninst_dep.version().version_compare('>=13.0.0'))
  cfg.set('USE_GET_CONTAINING_MODULE', dyninst_dep.version().version_compare('>=13.0.0'))
  cfg.set('DYNINST_SUPPORTS_INTEL_GPU', dyninst_dep.version().version_compare('>=11.0.0'))
  cfg.set('DYNINST_USE_CUDA', dyninst_dep.version().version_compare('>=10.0.0'))
else
  cfg.set('USE_XED_FOR_GAPS', cpp.compiles('''
#include <InstructionDecoder.h>
void test() {
  (void)Dyninst::InstructionAPI::InstructionDecoder::unknown_instruction::register_callback;
}
''',
            name: 'Dyninst::InstructionAPI::InstructionDecoder has unknown_instruction::register_callback',
            dependencies: dyninst_dep),
          description: 'use dyninst and xed for unknown instructions')
  cfg.set('USE_GET_CONTAINING_MODULE', cpp.compiles('''
#include <Symtab.h>
void test() {
  Dyninst::SymtabAPI::Symtab sym;
  sym.getContainingModule(42);
}
''',
            name: 'Dyninst::SymtabAPI::Symtab uses getContainingModule()',
            dependencies: dyninst_dep),
          description: 'use getContainingModule API')
  cfg.set('DYNINST_SUPPORTS_INTEL_GPU', cpp.has_header_symbol('entryIDs.h', 'intel_gpu_op_general', dependencies: dyninst_dep),
          description: 'dyninst supports Intel GPUs')
  cfg.set('DYNINST_USE_CUDA', cpp.compiles('''
#include <Symtab.h>
void test() {
  (void)Dyninst::Arch_cuda;
}
''',
            name: 'Dyninst has Arch_cuda',
            dependencies: dyninst_dep),
          description: 'dyninst supports cuda')
endif

subdir('src')
subdir('lib')
subdir('doc')
subdir('tests2') # FIXME: Eventually rename this to just tests/
subdir('meson')


# Install a few files from this directory
install_data(
  'README.md',
  'README.Acknowledgments',
  'README.Install',
  'README.ReleaseNotes',
  'LICENSE',
  install_dir: docdir,
  install_tag: 'docs',
)
