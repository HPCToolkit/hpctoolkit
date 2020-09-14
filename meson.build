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

project('hpctoolkit', ['c', 'cpp'],
        version: '2020.08-develop',
        license: 'MIT',
        meson_version: '>=0.54.0',
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
python = import('python').find_installation('python3')

# The hpc* launch scripts don't work if the libdir or libexecdir are altered
# from their defaults.
# FIXME: Configure the scripts with the proper values for these.
if get_option('libdir') != 'lib' or get_option('libexecdir') != 'libexec'
  warning('libdir or libexecdir is altered from default, '
          + 'launch scripts will not function!')
endif

# TODO: Hunt down the Git version and Spack spec. Generally not needed.
# (For Git, use vcs_tag, probably. Might not work for all cases.)

# TODO: Autotools issues a whole bunch of sanity checks for headers and types.
# In particular:
#  - stdlib.h, stdarg.h, string.h, float.h
#  - unistd.h, inttypes.h (but only for C++?)
#  - void* (for C)
#  - cxxabi.h, cstdlib, cstddef, cstdio, cassert, cerrno, cstdarg, cstring,
#    cmath, csetjmp, csignal, cctype, climits, cfloat, clocale (for C++)
#  - sizeof void* (but only for C++?)
#  - ushort, uint, ulong
#  - pthread.h (more than preprocessor)
# None of these are probably nessesary on a modern system.

# TODO: Autotools has a long switch statement configuring the unwinder usage
# and host flags. Some of it is handled by Meson
# (host_machine.system() and .cpu_family()), but the rest is tricky.

# Warn if we're not running a GNU compiler with a sufficient version
if meson.get_compiler('c').get_id() != 'gcc' \
   or meson.get_compiler('cpp').get_id() != 'gcc'
  warning('Compilers other than GCC are not supported!')
endif
# TODO: Handle getting the GCC version and checking that

# TODO: Fix the compiler wrapper thingy for Spack builds

# TODO: Find libstdc++, for hpcfnbounds apparently. Not used for hpcfnbounds2.

# Enable extended C++ alignment if possible, for dyninst
if cpp.get_argument_syntax() == 'gcc'
  add_project_arguments(cpp.get_supported_arguments(['-faligned-new']),
                        language: 'cpp')
endif

# Enable OpenMP.
# TODO: Figure out how to handle this properly. The original controls its usage
# via a test for Dyninst support, but it doesn't appear to compile here without
# having it enabled. Maybe something to do with the library?
openmp_dep = dependency('OpenMP', required: false)
# TODO: Handle finding the OpenMP directory.

# Enable MPI (for C and C++).
# NOTE: Autotools has a separate search system for Cray and Blue Gene, to get
# around a Spack bug. We trust Meson's search functionality and ability to
# override the issue in a Spack recipe.
mpi_c_dep = dependency('MPI', language: 'c', required: false)
mpi_cpp_dep = dependency('MPI', language: 'cpp', required: false)

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
  spack = find_program(spack, version: '>=0.15.0', native: true)
endif

# TODO: Sanity-check that Spack's compiler and ours are compatible.
# Is this really nessesary? The .a, .o and .so formats have been stable
# for a very long time.

# Get all the bits for our dependencies, hunt down the parts and construct
# the dependency objects. We do the searches within Meson to allow the automatic
# addition into the build_rpath (not the install_rpath, as per the usual).
_spack_cache = {}
_deps = {
  'libbfd': { 'propstems': ['bfd', 'binutils'], 'spack': 'binutils',
              'libs': ['bfd'], 'headers': ['bfd.h'],
            },
  'libunwind': { 'propstems': ['libunwind'], 'spack': 'libunwind', 'dependency': 'libunwind',
                 'libs': ['unwind'], 'headers': ['libunwind.h'],
               },
  'libunwind_static': { 'propstems': ['libunwind'], 'spack': 'libunwind', 'dependency': 'libunwind',
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
  'libelf_static': { 'propstems': ['libelf', 'elfutils'], 'spack': 'elfutils', 'dependency': 'libelf',
                     'libs': ['elf'], 'headers': ['libelf.h'], 'static': true,
                     'extra_deps': ['zlib_static_dep'],
                   },
  'libdw': { 'propstems': ['libdw', 'elfutils'], 'spack': 'elfutils', 'dependency': 'libdw',
             'libs': ['dw'], 'headers': ['elfutils/libdw.h'],
           },
  'libdwarf': { 'propstems': ['libdwarf'], 'spack': 'libdwarf',
                'libs': ['dwarf'], 'headers': ['libdwarf.h'],
              },
  'liblzma': { 'propstems': ['liblzma', 'lzma'], 'spack': 'xz', 'dependency': 'liblzma',
               'libs': ['lzma'], 'headers': ['lzma.h'],
             },
  'liblzma_static': { 'propstems': ['liblzma', 'lzma'], 'spack': 'xz', 'dependency': 'liblzma',
                      'libs': ['lzma'], 'headers': ['lzma.h'], 'static': true,
                    },
  'mbedtls_crypto': { 'propstems': ['mbedtls'], 'spack': 'mbedtls',
                      'libs': ['mbedcrypto'], 'headers': ['mbedtls/md5.h'],
                      'static': true,
                    },
  'gotcha': { 'propstems': ['gotcha'], 'spack': 'gotcha',
              'libs': ['gotcha'], 'headers': ['gotcha/gotcha.h'],
            },
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
  'rocm_headers': { 'propstems': ['rocm'],
                    'libs': [], 'headers': ['hip/hip_runtime.h', 'roctracer/roctracer.h'],
                  },

  'libmonitor': { 'propstems': ['libmonitor', 'monitor'], 'spack': 'libmonitor',
                  'spack_spec': '+hpctoolkit',
                  'libs': ['monitor'], 'headers': ['monitor.h'],
                },
  'libmonitor_wrap': { 'propstems': ['libmonitor', 'monitor'], 'spack': 'libmonitor',
                       'spack_spec': '+hpctoolkit',
                       'libs': ['monitor_wrap'], 'headers': ['monitor.h'],
                       'static': true,
                     },

  'tbb': { 'propstems': ['tbb'], 'spack': 'intel-tbb',
           'libs': ['tbb', 'tbbmalloc'], 'headers': ['tbb/concurrent_hash_map.h'],
           'cc': cpp,
         },
  'tbb_mproxy': { 'propstems': ['tbb'], 'spack': 'intel-tbb',
                  'libs': ['tbbmalloc_proxy'], 'headers': ['tbb/concurrent_hash_map.h'],
                  'cc': cpp,
                },
  'boost': { 'propstems': ['boost'], 'spack': 'boost', 'dependency': 'boost',
             'spack_spec': 'visibility=global',
             'libs': ['boost_graph', 'boost_regex', 'boost_system'],
             'headers': ['boost/shared_ptr.hpp'],
             'components': ['graph', 'regex', 'system'],
             'cc': cpp,
           },
  'dyninst': { 'propstems': ['dyninst'], 'spack': 'dyninst',
               'libs': ['parseAPI', 'instructionAPI', 'symtabAPI', 'dynDwarf',
                        'dynElf', 'common'],
               'headers': ['Symtab.h'], 'cc': cpp,
               'extra_incdeps': ['tbb_dep', 'boost_dep'],
             },
  'xerces': { 'propstems': ['xerces'], 'spack': 'xerces-c', 'dependency': 'xerces-c',
              'libs': ['xerces-c'], 'headers': ['xercesc/sax2/SAX2XMLReader.hpp'],
              'cc': cpp,
            },

  # NOTE: BEGIN the ones that currently aren't referenced anywhere.
  # Remove the x_ tag and move above this line when these are in use.
  'x_libiberty': { 'propstems': ['iberty', 'binutils'], 'spack': 'libiberty',
                   'libs': ['iberty'], 'headers': ['libiberty/libiberty.h'],
                 },
  'x_bzip': { 'propstems': ['bzip2', 'bzip'], 'spack': 'bzip2', 'dependency': 'BZip2',
              'libs': ['bz2'], 'headers': ['bzlib.h'],
            },
  'x_valgrind_headers': { 'propstems': ['valgrind'], 'spack': 'valgrind', 'dependency': 'valgrind',
                          'libs': [], 'headers': ['valgrind.h', 'helgrind.h'],
                        },
  # NOTE: END the ones that currently aren't referenced anywhere.
}
foreach var, dep : _deps
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
    set_variable(var+'_dep', dependency('', required: false))
    set_variable(var+'_dep_libs', [])
    set_variable(var+'_dep_idirs', [])
    summary(var, false, section: 'Dependencies', bool_yn: true)
    continue
  endif

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
    elif meson.get_external_property(stem+'_root', '') != ''
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
    set_variable(var+'_dep', dep.get('root_use_dep', false) ? {'skip_to_dep': true} : {
      'summary': meson.get_external_property(stem+'_root', _dep_ldirs[0]),
      'idirs': _dep_idirs, 'ldirs': _dep_ldirs,
    })
    break
  endforeach

  # If a Spack is available and usable for this dependency, use it to scare up some paths
  if not is_variable(var+'_dep') and is_variable('spack') and dep.has_key('spack') \
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
        set_variable(var + '_dep', dependency('', required: false))
        summary(var, false, section: 'Dependencies', bool_yn: true)
        continue
      endif

      _spack_paths = _spack_paths.stdout().strip().split('\0')
      _dep_summary = ''
      _dep_idirs = []
      _dep_ldirs = []
      foreach part : _spack_paths
        if _dep_summary == ''
          _dep_summary = part
        elif part.startswith('L')
          _dep_ldirs += (part / '').strip('L')
        elif part.startswith('I')
          _dep_idirs += (part / '').strip('I')
        else
          error('Invalid output from spack-paths.py: ' + part)
        endif
      endforeach
      if _dep_idirs.length() == 0 or _dep_ldirs.length() == 0
        error('Invalid output from spack-paths.py, missing elements')
      endif

      # Throw the variables in the cache for later
      _spack_cache += { dep['spack']: {
        'summary': _dep_summary, 'idirs': _dep_idirs, 'ldirs': _dep_ldirs,
      }}
    endif
    set_variable(var+'_dep', _spack_cache[dep['spack']])
  endif

  # Fetch the base dependencies needed to make this all work
  _dep_idirs = []
  _dep_xlibs = []
  foreach exdep : dep.get('extra_deps', [])
    _dep_idirs += get_variable(exdep+'_idirs')
    _dep_xlibs += get_variable(exdep+'_libs')
  endforeach
  foreach exdep : dep.get('extra_incdeps', [])
    _dep_idirs += get_variable(exdep+'_idirs')
    foreach d : get_variable(exdep+'_libs')
      _dep_xlibs += d.partial_dependency(compile_args: true, includes: true)
    endforeach
  endforeach

  # If we reach this point, we may have to do the actual search.
  _dep_cc = dep.get('cc', cc)
  _dep_libs = []
  if is_variable(var+'_dep') and not get_variable(var+'_dep').get('skip_to_dep', false)
    _dep_paths = get_variable(var+'_dep')

    # In the end, these are the include directories that we will depend on.
    _dep_idirs += include_directories(_dep_paths['idirs'])
    set_variable(var+'_dep_idirs', _dep_idirs)

    # Hunt down the libraries on the given paths. Also check for the headers.
    foreach lib : dep['libs']
      if dep.has_key('static')
        _dep_libs += _dep_cc.find_library(lib,
          dirs: _dep_paths['ldirs'], required: true, static: dep['static'],
          has_headers: dep['headers'], header_include_directories: _dep_idirs)
      else
        _dep_libs += _dep_cc.find_library(lib,
          dirs: _dep_paths['ldirs'], required: true,
          has_headers: dep['headers'], header_include_directories: _dep_idirs)
      endif
    endforeach
    if dep['libs'].length() == 0
      foreach h : dep['headers']
        _dep_libs += cc.has_header(h, include_directories: _dep_idirs) ? [] : disabler()
      endforeach
    endif
    _dep_libs += _dep_xlibs
    set_variable(var+'_dep_libs', _dep_libs)

    # Stitch together a final dependency to represent the entire mess.
    set_variable(var+'_dep', declare_dependency(
      dependencies: _dep_libs, include_directories: _dep_idirs,
    ))
    summary(var, _dep_paths['summary'], section: 'Dependencies')
  elif not dep.has_key('dependency')
    # Repeat of the above, but only using the system paths.
    set_variable(var+'_dep_idirs', _dep_idirs)
    foreach lib : dep['libs']
      if dep.has_key('static')
        _dep_libs += _dep_cc.find_library(lib, required: false,
          static: dep['static'],
          has_headers: dep['headers'], header_include_directories: _dep_idirs)
      else
        _dep_libs += _dep_cc.find_library(lib, required: false,
          has_headers: dep['headers'], header_include_directories: _dep_idirs)
      endif
    endforeach
    if dep['libs'].length() == 0
      foreach h : dep['headers']
        _dep_libs += cc.has_header(h, include_directories: _dep_idirs) ? [] : disabler()
      endforeach
    endif
    _dep_libs += _dep_xlibs
    set_variable(var+'_dep_libs', _dep_libs)
    set_variable(var+'_dep', declare_dependency(
      dependencies: _dep_libs, include_directories: _dep_idirs,
    ))
    summary(var, get_variable(var+'_dep').found() ? '(system)' : false,
            section: 'Dependencies', bool_yn: true)
  else
    # Use the standard dependency logic to pull from the system.
    # Unlike the other paths, we assume the standard logic can handle the
    # internal dependencies without our help.
    if dep.has_key('static')
      set_variable(var+'_dep', dependency(dep['dependency'],
        required: is_variable(var+'_dep'), static: dep['static'],
        modules: dep.get('modules', []),
        components: dep.get('components', [])))
    else
      set_variable(var+'_dep', dependency(dep['dependency'],
        required: is_variable(var+'_dep'),
        modules: dep.get('modules', []),
        components: dep.get('components', [])))
    endif
    set_variable(var+'_dep_idirs', [])
    set_variable(var+'_dep_libs', [get_variable(var+'_dep')])
    if get_variable(var+'_dep').found()
      summary(var, get_variable(var+'_dep').get_variable(
        cmake: get_variable(var+'_dep').name() + '_LIBRARY',
        pkgconfig: 'prefix',
        default_value: '(system)'), section: 'Dependencies')
    else
      summary(var, false, section: 'Dependencies', bool_yn: true)
    endif
  endif
endforeach

# The Valgrind dependency should only reference the headers, so remove any
# of its other bits.
x_valgrind_headers_dep = x_valgrind_headers_dep.partial_dependency(compile_args: true, includes: true)

# For every dependency (_dep) variable, add an additional _d_dep variable that
# is a disabler when the dependency is not found. Because disablers are handy.
openmp_d_dep = openmp_dep.found() ? openmp_dep : disabler()
mpi_c_d_dep = mpi_c_dep.found() ? mpi_c_dep : disabler()
mpi_cpp_d_dep = mpi_cpp_dep.found() ? mpi_cpp_dep : disabler()
foreach var, _ : _deps
  set_variable(var+'_d_dep', get_variable(var+'_dep').found() ? get_variable(var+'_dep') : disabler())
endforeach

## CUDA a special case, Meson again overrides the logic, and we don't need everything.

# There is no support for cuda/cupti_root, so error if anyone tries to use it.
if meson.get_external_property('cuda_root', '') != ''
  error('cuda_root as a property is not effective, '
        + 'use the CUDA_HOME environment variable!')
endif
if meson.get_external_property('cupti_root', '') != ''
  error('cupti_root as a property is not effective, '
        + 'use the CUDA_HOME environment variable!')
endif

# TODO: Handle Spack-provided CUDA. Or don't. Decide what to do there.

# Search for the CUDA toolkit
cuda_dep = dependency('CUDA', required: false, modules: ['cupti'])
cuda_d_dep = cuda_dep.found() ? cuda_dep : disabler()

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
