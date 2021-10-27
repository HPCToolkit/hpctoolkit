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

project('hpctoolkit', ['c', 'cpp'],
        version: '2021.10.15-master',
        license: 'MIT',
        meson_version: '>=0.57.0',
        default_options: ['buildtype=release', 'b_staticpic=false',
                          'libdir=lib', 'libexecdir=libexec',
                          'c_std=gnu11', 'cpp_std=gnu++11'])

hpctoolkit_config = configuration_data()
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
fs = import('fs')
sourceset = import('sourceset')
pkgconfig = import('pkgconfig')

# Various bits we need require the use of Python 3, so get it early
python = find_program('python3', 'python', version: '>=3.0.0')

# The hpc* launch scripts don't work if the libdir or libexecdir are altered
# from their defaults.
# FIXME: Configure the scripts with the proper values for these.
if get_option('libdir') != 'lib' or get_option('libexecdir') != 'libexec'
  warning('libdir or libexecdir is altered from default, '
          + 'launch scripts will not function!')
endif

# Warn if we're not running a GNU compiler with a sufficient version
if cc.get_id() != 'gcc' or cpp.get_id() != 'gcc'
  warning('Compilers other than GCC are not supported!')
elif cc.version().version_compare('<5') or cpp.version().version_compare('<5')
  warning('Compilers older than GCC 5.x are not supported!')
endif

# Enable extended C++ alignment if possible, for dyninst
if cpp.get_argument_syntax() == 'gcc'
  add_project_arguments(cpp.get_supported_arguments(['-faligned-new']),
                        language: 'cpp')
endif

# Handle getting the Spack we'll use for our business
if get_option('spack') != '' or meson.get_external_property('spack', '') != ''
  # Getting cross-compiling bits from Spack is very likely a bad idea
  assert(not meson.is_cross_build(),
    'Spack doesn\'t support cross-compiling, use explicit *_roots instead!')

  # Warn if both the option and property are set, and error if they differ
  if get_option('spack') != '' and meson.get_external_property('spack', '') != ''
    if get_option('spack') == meson.get_external_property('spack', '')
      warning('Both the option and the property `spack\' are set, '
              + 'propably only the property should be set.')
    else
      error('Both the option and the property `spack\' are set and differ, '
            + 'which Spack did you want?')
    endif
  endif

  if meson.get_external_property('spack', '') != ''
    spack = fs.expanduser(meson.get_external_property('spack', ''))
    _spack_mode = '--unique'
  else
    spack = fs.expanduser(get_option('spack'))
    _spack_mode = '--any'
  endif
  if spack.contains('/')
    assert(fs.is_absolute(spack), 'Spack path `' + spack + '\' must be absolute!')
    assert(fs.is_file(spack),
           'Spack path `' + spack + '\' must be a file (i.e. .../bin/spack)!')
  endif
  spack = find_program(spack, native: true)
endif

