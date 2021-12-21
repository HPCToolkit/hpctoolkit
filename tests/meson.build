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

### NOTE TO DEVELOPERS: This file will be copied out of the source tree by Spack,
### so the code below must find it's own dependencies and recover missing
### executables/scripts from the final installation.

# Recover the HPCToolkit main programs if we don't have them already
if not is_variable('hpcrun_exe')
  hpcrun_exe = find_program('hpcrun', disabler: true, required: false)
endif
if not is_variable('hpcstruct_exe')
  hpcstruct_exe = find_program('hpcstruct', disabler: true, required: false)
endif
if not is_variable('hpcprof_exe')
  hpcprof_exe = find_program('hpcprof', disabler: true, required: false)
endif


# TEST: sort (Smoke test of hpcrun/struct/prof on selection sort)
test('sort', find_program('run-sort.sh'), protocol: 'tap',
     args: [executable('sort', files('sort.cpp')),
            hpcrun_exe, hpcstruct_exe, hpcprof_exe],
     workdir: meson.current_build_dir())
