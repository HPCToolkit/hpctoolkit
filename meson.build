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
openmp = dependency('OpenMP')
# TODO: Handle finding the OpenMP directory.

# Enable MPI (for C and C++).
# NOTE: Autotools has a separate search system for Cray and Blue Gene, to get
# around a Spack bug. We trust Meson's search functionality and ability to
# override the issue in a Spack recipe.
mpi_c = dependency('MPI', language: 'c')
mpi_cpp = dependency('MPI', language: 'cpp')