# Get all the bits for our dependencies, hunt down the parts and construct
# the dependency objects. We do the searches within Meson to allow the automatic
# addition into the build_rpath (not the install_rpath, as per the usual).
_spack_cache = {}
_deps = {
  'libbfd': { 'propstems': ['bfd', 'binutils'], 'spack': 'binutils',
              'libs': ['bfd'], 'headers': ['bfd.h'],
              'check_headers': true,
            },
  'libiberty': { 'propstems': ['iberty', 'binutils'], 'spack': 'binutils',
                 'libs': ['iberty'], 'headers': ['libiberty/demangle.h'],
                 'static': true,
               },
  # NOTE: Only static version to support internalization
  'libunwind_static': { 'propstems': ['libunwind'], 'spack': 'libunwind',
                        'spack_spec': '+pic',
                        'libs': ['unwind'], 'headers': ['libunwind.h'],
                        'static': true,
                      },
  'zlib': { 'propstems': ['zlib'], 'spack': 'zlib', 'dependency': 'zlib',
            'libs': ['z'], 'headers': ['zlib.h'],
          },
  'zlib_static': { 'propstems': ['zlib'], 'spack': 'zlib', 'dependency': 'zlib',
                   'libs': ['z'], 'headers': ['zlib.h'], 'static': true,
                 },
  'libelf': { 'propstems': ['libelf', 'elfutils'], 'spack': 'elfutils', 'dependency': 'libelf',
              'libs': ['elf'], 'headers': ['libelf.h'],
            },
  'libdw': { 'propstems': ['libdw', 'elfutils'], 'spack': 'elfutils', 'dependency': 'libdw',
             'libs': ['dw'], 'headers': ['elfutils/libdw.h'],
           },
  'libdwarf': { 'propstems': ['libdwarf'], 'spack': 'libdwarf',
                'libs': ['dwarf'], 'headers': ['libdwarf.h'],
              },
  'liblzma': { 'propstems': ['liblzma', 'lzma'], 'spack': 'xz', 'dependency': 'liblzma',
               'spack_spec': '+pic',
               'libs': ['lzma'], 'headers': ['lzma.h'],
             },
  # NOTE: no dependency() option to support internalization
  'liblzma_static': { 'propstems': ['liblzma', 'lzma'], 'spack': 'xz',
                      'spack_spec': '+pic',
                      'libs': ['lzma'], 'headers': ['lzma.h'], 'static': true,
                    },
  'mbedtls_crypto': { 'propstems': ['mbedtls'], 'spack': 'mbedtls',
                      'libs': ['mbedcrypto'], 'headers': ['mbedtls/md5.h'],
                      'static': true,
                    },
  # 'gotcha': { 'propstems': ['gotcha'], 'spack': 'gotcha',
  #             'libs': ['gotcha'], 'headers': ['gotcha/gotcha.h'],
  #           },
  'papi': { 'propstems': ['papi'], 'spack': 'papi', 'dependency': 'papi',
            'libs': ['papi'], 'headers': ['papi.h'],
          },
  'papi_static': { 'propstems': ['papi'], 'spack': 'papi', 'dependency': 'papi',
                   'libs': ['papi'], 'headers': ['papi.h'],
                   'static': true,
                 },
  'perfmon': { 'propstems': ['perfmon', 'pfm', 'papi'], 'spack': 'papi',
               'libs': ['pfm'], 'headers': ['perfmon/pfmlib.h'],
             },
  'perfmon_static': { 'propstems': ['perfmon', 'pfm', 'papi'], 'spack': 'papi',
                      'libs': ['pfm'], 'headers': ['perfmon/pfmlib.h'],
                      'static': true,
                    },
  'xed': { 'propstems': ['xed'], 'spack': 'intel-xed',
           'libs': ['xed'], 'headers': ['xed-interface.h'],
         },
  'xed_static': { 'propstems': ['xed'], 'spack': 'intel-xed',
                  'libs': ['xed'], 'headers': ['xed-interface.h'],
                  'static': true,
                },
  # TODO: Update to allow for a separated ROCM install
  'rocm-headers': { 'propstems': ['rocm'],
                    'libs': [], 'headers': ['hip/hip_runtime.h', 'roctracer/roctracer.h'],
                  },

  'libmonitor': { 'propstems': ['libmonitor', 'monitor'], 'spack': 'libmonitor',
                  'spack_spec': '+hpctoolkit',
                  'libs': ['monitor'], 'headers': ['monitor.h'],
                },
  'libmonitor_static': { 'propstems': ['libmonitor', 'monitor'], 'spack': 'libmonitor',
                       'spack_spec': '+hpctoolkit',
                       'libs': ['monitor_wrap'], 'headers': ['monitor.h'],
                       'static': true,
                     },

  'tbb': { 'propstems': ['tbb'], 'spack': 'intel-tbb',
           'libs': ['tbb', 'tbbmalloc'], 'headers': ['tbb/concurrent_hash_map.h'],
           'cc': cpp,
         },
  'boost': { 'propstems': ['boost'], 'spack': 'boost', 'dependency': 'boost',
             'spack_spec': 'visibility=global',
             'libs': ['boost_graph', 'boost_regex', 'boost_system'],
             'headers': ['boost/shared_ptr.hpp'],
             'modules': ['graph', 'regex', 'system'],
             'cc': cpp,
           },
  'xerces': { 'propstems': ['xerces'], 'spack': 'xerces-c', 'dependency': 'xerces-c',
              'libs': ['xerces-c'], 'headers': ['xercesc/sax2/SAX2XMLReader.hpp'],
              'cc': cpp,
            },
  'valgrind-headers': { 'propstems': ['valgrind'], 'spack': 'valgrind',
                        'dependency': 'valgrind',
                        'libs': [], 'headers': ['valgrind.h', 'helgrind.h'],
                        'spack_include_suffix': 'valgrind',
                      },
  'opencl-headers': { 'propstems': ['opencl'],
                      'libs': [], 'headers': ['CL/cl.h'],
                    },
  'gtpin': { 'propstems': ['gtpin'],
             'libs': ['gtpin'], 'headers': ['gtpin.h'],
           },
  'level0-headers': { 'propstems': ['level0'],
                      'libs': [], 'headers': ['level0???.h'],
                    },
  'igc': { 'propstems': ['igc'],
           'libs': ['igc', 'igc64'], 'headers': ['igc/igc.opencl.h'],
         },
  'metrics-discovery': { 'propstems': ['intel-md'],
                         'libs': ['md'], 'headers': ['metrics_discovery_api.h'],
                       },
}
_deps2 = {
  'libelf_static': { 'propstems': ['libelf', 'elfutils'], 'spack': 'elfutils', 'dependency': 'libelf',
                     'libs': ['elf'], 'headers': ['libelf.h'], 'static': true,
                     'extra_deps': ['zlib_static'],
                   },
  'dyninst': { 'propstems': ['dyninst'], 'spack': 'dyninst',
               'libs': ['parseAPI', 'instructionAPI', 'symtabAPI', 'dynDwarf',
                        'dynElf', 'common'],
               'headers': ['Symtab.h'], 'cc': cpp,
               'extra_deps': ['tbb', 'boost'],
             },
}
_resolved_deps = {}
foreach deps : [_deps, _deps2]
foreach var, dep : deps
  # If we need to, we'll add an override for dependencies that can't be found
  # via the normal methods. This is the name of said dependency.
  _dep_name = dep.get('dependency', var.split('_')[0])

  # If an explicit *_enabled property is set to false, don't do anything more.
  # We use the first explicitly set property we find.
  _dep_enabled = true
  foreach stem : dep['propstems']
    if meson.get_external_property(stem+'_enabled', '') != ''
      _dep_enabled = meson.get_external_property(stem+'_enabled')
      break
    endif
  endforeach
  if not _dep_enabled
    if dep.has_key('static')
      assert(not dep.has_key('modules'), 'TODO: \'static\' and \'modules\' are currently incompatible!')
      meson.override_dependency(_dep_name, dependency('', required: false),
        static: dep['static'])
    elif dep.has_key('modules')
      meson.override_dependency(_dep_name, dependency('', required: false),
        modules: dep['modules'])
    else
      meson.override_dependency(_dep_name, dependency('', required: false))
    endif
    _resolved_deps += {var: dependency('', required: false)}
    continue
  endif

  # The following few clauses attempt to find the search data for this dep.
  _dep_search = {}

  # If an explicit *_root or *_librarydir/*_includedir property is set, search there.
  # Boost is a special case, its _root variable is handled by Meson.
  foreach stem : dep['propstems']
    if meson.get_external_property(stem+'_librarydir', '') != '' \
       and meson.get_external_property(stem+'_includedir', '') != ''
      if not dep.get('root_use_dep', false)
        _dep_ldirs = [fs.expanduser(meson.get_external_property(stem+'_librarydir'))]
        if not fs.is_absolute(_dep_ldirs[0])
          error(stem+'_librarydir must be an absolute path')
        elif not fs.is_dir(_dep_ldirs[0])
          error(stem+'_librarydir must exist and be a directory')
        endif
        _dep_idirs = [fs.expanduser(meson.get_external_property(stem+'_includedir'))]
        if not fs.is_absolute(_dep_idirs[0])
          error(stem+'_includedir must be an absolute path')
        elif not fs.is_dir(_dep_idirs[0])
          error(stem+'_librarydir must exist and be a directory')
        endif
      endif
    elif meson.get_external_property(stem+'_librarydir', '') != '' \
         or meson.get_external_property(stem+'_includedir', '') != ''
      if not dep.get('root_use_dep', false)
        error('Both of '+stem+'_librarydir and '+stem+'_includedir must be specified')
      endif  # It'll be a hard error later down
    elif meson.get_external_property(stem+'_root', []) != []
      if not dep.get('root_use_dep', false)
        _dep_root = fs.expanduser(meson.get_external_property(stem+'_root'))
        if not fs.is_absolute(_dep_root)
          error(stem+'_root must be an absolute path')
        endif
        # Generate a set of configurable gueses, if we don't have the full answer.
        _dep_ldirs = []
        foreach suffix : meson.get_external_property('library_path_suffixes', ['lib64', 'lib'])
          if fs.is_dir(_dep_root / suffix)
            _dep_ldirs += [_dep_root / suffix]
          endif
        endforeach
        if _dep_ldirs.length() == 0
          error(stem+'_root has no existing library path canidates, check it and library_path_suffixes')
        endif
        _dep_idirs = []
        foreach suffix : meson.get_external_property('include_path_suffixes', ['include'])
          if fs.is_dir(_dep_root / suffix)
            _dep_idirs += [_dep_root / suffix]
          endif
        endforeach
        if _dep_idirs.length() == 0
          error(stem+'_root has no existing include path canidates, check it and include_path_suffixes')
        endif
      endif
    else
      continue
    endif
    if dep.has_key('root_min_ver') and meson.version().version_compare('<'+dep['root_min_ver'])
      error(stem+'_root as a property is not effective until '+dep['root_min_ver']+', please upgrade!')
    endif
    _dep_search = dep.get('root_use_dep', false) \
                  ? {'skip_to_dep': true} \
                  : {'idirs': _dep_idirs, 'ldirs': _dep_ldirs}
    break
  endforeach

  # If a Spack is available and usable for this dependency, use it to scare up some paths
  if _dep_search == {} and is_variable('spack') and dep.has_key('spack') \
     and meson.get_external_property(dep['spack']+'_spec', '') != 'system'
    # To reduce the amount of times we have to fire up Spack, check the cache.
    if not _spack_cache.has_key(dep['spack'])
      # Run our helper script, and do our best with it.
      _spack_paths = run_command(spack, 'python', files('meson/spack-query.py'),
        spack, _spack_mode, dep['spack'], dep.get('spack_spec', ''),
        meson.get_external_property(dep['spack']+'_spec', meson.get_external_property('default_spec', '')), check: true)
      if _spack_paths.stdout() == ''
        if _spack_paths.stderr().strip() != ''
          error(_spack_paths.stderr().strip())
        else
          error('Failure during Spack lookup!')
        endif
      endif
      if _spack_paths.stdout().strip() == 'SKIP'
        if _spack_paths.stderr().strip() != ''
          warning(_spack_paths.stderr().strip())
        endif
        if dep.has_key('static')
          assert(not dep.has_key('modules'), 'TODO: \'static\' and \'modules\' are currently incompatible!')
          meson.override_dependency(_dep_name, dependency('', required: false),
            static: dep['static'])
        elif dep.has_key('modules')
          meson.override_dependency(_dep_name, dependency('', required: false),
            modules: dep['modules'])
        else
          meson.override_dependency(_dep_name, dependency('', required: false))
        endif
        _resolved_deps += {var: dependency('', required: false)}
        continue
      endif

      _spack_paths = _spack_paths.stdout().strip().split('\0')
      _dep_version = 'unknown'
      _dep_idirs = []
      _dep_ldirs = []
      foreach part : _spack_paths
        if part.startswith('L')
          _dep_ldirs += part.substring(1)
        elif part.startswith('I')
          _dep_idirs += part.substring(1) / dep.get('spack_include_suffix', '')
        elif part.startswith('V')
          assert(_dep_version == 'unknown', 'multiple version values?')
          _dep_version = part.substring(1)
        elif part != ''
          error('Invalid output from spack-query.py: ' + part)
        endif
      endforeach
      if _dep_idirs.length() == 0 or _dep_ldirs.length() == 0
        error('Invalid output from spack-query.py, missing elements')
      endif

      # Throw the variables in the cache for later
      _spack_cache += {
        dep['spack']: {
          'version': 'spack: '+_dep_version,
          'idirs': _dep_idirs, 'ldirs': _dep_ldirs,
        }
      }
    endif
    _dep_search = _spack_cache[dep['spack']]
  endif

  # If we don't have an alternative with dependency(), search the system paths
  # instead. In this case don't error if we don't find it.
  if _dep_search == {} and not dep.has_key('dependency')
    _dep_search = {'idirs': [], 'ldirs': [], 'required': false}
  endif

  # At this point we have enough info to attempt to search for the dependency
  if _dep_search != {} and not _dep_search.get('skip_to_dep', false)
    # This is a rooted search: we know the paths to check for the bits we need.
    _dep_idirs = include_directories(_dep_search['idirs'])
    _dep_libs = []
    foreach lib : dep['libs']
      if dep.has_key('static')
        _dep_libs += dep.get('cc', cc).find_library(lib, disabler: true,
          has_headers: dep['headers'], header_include_directories: _dep_idirs,
          dirs: _dep_search['ldirs'], required: _dep_search.get('required', true),
          static: dep['static'])
      else
        _dep_libs += dep.get('cc', cc).find_library(lib, disabler: true,
          has_headers: dep['headers'], header_include_directories: _dep_idirs,
          dirs: _dep_search['ldirs'], required: _dep_search.get('required', true))
      endif
    endforeach
    # Special case: make sure we scan for header-only dependencies
    if dep['libs'].length() == 0
      foreach h : dep['headers']
        if not dep.get('cc', cc).has_header(h, include_directories: _dep_idirs,
                    required: _dep_search.get('required', true))
          _dep_libs += [disabler()]
        endif
      endforeach
    endif
    # Optionally ensure that the headers actually work. Catches issues early.
    if dep.get('check_headers', false)
      foreach h : dep['headers']
        if not dep.get('cc', cc).check_header(h, include_directories: _dep_idirs,
                    required: _dep_search.get('required', true))
          _dep_libs += [disabler()]
        endif
      endforeach
    endif

    # If there are any extra dependencies, make sure they get incorperated
    foreach xdep : dep.get('extra_deps', [])
      assert(_resolved_deps.has_key(xdep), 'Invalid ordering of dependencies: @0@ should come before @1@'.format(xdep, var))
      xdep = _resolved_deps[xdep]
      if is_disabler(xdep)
        if dep.has_key('static')
          _dep_libs += dependency(xdep, static: dep['static'])
        else
          _dep_libs += dependency(xdep)
        endif
      else
        _dep_libs += xdep
      endif
    endforeach

    # Stitch together a final dependncy to represent the entire mess
    # FIXME: Workaround for an issue where the compiler objects don't add
    # include_directories to their compile lines.
    _dep_flags = []
    foreach dir : _dep_search['idirs']
      _dep_flags += '-I'+dir
    endforeach
    _dep = declare_dependency(
      dependencies: _dep_libs,
      include_directories: include_directories(_dep_search['idirs']),
      compile_args: _dep_flags,
      version: _dep_search.get('version', 'unknown'),
    )
    if is_disabler(_dep)
      _dep = dependency('', required: false)
    endif
    if dep.has_key('static')
      assert(not dep.has_key('modules'), 'TODO: \'static\' and \'modules\' are currently incompatible!')
      meson.override_dependency(_dep_name, _dep, static: dep['static'])
    elif dep.has_key('modules')
      meson.override_dependency(_dep_name, _dep, modules: dep['modules'])
    else
      meson.override_dependency(_dep_name, _dep)
    endif
    _resolved_deps += {var: _dep}
  else
    _resolved_deps += {var: disabler()}
  endif
