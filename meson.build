# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

project(
  'hpctoolkit',
  'c',
  'cpp',
  version: '2024.01.99-next',
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


# FIXME: -Wunused-result reports warnings all over the codebase. Until this can be fixed, prevent
# it from causing complete compilation failure when running with -Werror.
if get_option('werror')
  foreach _lang : ['c', 'cpp']
    add_project_arguments(
      meson.get_compiler(_lang).first_supported_argument('-Wno-error=unused-result'),
      language: _lang,
    )
  endforeach
endif


# Find Python.
python = find_program('python3', 'python', version: '>=3.8', required: false)
if not python.found()
  _pynames = []
  foreach ver : range(8, 15)
    _pynames += [f'python3.@ver@']
  endforeach
  python = find_program(_pynames, version: '>=3.8')
endif

# Find the Python installation, used for the Python unwinding support. This needs to be the same
# version as the Python we test with to make everything compatible.
python_inst = pymod.find_installation(python.full_path(), required: get_option('python'))
if python_inst.found()
  assert(python_inst.language_version().version_compare('>=3.8'))
  _py_dep = python_inst.dependency(
    required: get_option('python'),
    include_type: 'system',
  )
  # FIXME: Workaround for https://github.com/mesonbuild/meson/issues/12862
  if not cc.has_header('Python.h', dependencies: _py_dep)
    _py_dep = dependency('', required: false)
  endif
  # To support methods like extension_module(), we make the whole Python install object
  # "not found" if the dependency was not found.
  if not _py_dep.found()
    python_inst = disabler()
  endif
endif
if python_inst.found()
  add_project_arguments('-DENABLE_LOGICAL_PYTHON', language: ['c', 'cpp'])
endif

# Find TBB. This is done first before any other subprojects using it to resolve overridden
# dependencies in the subproject (which does not use the wrap fallback trigger).
# NB: We have to find normal TBB first to enable the tbb-wrap subproject override for Dyninst.
dependency('tbb', 'TBB', components: ['tbb'], fallback: ['tbb-wrap', 'tbb_dep'])
tbb_malloc_dep = dependency(
  'tbbmalloc',
  'TBB',
  components: ['tbb', 'tbbmalloc'],
  fallback: ['tbb-wrap', 'tbbmalloc_dep'],
  include_type: 'system',
)

# Find Elfutils. We search for libdw first because if libelf is installed but libdw isn't, the
# dependency('libdw') call will invoke the Elfutils subproject and call meson.override_dependency()
# for libelf, if we found libelf first this call causes a Meson setup failure.
# In all major distros you can't install libdw without libelf, so this shouldn't cause problems.
libdw_dep = dependency(
  'libdw',
  version: '>=0.186',
  default_options: ['thread_safety=true'],
  include_type: 'system',
)
libelf_dep = dependency(
  'libelf',
  version: '>=0.186',
  default_options: ['thread_safety=true'],
  include_type: 'system',
)

# Collect all the dependencies we require for HPCToolkit
boost_dep = dependency(
  'boost',
  'Boost',
  modules: ['graph'],
  version: '>=1.66.0',
  include_type: 'system',
)
libunwind_dep = dependency(
  'libunwind',
  version: ['>=1.6.2', '!=1.21'],
  default_options: ['minidebuginfo=enabled', 'zlibdebuginfo=enabled'],
  include_type: 'system',
)
_xerces_c_ver = '>=3.2.2'
if meson.version().version_compare('>=1.4.0')
  xerces_dep = dependency('xerces-c', version: _xerces_c_ver, include_type: 'system')
else
  xerces_dep = dependency('xerces-c', include_type: 'system')
  if xerces_dep.type_name() != 'internal'
    if not xerces_dep.version().version_compare(_xerces_c_ver)
      message(
        'Dependency @0@ found @1@ but need: \'@2@\''.format(
          xerces_dep.name(),
          xerces_dep.version(),
          _xerces_c_ver,
        ),
      )
      xerces_dep = dependency('xerces-c', method: 'system', include_type: 'system')      # Force fallback to wrap
    endif
  endif
endif
lzma_dep = dependency('liblzma', version: '>=5.2.0', include_type: 'system')
xxhash_dep = dependency('libxxhash', version: '>=0.8.1', include_type: 'system')
_yaml_cpp_ver = '>=0.6.2'
if meson.version().version_compare('>=1.4.0')
  yaml_cpp_dep = dependency('yaml-cpp', version: _yaml_cpp_ver, include_type: 'system')
else
  yaml_cpp_dep = dependency('yaml-cpp', include_type: 'system')
  if yaml_cpp_dep.type_name() != 'internal'
    if not yaml_cpp_dep.version().version_compare(_yaml_cpp_ver)
      message(
        'Dependency @0@ found @1@ but need: \'@2@\''.format(
          yaml_cpp_dep.name(),
          yaml_cpp_dep.version(),
          _yaml_cpp_ver,
        ),
      )
      yaml_cpp_dep = dependency('yaml-cpp', method: 'system', include_type: 'system')      # Force fallback to wrap
    endif
  endif
endif
papi_dep = dependency(
  'papi',
  required: get_option('papi'),
  version: '>=5.6.0',
  include_type: 'system',
)
papi_has_components = false
if papi_dep.found()
  add_project_arguments('-DHPCRUN_SS_PAPI', language: ['c', 'cpp'])
  papi_has_components = cc.has_header_symbol(
    'papi.h',
    'PAPI_get_eventset_component',
    dependencies: papi_dep,
  )
  if papi_has_components
    add_project_arguments('-DHPCRUN_SS_PAPI_C_INTEL', language: ['c', 'cpp'])
  endif
endif


# These dependencies don't ship any dependency files, so we use CMake by default and fall back to
# compiler checks as a robustness case. The final fallback to dependency() in each stanza is to
# provide a better error message and enable wrap fallbacks.
libiberty_dep = dependency(
  'Libiberty',
  method: 'cmake',
  cmake_module_path: 'cmake',
  required: false,
  include_type: 'system',
)
if not libiberty_dep.found()
  if cc.has_header('libiberty/demangle.h') or cc.has_header('demangle.h')
    _libiberty_lib = cc.find_library('iberty', required: false)
    if _libiberty_lib.found()
      libiberty_dep = declare_dependency(
        dependencies: _libiberty_lib,
        version: 'unknown',
      )
    endif
  endif
endif
if not libiberty_dep.found()
  libiberty_dep = dependency('libiberty', method: 'system', include_type: 'system')  # Force wrap fallback
endif

xed_dep = dependency('', required: false)
if host_machine.cpu_family() in ['x86', 'x86_64']
  xed_dep = dependency(
    'Xed',
    method: 'cmake',
    cmake_module_path: 'cmake',
    version: '>=2022.08.11',
    required: false,
  )
  if not xed_dep.found()
    if cc.has_header('xed/xed-interface.h') or cc.has_header('xed-interface.h')
      _xed_lib = cc.find_library('xed', required: false)
      if _xed_lib.found()
        _xed_ver = cc.get_define(
          'XED_VERSION',
          prefix: '''
          #if __has_include(<xed/xed-interface.h>)
          #include <xed/xed-interface.h>
          #else
          #include <xed-interface.h>
          #endif
        ''',
          dependencies: _xed_lib,
        ).strip('"')
        if _xed_ver.startswith('v')
          _xed_ver = _xed_ver.substring(1)
        endif
        xed_dep = declare_dependency(dependencies: _xed_lib, version: _xed_ver)
      endif
    endif
  endif
  if not xed_dep.found()
    xed_dep = dependency('xed', method: 'system', version: '>=2022.08.11')    # Force wrap fallback
  endif
endif
if xed_dep.found()
  if xed_dep.type_name() == 'internal'
    assert(xed_dep.version() != 'undefined')
    if xed_dep.version().version_compare('>=2023.08.21')
      add_project_arguments('-DXED_DISPLACEMENT_INT64', language: ['c', 'cpp'])
    endif
  else
    foreach _prefix : ['xed/', '']
      if cc.has_header_symbol(
        f'@_prefix@xed-operand-values-interface.h',
        'xed_operand_values_get_branch_displacement_int64',
        dependencies: xed_dep,
      )
        add_project_arguments('-DXED_DISPLACEMENT_INT64', language: ['c', 'cpp'])
        break
      endif
    endforeach
  endif
endif

perfmon_dep = dependency(
  'Perfmon2',
  method: 'cmake',
  cmake_module_path: 'cmake',
  version: '>=4.0',
  required: false,
  include_type: 'system',
)
if not perfmon_dep.found()
  _perfmon_lib = cc.find_library(
    'pfm',
    has_headers: ['perfmon/pfmlib.h'],
    required: false,
  )
  if _perfmon_lib.found()
    perfmon_dep = declare_dependency(
      dependencies: _perfmon_lib,
      version: cc.get_define(
        'LIBPFM_VERSION',
        prefix: '#include <perfmon/pfmlib.h>',
        dependencies: _perfmon_lib,
      ),
    )
  endif
endif
if not perfmon_dep.found()
  perfmon_dep = dependency(
    'libpfm',
    method: 'system',
    version: '>=4.0',
    include_type: 'system',
  )
endif


# Dyninst ships config files, but up through 12.3 they aren't actually usable. We have a Find*
# script that fiddles with the exposed targets until it works, so if we can't get what we want
# directly use the find script to do so.
_dyninst_opts = ['openmp=enabled', 'debuginfod=disabled']
dyninst_dep = dependency(
  'Dyninst',
  components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
  modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
  version: '>=12.3',
  required: false,
  default_options: _dyninst_opts,
  include_type: 'system',
)
if not dyninst_dep.found()
  dyninst_dep = dependency(
    'Dyninst',
    method: 'cmake',
    cmake_module_path: 'cmake',
    components: ['parseAPI', 'instructionAPI', 'symtabAPI'],
    modules: ['Dyninst::parseAPI', 'Dyninst::instructionAPI', 'Dyninst::symtabAPI'],
    version: '>=12.3',
    default_options: _dyninst_opts,
    include_type: 'system',
  )
endif

# Set flags for features available in Dyninst
if dyninst_dep.type_name() == 'internal'
  assert(dyninst_dep.version() != 'undefined')
  if dyninst_dep.version().version_compare('>=13.0.0')
    add_project_arguments('-DUSE_GET_CONTAINING_MODULE', language: ['c', 'cpp'])
    if xed_dep.found()
      add_project_arguments('-DUSE_XED_FOR_GAPS', language: ['c', 'cpp'])
    endif
  endif
  if dyninst_dep.version().version_compare('>=11.0.0')
    add_project_arguments('-DDYNINST_SUPPORTS_INTEL_GPU', language: ['c', 'cpp'])
  endif
  if dyninst_dep.version().version_compare('>=10.0.0')
    add_project_arguments('-DDYNINST_USE_CUDA', language: ['c', 'cpp'])
  endif
else
  if xed_dep.found() and cpp.compiles(
    '''
#include <InstructionDecoder.h>
void test() {
  (void)Dyninst::InstructionAPI::InstructionDecoder::unknown_instruction::register_callback;
}
''',
    name: 'Dyninst::InstructionAPI::InstructionDecoder has unknown_instruction::register_callback',
    dependencies: dyninst_dep,
  )
    add_project_arguments('-DUSE_XED_FOR_GAPS', language: ['c', 'cpp'])
  endif
  if cpp.compiles(
    '''
#include <Symtab.h>
void test() {
  Dyninst::SymtabAPI::Symtab sym;
  sym.getContainingModule(42);
}
''',
    name: 'Dyninst::SymtabAPI::Symtab uses getContainingModule()',
    dependencies: dyninst_dep,
  )
    add_project_arguments('-DUSE_GET_CONTAINING_MODULE', language: ['c', 'cpp'])
  endif
  if cpp.has_header_symbol(
    'entryIDs.h',
    'intel_gpu_op_general',
    dependencies: dyninst_dep,
  )
    add_project_arguments('-DDYNINST_SUPPORTS_INTEL_GPU', language: ['c', 'cpp'])
  endif
  if cpp.compiles(
    '''
#include <Symtab.h>
void test() {
  (void)Dyninst::Arch_cuda;
}
''',
    name: 'Dyninst has Arch_cuda',
    dependencies: dyninst_dep,
  )
    add_project_arguments('-DDYNINST_USE_CUDA', language: ['c', 'cpp'])
  endif
endif
add_project_arguments(
  '-DENABLE_OPENMP',
  '-DENABLE_OPENMP_SYMTAB',
  language: ['c', 'cpp'],
)


# Find GoogleTest which is used as a test harness and framework. GoogleMock is a bundled library
# with extra features that extend the base GoogleTest experience.
gtest_dep = dependency('gtest', version: '>=1.8.1')
gtest_main_dep = dependency('gtest_main', version: '>=1.8.1')
gmock_dep = dependency('gmock', version: '>=1.8.1')
gmock_main_dep = dependency('gmock_main', version: '>=1.8.1')


# Find rst2man, which is used to generate the man pages from source
rst2man = find_program('rst2man', 'rst2man.py', required: get_option('manpages'))


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
      _decoded_mpi = run_command(
        python,
        files('meson/decode-mpi'),
        '--machine-readable',
        _mpicxx,
        check: false,
        capture: true,
      )
      if _decoded_mpi.returncode() == 0 and _decoded_mpi.stdout() != ''
        _mpi_parts = _decoded_mpi.stdout().split('\0\0')
        mpi_dep = declare_dependency(
          compile_args: _mpi_parts[0].split('\0'),
          link_args: _mpi_parts[1].split('\0'),
        )

        # Sanity check that the flags we get from the script do in fact work like they should.
        assert(
          cpp.links(
            '''
        #include <mpi.h>
        int main() {
          MPI_Init(nullptr, nullptr);
          int x = 42;
          MPI_Bcast(&x, 1, MPI_INT, 0, MPI_COMM_WORLD);
          MPI_Finalize();
          return 0;
        }
        ''',
            name: 'MPI sanity check',
            dependencies: mpi_dep,
          ),
        )
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
nvdisasm = find_program(
  'nvdisasm',
  '/usr/local/cuda/bin/nvdisasm',
  required: get_option('cuda'),
)
if nvdisasm.found()
  add_project_arguments(
    '-DCUDA_NVDISASM_PATH="@0@"'.format(
      nvdisasm.full_path().replace('\\', '\\\\').replace('"', '\\"'),
    ),
    language: ['c', 'cpp'],
  )
endif
cupti_dep = dependency('', required: false)
if get_option('cuda').allowed()
  cupti_dep = dependency(
    'CUDA',
    modules: ['cupti', 'cuda'],
    required: false,
    version: '>=11.2',
    include_type: 'system',
  )
  if not cupti_dep.found()
    cupti_dep = dependency(
      'CUDAToolkit',
      components: ['cupti'],
      modules: ['CUDA::cupti', 'CUDA::cuda_driver', 'CUDA::cudart_static'],
      required: get_option('cuda'),
      include_type: 'system',
    )
  endif
endif
if cupti_dep.found()
  add_project_arguments(
    '-DOPT_HAVE_CUDA',
    '-DENABLE_CUDA',
    '-DHPCRUN_SS_NVIDIA',
    '-DCUPTI_INSTALL_PREFIX="/nonexistent"',  # TODO
    language: ['c', 'cpp'],
  )
endif
if false  # get_option('papi_cupti')  # --enable-papi-c-cupti
  add_project_arguments('-DHPCRUN_SS_PAPI_C_CUPTI', language: ['c', 'cpp'])
endif

# Note that we only need the CUDA language for the tests, so if cupti_dep wasn't found we don't
# enable this here either.
has_cuda = false
if cupti_dep.found()
  has_cuda = add_languages('cuda', native: false, required: get_option('extended_tests'))
  if has_cuda
    cuda_cc = meson.get_compiler('cuda')
    if cuda_cc.get_id() == 'nvcc'
      _arches = 'All'
      unstable_cuda = import('unstable-cuda')
      message(
        'Building for CUDA architectures: ' + ' '.join(
          unstable_cuda.nvcc_arch_readable(cuda_cc.version(), _arches),
        ),
      )
      add_project_arguments(
        unstable_cuda.nvcc_arch_flags(cuda_cc.version(), _arches),
        language: 'cuda',
      )
    endif
  endif
endif


# Find support for ROCm performance monitoring
hip_dep = dependency(
  'hip',
  method: 'cmake',
  modules: ['hip::amdhip64'],
  required: get_option('rocm'),
  version: '>=5.1',
  include_type: 'system',
)
hsa_dep = dependency(
  'hsa-runtime64',
  method: 'cmake',
  required: get_option('rocm'),
  include_type: 'system',
)

# Technically, ROCtracer and ROCprofiler don't ship any dependency files. It is extremely rare to
# see them outside of a complete "ROCm installation" that also includes hip and hsa (above), but for
# hardness we also support getting them via compiler checks.
roctracer_dep = dependency('', required: false)
if get_option('rocm').allowed()
  roctracer_dep = dependency(
    'ROCtracer',
    method: 'cmake',
    cmake_module_path: 'cmake',
    required: false,
    include_type: 'system',
  )
  if not roctracer_dep.found()
    _roctracer_lib = cc.find_library(
      'roctracer64',
      has_headers: ['roctracer/roctracer_hip.h'],
      required: false,
    )
    if _roctracer_lib.found()
      roctracer_dep = declare_dependency(
        dependencies: _roctracer_lib,
        version: '@0@.@1@'.format(
          cc.get_define(
            'ROCTRACER_VERSION_MAJOR',
            prefix: '#include <roctracer/roctracer.h>',
            dependencies: _roctracer_lib,
          ),
          cc.get_define(
            'ROCTRACER_VERSION_MINOR',
            prefix: '#include <roctracer/roctracer.h>',
            dependencies: _roctracer_lib,
          ),
        ),
      )
    endif
  endif
  if not roctracer_dep.found()
    roctracer_dep = dependency(
      'ROCtracer',
      method: 'system',
      required: get_option('rocm'),
      include_type: 'system',
    )
  endif
endif

rocprofiler_dep = dependency('', required: false)
if get_option('rocm').allowed()
  rocprofiler_dep = dependency(
    'ROCprofiler',
    method: 'cmake',
    cmake_module_path: 'cmake',
    required: false,
    include_type: 'system',
  )
  if not rocprofiler_dep.found()
    _rocprofiler_lib = cc.find_library(
      'rocprofiler64',
      has_headers: ['rocprofiler/rocprofiler.h'],
      required: false,
    )
    if _rocprofiler_lib.found()

      # As a very special case, we also need the path to a metrics.xml file in the ROCprofiler
      # installation. We don't have any paths at hand to find it otherwise, so we require the user
      # to specify the path via a project option. (We use an option to match the `c*_args` used for
      # the compiler checks, usually one would add these options to a machine file.)
      if get_option('rocprofiler_metrics_xml') == ''
        error(
          '-Drocprofiler_metrics_xml option must be set to use compiler fallback for ROCprofiler',
        )
      endif
      _metrics_xml = fs.expanduser(get_option('rocprofiler_metrics_xml'))
      if not fs.exists(_metrics_xml)
        error(
          f'-Drocprofiler_metrics_xml set to an invalid path: no such file: @_metrics_xml@',
        )
      endif
      rocprofiler_dep = declare_dependency(
        dependencies: _rocprofiler_lib,
        variables: {'metrics_xml': _metrics_xml},
        version: '@0@.@1@'.format(
          cc.get_define(
            'ROCPROFILER_VERSION_MAJOR',
            prefix: '#include <rocprofiler/rocprofiler.h>',
            dependencies: _rocprofiler_lib,
          ),
          cc.get_define(
            'ROCPROFILER_VERSION_MINOR',
            prefix: '#include <rocprofiler/rocprofiler.h>',
            dependencies: _rocprofiler_lib,
          ),
        ),
      )
    endif
  endif
  if not rocprofiler_dep.found()
    rocprofiler_dep = dependency(
      'ROCprofiler',
      method: 'system',
      required: get_option('rocm'),
      include_type: 'system',
    )
  endif
endif

_rocm_subdeps = [rocprofiler_dep, roctracer_dep, hip_dep, hsa_dep]
rocm_dep = declare_dependency(dependencies: _rocm_subdeps, version: hip_dep.version())
rocprofiler_path = disabler()
foreach dep : _rocm_subdeps
  if not dep.found()
    rocm_dep = dependency('', required: false)
    break
  endif
endforeach
if rocm_dep.found()
  add_project_arguments(
    '-DUSE_ROCM',
    '-DHPCRUN_SS_AMD',
    '-D__HIP_PLATFORM_AMD__',
    '-D__HIP_PLATFORM_HCC__',
    language: ['c', 'cpp'],
  )
  if cc.has_member(
    'roctracer_record_t',
    'kernel_name',
    prefix: '#include <roctracer/roctracer_hip.h>',
    dependencies: rocm_dep,
  )
    add_project_arguments('-DHAVE_ROCM_ACTIVITY_KERNEL_NAME', language: ['c', 'cpp'])
  endif
  rocprofiler_path = fs.parent(
    rocprofiler_dep.get_variable(
      cmake: 'ROCprofiler_METRICS_XML',
      internal: 'metrics_xml',
    ),
  )
endif
if false  # get_option('papi_rocm')  # --enable-papi-c-rocm
  add_project_arguments('-DHPCRUN_SS_PAPI_C_ROCM', language: ['c', 'cpp'])
endif


# Find HIP, the language used to test the ROCm support. Meson doesn't support it as a language, but
# we emulate Meson's basic behavior for languages.
# Similar to CUDA, we combine cpp_*args and hip_*args for the command lines here.
has_hip = false
if rocm_dep.found()
  _hip_cc = find_program('hip', 'hipcc', required: get_option('extended_tests'))
  has_hip = _hip_cc.found()
  if has_hip
    hip = generator(
      _hip_cc,
      arguments: [
        get_option('cpp_args'),
        get_option('hip_args'),
        '-MD',
        '-MQ',
        '@OUTPUT@',
        '-MF',
        '@DEPFILE@',
        '-o',
        '@OUTPUT@',
        '-c',
        '@INPUT@',
      ],
      output: '@PLAINNAME@.o',
      depfile: '@PLAINNAME@.o.d',
    )
    hip_ld = [
      _hip_cc,
      get_option('cpp_link_args'),
      get_option('hip_link_args'),
      '-o',
      '@OUTPUT0@',
      '@INPUT@',
    ]
  endif
endif


# Find OpenCL support. We only need the headers, but there are many ways to acquire them.
opencl_dep = dependency('', required: false)
if get_option('opencl').allowed()
  _opencl_ver = '>=2.1'

  # - As of OpenCL >=2023.02.06 (found in Debian >=12, Ubuntu >=23.04, and Fedora) the headers can
  #   be accessed via pkg-config as OpenCL-Headers.pc.
  # - Before that, since OpenCL >=2020.12.18 (found in Debian >=10, Ubuntu >=20.04, and RHEL >=9)
  #   the headers can be accessed via CMake as OpenCLHeaders-config.cmake.
  opencl_dep = dependency(
    'OpenCL-Headers',
    'OpenCLHeaders',
    required: false,
    version: _opencl_ver,
    include_type: 'system',
  )

  # For older distros, CMake >=3.1 ships a FindOpenCL.cmake script, but it requires a full OpenCL
  # installation and not just the headers. If that's all we got, use it and cut out the link
  # flags so we only get the headers.
  if not opencl_dep.found()
    opencl_dep = dependency(
      'OpenCL',
      required: false,
      version: _opencl_ver,
      include_type: 'system',
    )
    opencl_dep = opencl_dep.partial_dependency(includes: true, compile_args: true)
  endif

  # For Spack builds and partial system installations, we also support the case where the header
  # is already available on the standard include paths.
  if not opencl_dep.found() and cc.has_header('CL/cl.h', required: false)
    opencl_dep = declare_dependency()
  endif

  # Fall back to dependency() to get a good error message and enable wrap fallbacks.
  if not opencl_dep.found()
    opencl_dep = dependency(
      'OpenCL-Headers',
      required: get_option('opencl'),
      include_type: 'system',
    )
  endif
endif
if opencl_dep.found()
  add_project_arguments(
    '-DENABLE_OPENCL',
    '-DHPCRUN_SS_OPENCL',
    '-DCL_TARGET_OPENCL_VERSION=210',
    '-DCL_USE_DEPRECATED_OPENCL_1_2_APIS',
    language: ['c', 'cpp'],
  )
endif

# Find a full OpenCL installation. We need this to run some of the tests.
opencl_full_dep = dependency('', required: false)
if opencl_dep.found()
  opencl_full_dep = dependency(
    'OpenCL',
    required: get_option('extended_tests'),
    version: _opencl_ver,
    include_type: 'system',
  )
endif


# Find support for Intel performance monitoring via Level Zero.
level0_dep = dependency('', required: false)
if get_option('level0').allowed()
  level0_dep = dependency(
    'level-zero',
    required: false,
    version: '>=1.0',
    include_type: 'system',
  )
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
        _l0_lib = cc.find_library(
          'ze_loader',
          dirs: [_l0_libdir, _l0_new_libdir],
          required: false,
        )
        if _l0_lib.found()
          message(
            'Replacing Level Zero libdir: \'@0@\' -> \'@1@\''.format(
              _l0_libdir,
              _l0_new_libdir,
            ),
          )
          level0_dep = declare_dependency(
            dependencies: [level0_dep, _l0_lib],
            link_args: ['-L@0@'.format(_l0_new_libdir)],
            version: level0_dep.version(),
          )
        endif
      endif
    endif

    # Sanity-check the modified dependency before calling it done.
    if not cc.links(
      '''
      #include <ze_api.h>
      int main() {
        zeInit(0);
        return 0;
      }
      ''',
      name: 'Level Zero sanity check',
      dependencies: level0_dep,
    )
      error(
        '''Failed to reconstruct usable Level Zero dependency! This could be for many reasons, consider:
- Checking meson-logs/meson-log.txt for the compiler error, and
- Manually fixing the libdir variable in the pkg-config file shipped by Level Zero (pkg-config --path level-zero), or
- Appending the flags to use Level Zero to (c|cpp)(_link)?_args, or
- Removing Level Zero from your PKG_CONFIG_PATH.''',
      )
    endif
  endif

  if not level0_dep.found()

    # To work around certain incomplete modulefiles, we also support the case
    # where the library and headers are already available in compiler flags.
    _l0_lib = cc.find_library('ze_loader', has_headers: ['ze_api.h'], required: false)
    if _l0_lib.found()
      _l0_ver = cc.compute_int(
        'ZE_API_VERSION_CURRENT',
        prefix: '#include <ze_api.h>',
        dependencies: _l0_lib,
        low: 0x10000,
        guess: 0x10008,
        high: 0x30000,
      )
      level0_dep = declare_dependency(
        dependencies: _l0_lib,
        version: '@0@.@1@'.format(_l0_ver / 0x10000, _l0_ver % 0x10000),
      )
    endif
  endif

  # Fall back to dependency() to get a good error message and enable wrap fallbacks
  if not level0_dep.found()
    level0_dep = dependency(
      'level-zero',
      required: get_option('level0'),
      version: '>=1.0',
      include_type: 'system',
    )
  endif
endif
if level0_dep.found()
  add_project_arguments('-DUSE_LEVEL0', '-DHPCRUN_SS_LEVEL0', language: ['c', 'cpp'])
endif

# Find support for instruction level collection, which requires IGC and IGA
_gtpin_f = get_option('gtpin').require(
  level0_dep.found(),
  error_message: 'gtpin is only available with level0',
)

igc_dep = dependency('', required: false)
if _gtpin_f.allowed()
  igc_dep = dependency('igc-opencl', required: false, include_type: 'system')

  if not igc_dep.found()

    # To work around certain incomplete deployments/modulefiles, we also support
    # the case where the headers are already available by full path in the
    # compiler flags. In this case IGC_INCLUDE_FULL_PATH is #defined and
    # downstream sources #include a longer header path when this is set.
    _ok = true
    foreach _hdr : [
      'igc.opencl.h',
      'ocl_igc_shared/executable_format/patch_list.h',
      'ocl_igc_shared/device_enqueue/DeviceEnqueueInternalTypes.h',
    ]
      if not cc.has_header(_hdr)
        _ok = false
        break
      endif
    endforeach
    if _ok
      igc_dep = declare_dependency(
        compile_args: ['-DIGC_INCLUDE_FULL_PATH'],
        version: 'unknown',
      )
    endif
  endif

  # Fall back to dependency() to get a good error message and enable wrap fallbacks
  if not igc_dep.found()
    igc_dep = dependency('igc-opencl', required: _gtpin_f, include_type: 'system')
  endif
endif
if igc_dep.found()
  add_project_arguments('-DENABLE_IGC', language: ['c', 'cpp'])
endif

iga_dep = dependency('', required: false)
if _gtpin_f.allowed()
  iga_dep = dependency(
    'IGA',
    method: 'cmake',
    cmake_module_path: 'cmake',
    required: false,
    include_type: 'system',
  )

  if not iga_dep.found()

    # To work around certain incomplete modulefiles, we also support the case where the
    # headers and library are already available in the compiler flags.
    _iga_lib = cc.find_library('iga64', has_headers: ['iga/iga.h'], required: false)
    if _iga_lib.found()
      iga_dep = declare_dependency(dependencies: _iga_lib, version: 'unknown')
    endif
  endif

  # Fall back to dependency() to get a good error message and enable wrap fallbacks
  if not iga_dep.found()
    iga_dep = dependency(
      'IGA',
      method: 'cmake',
      cmake_module_path: 'cmake',
      required: _gtpin_f,
      include_type: 'system',
    )
  endif
endif

# GTPin is a snowflake of a case, it only comes in a binary tarball with non-standard names for
# all the paths like `Include` and `Lib`. This means we can't use CMake to find it, the user
# *has* to provide it through `cpp_args` and `cpp_link_args`.
# But since that's way too much trouble in most cases, we also support pulling it from a wrap,
# when the feature is explicitly requested.
gtpin_dep = cpp.find_library(
  'gtpin',
  has_headers: ['api/gtpin_api.h', 'ged.h'],
  required: false,
)
if gtpin_dep.found()
  gtpin_dep = declare_dependency(dependencies: gtpin_dep, version: 'unknown')
else
  gtpin_dep = dependency(
    'gtpin',
    method: 'system',
    required: get_option('gtpin'),
    include_type: 'system',
  )
endif
if gtpin_dep.found()
  add_project_arguments('-DENABLE_GTPIN', '-DUSE_GTPIN', language: ['c', 'cpp'])
endif

# Find SYCL, the language we use to test Level Zero support. Meson doesn't support it as a language,
# but we emulate Meson's basic behavior for languages.
# Similar to CUDA, we combine cpp_*args and hip_*args for the command lines here.
has_sycl = false
if level0_dep.found() and get_option('extended_tests').allowed()

  # Path 1: If "sycl" has been registered as a binary in the machine file, use that.
  _sycl_cc = find_program('sycl', required: false)
  _sycl_args = []
  _sycl_link_args = []
  has_sycl = _sycl_cc.found()
  sycl_is_cpp = false

  # Path 2: If the C++ compiler supports compiling SYCL code, possibly with an extra flag, use that.
  # This path is a bit different because we use Meson's native C++ support instead of implementing
  # our own mess on top of things.
  if not has_sycl
    foreach sycl_dep : [
      declare_dependency(
        compile_args: [get_option('sycl_args')],
        link_args: [get_option('sycl_link_args')],
      ),
      declare_dependency(
        compile_args: ['-fsycl', get_option('sycl_args')],
        link_args: ['-fsycl', get_option('sycl_link_args')],
      ),
    ]
      if cpp.compiles(files('meson/sanity.sycl.cpp'), dependencies: sycl_dep)
        has_sycl = true
        sycl_is_cpp = true
        break
      endif
    endforeach
  endif

  # Path 3: Search for a known compiler that supports SYCL. These logics are tuned based on the
  # history and quirks of the compilers being attempted.
  if not has_sycl
    _icpx = find_program('icpx', required: false)
    if _icpx.found()
      run_command(
        python,
        files('meson/in-tmpdir.py'),
        _icpx,
        '-fsycl',
        '-c',
        '-o',
        'sanity.sycl.cpp.o',
        files('meson/sanity.sycl.cpp'),
        check: true,
      )
      _sycl_cc = _icpx
      _sycl_args = ['-fsycl']
      _sycl_link_args = ['-fsycl']
      has_sycl = true
    endif
  endif
  if not has_sycl
    _dpcpp = find_program('dpcpp', required: false)
    if _dpcpp.found()
      run_command(
        python,
        files('meson/in-tmpdir.py'),
        _dpcpp,
        '-c',
        '-o',
        'sanity.sycl.cpp.o',
        files('meson/sanity.sycl.cpp'),
        check: true,
      )
      _sycl_cc = _dpcpp
      _sycl_args = []
      _sycl_link_args = []
      has_sycl = true
    endif
  endif

  # Regardless of what route we take, if we need SYCL and don't have it, error out.
  if get_option('extended_tests').enabled() and not has_sycl
    error('Unable to find a SYCL compiler!')
  endif

  if has_sycl and not sycl_is_cpp
    sycl = generator(
      _sycl_cc,
      arguments: [
        _sycl_args,
        get_option('cpp_args'),
        get_option('sycl_args'),
        '-MD',
        '-MQ',
        '@OUTPUT@',
        '-MF',
        '@DEPFILE@',
        '-o',
        '@OUTPUT@',
        '-c',
        '@INPUT@',
      ],
      output: '@PLAINNAME@.o',
      depfile: '@PLAINNAME@.o.d',
    )
    sycl_ld = [
      _sycl_cc,
      _sycl_link_args,
      get_option('cpp_link_args'),
      get_option('sycl_link_args'),
      '-o',
      '@OUTPUT0@',
      '@INPUT@',
    ]
  endif
endif


# Collect features from the compiler itself
math_dep = cc.find_library('m')
threads_dep = dependency('threads')
openmp_dep = dependency('openmp')
dl_dep = dependency('dl')
rt_dep = cc.find_library('rt')
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


# Expose the results of various compile-time checks
add_project_arguments(
  '-DSIZEOF_VOIDP=@0@'.format(
    cc.compute_int('sizeof(void*)', low: 1, high: 64, guess: 8),
  ),
  language: ['c', 'cpp'],
)

if host_machine.cpu_family() == 'x86'
  add_project_arguments('-DHOST_CPU_x86', language: ['c', 'cpp'])
elif host_machine.cpu_family() == 'x86_64'
  add_project_arguments('-DHOST_CPU_x86_64', language: ['c', 'cpp'])
elif host_machine.cpu_family() == 'ia64'
  add_project_arguments('-DHOST_CPU_IA64', language: ['c', 'cpp'])
elif host_machine.cpu_family() == 'ppc64'
  add_project_arguments('-DHOST_CPU_PPC', language: ['c', 'cpp'])
elif host_machine.cpu_family() == 'aarch64'
  add_project_arguments('-DHOST_CPU_ARM64', language: ['c', 'cpp'])
endif

if host_machine.endian() == 'big'
  add_project_arguments('-DHOST_BIG_ENDIAN', language: ['c', 'cpp'])
elif host_machine.endian() == 'little'
  add_project_arguments('-DHOST_LITTLE_ENDIAN', language: ['c', 'cpp'])
endif

if cc.has_header_symbol('byteswap.h', 'bswap_64')
  add_project_arguments('-DUSE_SYSTEM_BYTESWAP', language: ['c', 'cpp'])
endif
if cc.has_header_symbol(
  'linux/futex.h',
  'SYS_futex',
  prefix: ['#include <sys/syscall.h>', '#include <unistd.h>'],
)
  add_project_arguments('-DHAVE_FUTEX_H', language: ['c', 'cpp'])
endif

if cc.has_header_symbol('time.h', 'CLOCK_REALTIME') and cc.has_header_symbol(
  'signal.h',
  'SIGEV_THREAD_ID',
)
  add_project_arguments('-DENABLE_CLOCK_REALTIME', language: ['c', 'cpp'])
  if cc.has_header_symbol('time.h', 'CLOCK_THREAD_CPUTIME_ID')
    add_project_arguments('-DENABLE_CLOCK_CPUTIME', language: ['c', 'cpp'])
  endif
endif

has_perf_events = cc.has_type(
  'struct perf_event_attr',
  prefix: [
    '#include <linux/perf_event.h>',
    '#include <linux/hw_breakpoint.h>',
    '#include <sys/syscall.h>',
    '#include <unistd.h>',
  ],
)
if has_perf_events
  add_project_arguments('-DHPCRUN_SS_LINUX_PERF', language: ['c', 'cpp'])
endif


# Inject some strings into the build
add_project_arguments(
  '-DHPCTOOLKIT_INSTALL_PREFIX="@0@"'.format(
    get_option('prefix').replace('\\', '\\\\').replace('"', '\\"'),
  ),
  language: ['c', 'cpp'],
)


subdir('doc')
subdir('src')
subdir('tests')
