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

# If we're running on a little-endian machine, we'll have to swap bytes.
# Try to use byteswap.h if we can.
hpctoolkit_config.set('USE_SYSTEM_BYTESWAP', false)
if host_machine.endian() == 'little'
  hpctoolkit_config.set('USE_SYSTEM_BYTESWAP',
                        cc.has_header_symbol('byteswap.h', 'bswap_64'))
elif host_machine.endian() == 'big'
  # All good, native format will work for output
else
  error('Unknown endian: ' + host_machine.endian())
endif

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

# TODO: Search for Spack and override suitable dependencies.

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
            },
  'libiberty': { 'name': 'Iberty',
                 'propstems': ['iberty', 'binutils'],
               },
  'bzip': { 'name': 'BZip2',
            'propstems': ['bzip2', 'bzip'],
          },
  'dyninst': { 'name': 'Dyninst',
               'propstems': ['dyninst'],
               'modules': ['parseAPI', 'instructionAPI', 'symtabAPI',
                           'dynDwarf', 'dynElf', 'common'],
             },
  'libelf': { 'name': 'libelf', 'cmake_name': 'LibElf',
              'propstems': ['libelf', 'elfutils'],
            },
  'libdw': { 'name': 'libdw', 'cmake_name': 'LibDw',
             'propstems': ['libdw', 'elfutils'],
           },
  'gotcha': { 'name': 'Gotcha',
              'propstems': ['gotcha'],
            },
  'libdwarf': { 'name': 'LibDwarf',
                'propstems': ['libdwarf'],
              },
  'libmonitor': { 'name': 'Monitor',
                  'propstems': ['libmonitor', 'monitor'],
                },
  'libunwind': { 'name': 'libunwind', 'cmake_name': 'LibUnwind',
                 'propstems': ['libunwind'],
               },
  'liblzma': { 'name': 'liblzma', 'cmake_name': 'LibLZMA',
               'propstems': ['liblzma', 'lzma'],
             },
  'mbedtls_crypto': { 'name': 'MbedTLS',
                      'propstems': ['mbedtls'],
                      'modules': ['MbedTLS::Crypto'],
                    },
  'papi': { 'name': 'PAPI',
            'propstems': ['papi'],
          },
  'perfmon': { 'name': 'PFM',
               'propstems': ['pfm', 'papi'],
             },
  'tbb': { 'name': 'TBB',
           'propstems': ['tbb'],
           'components': ['tbb', 'tbbmalloc'],
           'modules': ['TBB::tbb', 'TBB::tbbmalloc'],
         },
  'tbb_mproxy': { 'name': 'TBB',
                  'propstems': ['tbb'],
                  'components': ['tbbmalloc_proxy'],
                  'modules': ['TBB::tbbmalloc_proxy'],
                },
  'xed': { 'name': 'Xed',
           'propstems': ['xed'],
         },
  'xerces': { 'name': 'xerces-c', 'cmake_name': 'Xerces',
              'propstems': ['xerces'],
            },
  'zlib': { 'name': 'zlib',
            'propstems': ['zlib'],
          },
  'valgrind': { 'name': 'valgrind', 'cmake_name': 'Valgrind',
                'propstems': ['valgrind'],
              },
}
foreach var, properties : _deps
  # Highest priority: explicitly given in *_root
  foreach pstem : properties['propstems']
    _explicit_root = meson.get_external_property(pstem + '_root', '')
    if _explicit_root != ''
      _cmake_name = properties.get('cmake_name', properties['name'])
      set_variable(var, dependency(_cmake_name,
        method: 'cmake',
        required: false, static: false,
        cmake_args: ['-D' + _cmake_name + '_ROOT=' + _explicit_root,
                     '-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE',
                     '-DCMAKE_FIND_USE_CMAKE_PATH=FALSE',
                     '-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE'],
        modules: properties.get('modules', []),
        components: properties.get('components', []),
        cmake_module_path: 'cmake/modules'))
      if not get_variable(var).found()
        error('Could not find (' + ', '.join(properties['names']) + ') from '
              + pstem + '_root property: `' + _explicit_root + '\'')
      endif
    endif
  endforeach

  # Middle priority: searchable via Spack
  if not is_variable(var)
    # TODO: Try to use a Spack override instead.
  endif

  # Lowest priority: implicitly available
  if not is_variable(var)
    set_variable(var, dependency(properties['name'],
      required: false, static: false,
      modules: properties.get('modules', []),
      components: properties.get('components', []),
      cmake_module_path: 'cmake/modules'))
  endif

  # Because Meson saves the CMake cache, we can get the static side by repeating
  # the process but with static set.
  if properties.has_key('static_modules')
    if get_variable(var).found()
      set_variable(var + '_static', dependency(get_variable(var).name(),
        required: false, static: true,
        modules: properties['static_modules'],
        components: properties.get('components', []),
        cmake_module_path: 'cmake/modules'))
    else
      set_variable(var + '_static', dependency('', required: false))
    endif
  endif
endforeach

## Boost is a special case, since Meson overrides its search logic.

# Support for boost_root was added later, so error if you try to use it early.
if meson.get_external_property('boost_root', '') != '' and \
   meson.version().version_compare('<0.55.1')
  error('boost_root as a property is not effective until 0.55.1, please upgrade!')
endif

# TODO: Handle Spack-provided Boost

# Search for Boost. This includes all modules, individual deps handled later.
boost_all = dependency('Boost', required: false,
                       modules: ['atomic', 'chrono', 'date_time', 'filesystem',
                                 'graph', 'regex', 'system', 'thread', 'timer'])

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
cuda = dependency('CUDA', required: false, modules: ['cupti'])