endforeach
endforeach

## Handle cases where the built-in support for MPI fails

_mpi_test_code = '''
  #include <mpi.h>
  #include <stddef.h>
  int main() {
    MPI_Init(NULL, NULL);
    MPI_Finalize();
    return 0;
  }
'''
foreach lang : ['c', 'cpp']
  if dependency('MPI', required: false, language: lang).found()
    continue
  endif

  # On some systems (Cray) MPI is integrated into the base compiler.
  _comp = meson.get_compiler(lang)
  if _comp.links(_mpi_test_code, name: 'MPI is available by default test')
    _Mver = _comp.get_define('MPI_VERSION', prefix: '#include <mpi.h>')
    _mver = _comp.get_define('MPI_SUBVERSION', prefix: '#include <mpi.h>')
    _ver = _Mver + (_mver == '' ? '' : '.') + _mver
    _ver = _ver == '' ? 'unknown' : _ver
    meson.override_dependency('MPI', declare_dependency(version: _ver), language: lang)
    continue
  endif

  # Built-in Meson doesn't support MPICH, which responds to -compile_info and -link_info.
  # Note that this is a fragile uncached test. It would be more robust to upstream support.
  _printenv = find_program('printenv')
  if lang == 'c'
    _names = [run_command(_printenv, 'MPICC').stdout().strip(), 'mpicc']
  elif lang == 'cpp'
    _names = [run_command(_printenv, 'MPICXX').stdout().strip(), 'mpicxx']
  endif
  foreach name : _names
    if name == ''
      continue
    endif
    _mpicc = find_program(name, required: false)
    if _mpicc.found()
      _c_info = run_command(_mpicc, '-compile_info')
      _l_info = run_command(_mpicc, '-link_info')
      if _c_info.returncode() == 0 and _l_info.returncode() == 0
        # Skip the first argument, it should be the compiler itself
        _c_args = ['!!sentinal!!']
        foreach arg : _c_info.stdout().strip().split(' ')
          if _c_args == ['!!sentinal!!']
            _c_args = []
          else
            _c_args += arg
          endif
        endforeach
        _l_args = ['!!sentinal!!']
        foreach arg : _l_info.stdout().strip().split(' ')
          if _l_args == ['!!sentinal!!']
            _l_args = []
          else
            _l_args += arg
          endif
        endforeach

        # Check that the dependency actually provides MPI
        _dep = declare_dependency(compile_args: _c_args, link_args: _l_args, version: 'unknown mpich-like')
        assert(_comp.links(_mpi_test_code, name: 'MPI validation check', dependencies: _dep),
               'Found something like MPICH but it doesn\' seem to work')
        meson.override_dependency('MPI', _dep, language: lang)
        _names = []
        break
      endif
    endif
  endforeach
  if _names == []
    continue
  endif
