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
openmp = dependency('OpenMP', required: false, disabler: true)
# TODO: Handle finding the OpenMP directory.

# Enable MPI (for C and C++).
# NOTE: Autotools has a separate search system for Cray and Blue Gene, to get
# around a Spack bug. We trust Meson's search functionality and ability to
# override the issue in a Spack recipe.
mpi_c = dependency('MPI', language: 'c', required: false, disabler: true)
mpi_cpp = dependency('MPI', language: 'cpp', required: false, disabler: true)

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

# For the vast majority of our dependencies, the main search logic is handled
# by CMake or pkg-config, so we can share the common code regarding `*_root`
# property handling and Spack handling.
# NOTE: 'name' is the pkg-config or Meson name if supported, otherwise its the
# CMake name. 'cmake_name' is always the CMake name. This allows us to only
# use one `dependency` call for each one.
_deps = {
  'libbfd': { 'name': 'BFD',
              'propstems': ['bfd', 'binutils'],
              'static_modules': 'BFD::BFD_static',
              'spack': 'binutils',
            },
  'libiberty': { 'name': 'Iberty',
                 'propstems': ['iberty', 'binutils'],
                 'spack': 'libiberty',
               },
  'bzip': { 'name': 'BZip2',
            'propstems': ['bzip2', 'bzip'],
            'spack': 'bzip2',
          },
  'dyninst': { 'name': 'Dyninst',
               'propstems': ['dyninst'],
               'modules': ['Dyninst::parseAPI', 'Dyninst::instructionAPI',
                           'Dyninst::symtabAPI', 'Dyninst::dynDwarf',
                           'Dyninst::dynElf', 'Dyninst::common'],
               'spack': 'dyninst',
             },
  'libelf': { 'name': 'libelf', 'cmake_name': 'LibElf',
              'propstems': ['libelf', 'elfutils'],
              'spack': 'elfutils',
            },
  'libdw': { 'name': 'libdw', 'cmake_name': 'LibDw',
             'propstems': ['libdw', 'elfutils'],
             'spack': 'elfutils'
           },
  'gotcha': { 'name': 'Gotcha',
              'propstems': ['gotcha'],
              'spack': 'gotcha',
            },
  'libdwarf': { 'name': 'LibDwarf',
                'propstems': ['libdwarf'],
                'spack': 'libdwarf',
              },
  'libmonitor': { 'name': 'Monitor',
                  'propstems': ['libmonitor', 'monitor'],
                  'spack': 'libmonitor',
                },
  'libunwind': { 'name': 'libunwind', 'cmake_name': 'LibUnwind',
                 'propstems': ['libunwind'],
                 'spack': 'libunwind',
               },
  'liblzma': { 'name': 'liblzma', 'cmake_name': 'LibLZMA',
               'propstems': ['liblzma', 'lzma'],
               'spack': 'xz',
             },
  'mbedtls_crypto': { 'name': 'MbedTLS',
                      'propstems': ['mbedtls'],
                      'modules': ['MbedTLS::Crypto'],
                      'spack': 'mbedtls',
                    },
  'papi': { 'name': 'PAPI',
            'propstems': ['papi'],
            'spack': 'papi',
          },
  'perfmon': { 'name': 'PFM',
               'propstems': ['perfmon', 'pfm', 'papi'],
               'spack': 'papi',
             },
  'tbb': { 'name': 'TBB',
           'propstems': ['tbb'],
           'components': ['tbb', 'tbbmalloc'],
           'modules': ['TBB::tbb', 'TBB::tbbmalloc'],
           'spack': 'intel-tbb',
         },
  'tbb_mproxy': { 'name': 'TBB',
                  'propstems': ['tbb'],
                  'components': ['tbbmalloc_proxy'],
                  'modules': ['TBB::tbbmalloc_proxy'],
                  'spack': 'intel-tbb',
                },
  'xed': { 'name': 'Xed',
           'propstems': ['xed'],
           'spack': 'intel-xed',
         },
  'xerces': { 'name': 'xerces-c', 'cmake_name': 'Xerces',
              'propstems': ['xerces'],
              'spack': 'xerces-c',
            },
  'zlib': { 'name': 'zlib', 'cmake_name': 'ZLIB',
            'propstems': ['zlib'],
            'spack': 'zlib',
          },
  'valgrind_headers': { 'name': 'valgrind', 'cmake_name': 'Valgrind',
                        'propstems': ['valgrind'],
                        'spack': 'valgrind',
                        'modules': 'Valgrind::headers',
                      },
  'rocm_headers': { 'name': 'ROCm',
            'propstems': ['rocm'],
            'modules': 'ROCm::headers',
          },
}

