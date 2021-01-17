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

_srcs = files(
  'hpcrun-fmt.c',

  'hpcfmt.c',
  'hpcio.c',
  'hpcio-buffer.c',

  # 'atomic-op.i',
  'mcs-lock.c',
  'pfq-rwlock.c',
  'spinlock.c',
  'urand.c',
  'usec_time.c',

  'BalancedTree.c',

  'placeholders.c',

  'lush/lush-support.c',

  'binarytree.c',
  'cskiplist.c',
  'crypto-hash.c',
  'queues.c',
  'stacks.c',
  'bistack.c',
  'bichannel.c',
  'producer_wfq.c',
  'generic_pair.c',
  'procmaps.c',
  'vdso.c',
  'randomizer.c',
  'splay-uint64.c',
  'elf-helper.c',
)

prof_lean_lib = static_library('prof_lean', _srcs, pic: true,
  include_directories: main_includes,
  dependencies: [libelf_d_dep, mbedtls_crypto_d_dep])
prof_lean_static_lib = static_library('prof_lean_static', _srcs,
  include_directories: main_includes,
  dependencies: [libelf_static_d_dep, mbedtls_crypto_d_dep])
