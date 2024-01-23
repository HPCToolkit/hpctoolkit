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
tbb_dep = dependency('TBB', components: ['tbb'])
tbb_malloc_dep = dependency('TBB', components: ['tbb', 'tbbmalloc'])
libelf_dep = dependency('libelf', version: '>=0.186')
libdw_dep = dependency('libdw', version: '>=0.186')
libmonitor_dep = dependency('Libmonitor', method: 'cmake', cmake_module_path: 'cmake')
libunwind_dep = dependency('libunwind', version: '>=1.6.2')
xerces_dep = dependency('xerces-c', version: '>=3.2.3')
lzma_dep = dependency('liblzma', version: '>=5.2.0')
zlib_dep = dependency('zlib')
libiberty_dep = dependency('Libiberty', method: 'cmake', cmake_module_path: 'cmake')
xed_dep = dependency('', required: false)
if host_machine.cpu_family() in ['x86', 'x86_64']
  xed_dep = dependency('Xed', method: 'cmake', cmake_module_path: 'cmake', version: '>=2022.08.11')
endif
yaml_cpp_dep = dependency('yaml-cpp', version: '>=0.7.0')
perfmon_dep = dependency('Perfmon2', method: 'cmake', cmake_module_path: 'cmake', version: '>=4.0')
papi_dep = dependency('papi', required: get_option('papi'), version: '>=6.0.0')


# Dyninst ships config files, but up through 12.3 they aren't actually usable. We have a Find*
# script that fiddles with the exposed targets until it works, so if we can't get what we want
# directly use the find script to do so.
dyninst_dep = dependency('Dyninst',
  components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
  modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
  version: '>=12.3', required: false)
if not dyninst_dep.found()
  dyninst_dep = dependency('Dyninst', method: 'cmake',
    cmake_module_path: 'cmake',
    components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
    modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
    version: '>=12.3')
endif


# Find MPI. We try different methods to handle the quirks of different systems.
mpi_dep = dependency('', required: false)
if get_option('hpcprof_mpi').allowed()
  mpi_dep = dependency('MPI', language: 'cpp', required: false)
  if not mpi_dep.found()
    # Meson currently only properly supports OpenMPI. Search for the others via pkg-config first.
    mpi_dep = dependency('mpich-cxx', 'mvapich2', method: 'pkg-config', required: false)
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
roctracer_dep = dependency('ROCtracer', method: 'cmake', cmake_module_path: 'cmake', required: get_option('rocm'))
rocprofiler_dep = dependency('ROCprofiler', method: 'cmake', cmake_module_path: 'cmake', required: get_option('rocm'))
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
  rocprofiler_path = fs.parent(rocprofiler_dep.get_variable(cmake: 'ROCprofiler_METRICS_XML'))
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


# Find support for Intel GPU performance monitoring
level0_dep = dependency('level-zero', required: get_option('level0'), version: '>=1.0.0')
# GTPin, IGC and IGA are only needed for instruction-level collection
_gtpin_f = get_option('gtpin').require(level0_dep.found(), error_message: 'gtpin is only available with level0')
igc_dep = dependency('igc-opencl', required: _gtpin_f)
if igc_dep.found()
  add_project_arguments('-DENABLE_IGC', language: ['c', 'cpp'])
endif
iga_dep = dependency('IGA', method: 'cmake', cmake_module_path: 'cmake', required: _gtpin_f)

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
cfg.set_quoted('HPCTOOLKIT_GIT_VERSION', '@VCS_TAG@',
        description: 'Git branch and commit hash, if known.')
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
cfg.set('XED_DISPLACEMENT_INT64', cc.has_header_symbol('xed-operand-values-interface.h', 'xed_operand_values_get_branch_displacement_int64', dependencies: xed_dep),
        description: 'xed uses xed_operand_values_get_branch_displacement_int64')
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


subdir('src')
subdir('lib')
subdir('doc')
subdir('tests2') # FIXME: Eventually rename this to just tests/


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