_cmake_force_root_args = [
  '-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE',
  '-DCMAKE_FIND_USE_CMAKE_PATH=FALSE',
  '-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE',
]

foreach var, properties : _deps
  _cmake_name = properties.get('cmake_name', properties['name'])

  # Highest priority: explicitly given in *_root
  foreach pstem : properties['propstems']
    _explicit_root = meson.get_external_property(pstem + '_root', '')
    if _explicit_root != ''
      set_variable(var + '_dep', dependency(_cmake_name,
        method: 'cmake',
        required: false, static: false, disabler: true,
        cmake_args: ['-D' + _cmake_name + '_ROOT=' + _explicit_root]
                    + _cmake_force_root_args,
        modules: properties.get('modules', []),
        components: properties.get('components', []),
        cmake_module_path: 'cmake/modules'))
      if properties.has_key('static_modules')
        set_variable(var + '_static_dep', dependency(_cmake_name,
          method: 'cmake',
          required: false, static: true, disabler: true,
          cmake_args: ['-D' + _cmake_name + '_ROOT=' + _explicit_root]
                      + _cmake_force_root_args,
          modules: properties.get('static_modules', []),
          components: properties.get('components', []),
          cmake_module_path: 'cmake/modules'))
      endif
      summary(var, _explicit_root, section: 'Dependencies')
      assert(get_variable(var + '_dep').found(),
        'Could not find ' + properties['name'] + ' from '
        + pstem + '_root property: `' + _explicit_root + '\'')
    endif
  endforeach

  # Middle priority: searchable via Spack
  if not is_variable(var + '_dep') and is_variable('spack') \
     and properties.has_key('spack')
    # The user can specify the exact spec to use for any particular package
    _explicit_spec = meson.get_external_property(properties['spack'] + '_spec', '')

    # ...But we only use it if its not "system" (or some variant thereof)
    # If spack is being used the system should never be checked unless the
    # user explicitly states. The user should be very explicit about this stuff.
    if _explicit_spec.strip().to_lower() != 'system'
      # Run our helper script, and do our best with it.
      _spack_root = run_command(python, 'cmake/spack-parse.py', spack,
                                properties['spack'], _explicit_spec)
      if _spack_root.returncode() != 0
        error(_spack_root.stderr().strip())
      elif _spack_root.stdout().strip() == ''
        warning('Spack package ' + properties['spack'] + ' not found, '
                + 'please install: ' + spack.path() + ' install '
                + properties['spack'] + ' ' + _explicit_spec)
        warning('Set the property ' + properties['spack'] + '_spec '
                + 'to \'system\' to use the system/environment instead.')
        set_variable(var + '_dep', disabler())
      else
        _spack_root = _spack_root.stdout().strip()
        assert(_spack_root != '')
        _spack_root = _spack_root.split('\n')

        set_variable(var + '_dep', dependency(_cmake_name,
          method: 'cmake',
          required: false, static: false, disabler: true,
          cmake_args: ['-D' + _cmake_name + '_ROOT=' + _spack_root[0].strip()]
                      + _cmake_force_root_args,
          modules: properties.get('modules', []),
          components: properties.get('components', []),
          cmake_module_path: 'cmake/modules'))
        if properties.has_key('static_modules')
          set_variable(var + '_static_dep', dependency(_cmake_name,
            method: 'cmake',
            required: false, static: true, disabler: true,
            cmake_args: ['-D' + _cmake_name + '_ROOT=' + _spack_root[0].strip()]
                        + _cmake_force_root_args,
            modules: properties.get('static_modules', []),
            components: properties.get('components', []),
            cmake_module_path: 'cmake/modules'))
        endif
        assert(get_variable(var + '_dep').found(),
               'Failed to find ' + properties['name'] + ' from Spack prefix '
               + '`' + _spack_root[0].strip() + '\'')
      summary(var, _spack_root[1].strip(), section: 'Dependencies')
      endif
    endif
  endif

  # Lowest priority: implicitly available
  if not is_variable(var + '_dep')
    set_variable(var + '_dep', dependency(properties['name'],
      required: false, static: false, disabler: true,
      modules: properties.get('modules', []),
      components: properties.get('components', []),
      cmake_module_path: 'cmake/modules'))
    if properties.has_key('static_modules')
      set_variable(var + '_static_dep', dependency(properties['name'],
        required: false, static: true, disabler: true,
        modules: properties.get('static_modules', []),
        components: properties.get('components', []),
        cmake_module_path: 'cmake/modules'))
    endif
    if get_variable(var + '_dep').found()
      summary(var, get_variable(var + '_dep').get_variable(
        cmake: _cmake_name + '_LIBRARY',
        pkgconfig: 'prefix',
        default_value: '(system)'), section: 'Dependencies')
    else
      summary(var, false, section: 'Dependencies', bool_yn: true)
    endif
  endif