endforeach

## CUDA a special case, we use Meson's logic to handle differences between
## versions smoothly. Downside is that cuda_root doesn't function as expected.

# Only proceed if CUDA is enabled at all
if meson.get_external_property('cuda_enabled', true)
  # There is no support for cuda/cupti_root, so error if anyone tries to use it.
  if meson.get_external_property('cuda_root', '') != ''
    error('cuda_root as a property is not effective, '
          + 'use the CUDA_HOME environment variable!')
  endif
  if meson.get_external_property('cupti_root', '') != ''
    error('cupti_root as a property is not effective, '
          + 'use the CUDA_HOME environment variable!')
  endif

  # Search for the CUDA toolkit
  meson.override_dependency('CUDA', dependency('CUDA', required: get_option('cuda-monitoring'), modules: ['cupti']))
else
  assert(not get_option('cuda-monitoring').enabled(), 'cuda-monitoring is enabled but cuda_enabled is false!')
  meson.override_dependency('CUDA', dependency('', required: false))
endif

# Install the READMEs to the documentation directory, {datadir}/doc/hpctoolkit
install_data(files(
  'README.md', 'README.Install', 'README.License',
  'README.ReleaseNotes', 'README.Acknowledgments',
), install_dir: get_option('datadir') / 'doc' / meson.project_name())

# Recurse into subdirectories
subdir('lib')
subdir('src')
subdir('doc')
subdir('bin')
