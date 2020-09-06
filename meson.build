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
        version: '2020.08',
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
# (For Git, use vcs_tag)

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
# TODO: Control using an --enable-openmp option
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

  if get_option('spack') != ''
    spack = fs.expanduser(get_option('spack'))
  else
    spack = fs.expanduser(meson.get_external_property('spack', ''))
  endif
  if spack.contains('/')
    assert(fs.is_absolute(spack), 'Spack path `' + spack + '\' must be absolute!')
    assert(fs.is_file(spack),
           'Spack path `' + spack + '\' must be a file (i.e. .../bin/spack)!')
  endif
  spack = find_program(spack, version: '>=0.15.0')
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
               'libs': ['lzma'], 'headers': ['lzma.h'],
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
                  'libs': ['monitor'], 'headers': ['monitor.h'],
                },
  'libmonitor_static': { 'propstems': ['libmonitor', 'monitor'], 'spack': 'libmonitor',
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
  'x_zlib': { 'propstems': ['zlib'], 'spack': 'zlib', 'dependency': 'zlib',
              'libs': ['z'], 'headers': ['zlib.h'],
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
      _spack_paths = run_command(python, 'meson/spack-paths.py', spack,
                                 dep['spack'], meson.get_external_property(dep['spack']+'_spec', ''))
      if _spack_paths.returncode() != 0
        error(_spack_paths.stderr().strip())
      elif _spack_paths.stdout().strip() == ''
        warning(_spack_paths.stderr().strip())
        set_variable(var + '_dep', dependency('', required: false))
        summary(var, false, section: 'Dependencies', bool_yn: true)
        continue
      else
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
      endif

      # Throw the variables in the cache for later
      _spack_cache += { dep['spack']: {
        'summary': _dep_summary, 'idirs': _dep_idirs, 'ldirs': _dep_ldirs,
      }}
    endif
    set_variable(var+'_dep', _spack_cache[dep['spack']])
  endif

  # Fetch the other dependencies that will have to be added to make this work.
  _dep_exdeps = []
  _dep_exincs = []
  foreach exdep : dep.get('extra_incdeps', [])
    if is_variable(exdep+'_idirs')
      _dep_exincs += get_variable(exdep+'_idirs')
    else
      _dep_exdeps += get_variable(exdep).partial_dependency(compile_args: true, includes: true)
    endif
  endforeach

  # If we reach this point, we may have to do the actual search.
  _dep_cc = dep.get('cc', cc)
  if is_variable(var+'_dep') and not get_variable(var+'_dep').get('skip_to_dep', false)
    _dep_paths = get_variable(var+'_dep')

    # Hunt down the libraries on the given paths.
    _dep_libs = []
    foreach lib : dep['libs']
      if dep.get('static', false)
        _dep_libs += _dep_cc.find_library(lib, dirs: _dep_paths['ldirs'], required: true, static: true)
      else
        _dep_libs += _dep_cc.find_library(lib, dirs: _dep_paths['ldirs'], required: true)
      endif
    endforeach

    # Check for the headers, and error if we don't find them. They both must
    # actually exist on a path and work when compiled.
    _dep_idirs = [include_directories(_dep_paths['idirs'])] + _dep_exincs
    foreach inc : dep['headers']
      _has_it = false
      foreach idir : _dep_paths['idirs']
        if fs.is_file(idir / inc)
          _has_it = true
          break
        endif
      endforeach
      assert(_has_it, var+' does not provide `'+inc+'\'')
      _dep_cc.check_header(inc, include_directories: _dep_idirs,
                           required: true, dependencies: _dep_exdeps)
    endforeach

    # Finally, stitch together the final dependency and set it.
    set_variable(var+'_dep', declare_dependency(
      dependencies: _dep_libs + _dep_exdeps, include_directories: _dep_idirs,
    ))
    set_variable(var+'_dep_idirs', _dep_idirs)
    set_variable(var+'_dep_ex', _dep_exdeps)
    summary(var, _dep_paths['summary'], section: 'Dependencies')
  elif not dep.has_key('dependency')
    # Repeat of the above, but with system paths instead.
    _dep_libs = []
    _dep_allfound = true
    foreach lib : dep['libs']
      if dep.get('static', false)
        _dep_libs += _dep_cc.find_library(lib, static: true, required: false)
      else
        _dep_libs += _dep_cc.find_library(lib, required: false)
      endif
      _dep_allfound = _dep_allfound and _dep_libs[-1].found()
      if not _dep_allfound
        break
      endif
    endforeach
    # Ensure the headers are usable. Last thing we want is problems there.
    if _dep_allfound
      foreach inc : dep['headers']
        _dep_allfound = _dep_allfound and _dep_cc.check_header(inc, dependencies: _dep_exdeps)
        if not _dep_allfound
          break
        endif
      endforeach
    endif
    if _dep_allfound
      set_variable(var+'_dep', declare_dependency(dependencies: _dep_libs + _dep_exdeps))
      set_variable(var+'_dep_ex', _dep_exdeps)
      summary(var, '(system)', section: 'Dependencies')
    else
      set_variable(var+'_dep', dependency('', required: false))
      set_variable(var+'_dep_ex', dependency('', required: false))
      summary(var, false, section: 'Dependencies', bool_yn: true)
    endif
    set_variable(var+'_dep_idirs', include_directories())
  else
    # Use the standard dependency logic to pull from the system
    if dep.get('static', false)
      set_variable(var+'_dep', dependency(dep['dependency'],
        required: is_variable(var+'_dep'), static: true,
        modules: dep.get('modules', []),
        components: dep.get('components', [])))
    else
      set_variable(var+'_dep', dependency(dep['dependency'],
        required: is_variable(var+'_dep'),
        modules: dep.get('modules', []),
        components: dep.get('components', [])))
    endif
    set_variable(var+'_dep_ex', get_variable(var+'_dep'))
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
mpi_c_d_dep = openmp_dep.found() ? openmp_dep : disabler()
mpi_cpp_d_dep = openmp_dep.found() ? openmp_dep : disabler()
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
  'README', 'README.License', 'README.ReleaseNotes', 'README.Acknowledgments'
), install_dir: get_option('datadir') / 'doc' / meson.project_name())

# Recurse into subdirectories
subdir('src')