endforeach

## Fixup Dyninst's dependency to actually work properly

# The Dyninst config file doesn't list include directories, so the dependency
# can't be used directly. Half the fix is in the Find-script, but up here
# we stitch the Dyninst dependency to its dependencies.
dyninst_all_deps = [dyninst_dep, tbb_dep]
dyninst_dep = declare_dependency(dependencies: dyninst_all_deps)

## Boost is a special case, since Meson overrides its search logic.

# Support for boost_root was added later, so error if you try to use it early.
if meson.get_external_property('boost_root', '') != '' and \
   meson.version().version_compare('<0.55.1')
  error('boost_root as a property is not effective until 0.55.1, please upgrade!')
endif

_boost_our_modules = ['graph', 'regex', 'system']
_boost_other_modules = ['atomic', 'chrono', 'date_time', 'filesystem', 'thread',
                        'timer']
if meson.get_external_property('boost_root', '') == '' and is_variable('spack') \
   and meson.get_external_property('boost_spec', '').strip().to_lower() != 'system'
  # We can't use Meson's search functionality in this case, so we just
  # assume all the bits are in very particular locations for our usage.
  _spack_root = run_command(python, 'cmake/spack-parse.py', spack,
                            'boost', meson.get_external_property('boost_spec', ''))
  if _spack_root.returncode() != 0
    error(_spack_root.stderr().strip())
  elif _spack_root.stdout().strip() == ''
    warning('Spack package boost not found, '
            + 'please install: ' + spack.path() + ' install '
            + 'boost ' + _explicit_spec)
    warning('Set the property boost_spec '
            + 'to \'system\' to use the system/environment instead.')
    boost_all_dep = dependency('', required: false)
    boost_our_dep = dependency('', required: false)
  else
    _spack_root = _spack_root.stdout().strip()
    assert(_spack_root != '')
    _spack_root = _spack_root.split('\n')
    _dirs = [_spack_root[0] / 'lib', _spack_root[0] / 'lib64', _spack_root[0] / 'lib32']

    _deps = []
    foreach m : _boost_our_modules
      _deps += cpp.find_library('boost_'+m, dirs: _dirs)
    endforeach
    boost_our_dep = declare_dependency(
      include_directories: include_directories(_spack_root[0] / 'include'),
      dependencies: _deps,
    )
    foreach m : _boost_other_modules
      _deps += cpp.find_library('boost_'+m, dirs: _dirs)
    endforeach
    boost_all_dep = declare_dependency(
      include_directories: include_directories(_spack_root[0] / 'include'),
      dependencies: _deps,
    )
    summary('boost', _spack_root[1], section: 'Dependencies')
  endif
else
  # Search using the usual methods
  boost_all_dep = dependency('Boost', required: false,
                             modules: _boost_our_modules + _boost_other_modules)
  boost_our_dep = dependency('Boost', required: false, modules: _boost_our_modules)

  if meson.get_external_property('boost_root', '') != ''
    summary('boost', meson.get_external_property('boost_root', ''),
            section: 'Dependencies')
    assert(boost_all_dep.found() and boost_our_dep.found(),
           'Could not find Boost from boost_root property: '
            + '`' + meson.get_external_property('boost_root', '') + '\'')
  else
    summary('boost', '(system)', section: 'Dependencies')
  endif
endif

## CUDA is another special case, Meson again overrides the logic.

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

# Install the READMEs to the documentation directory, {datadir}/doc/hpctoolkit
install_data(files(
  'README', 'README.License', 'README.ReleaseNotes', 'README.Acknowledgments'
), install_dir: get_option('datadir') / 'doc' / meson.project_name())

# Recurse into subdirectories
subdir('src')
