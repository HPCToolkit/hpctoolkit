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

_srcs = [
  {'src': files('hpctoolkit.tex'), 'stem': 'hpctoolkit'},

  {'src': files('hpcrun.tex'), 'if': 'hpcrun_script', 'stem': 'hpcrun'},
  {'src': files('hpclink.tex'), 'if': 'hpclink_script', 'stem': 'hpclink'},

  {'src': files('hpcstruct.tex'), 'if': 'hpcstruct_exe'},

  {'src': files('hpcprof.tex'), 'if': 'hpcprof_exe'},
  {'src': files('hpcprof-mpi.tex'), 'if': 'hpcprof_mpi_exe'},
  {'src': files('hpcproftt.tex'), 'if': 'hpcproftt_exe'},
  {'src': files('hpcsummary.tex'), 'if': 'hpcsummary_script', 'stem': 'hpcsummary'},

  {'src': files('hpcviewer.tex'), 'stem': 'hpcviewer'},
]

_latex2man = find_program(files('../../tools/latex2man/latex2man'), native: true)
_latex2man_t = files('hpctoolkit-latex2man.trans')

_gen_tmp = generator(find_program('sed', native: true),
  arguments: ['-e', 's/=PACKAGE_VERSION=/@PACKAGE_VERSION@/g', '@INPUT@'],
  capture: true, output: '@BASENAME@.tex_pp')

foreach s : _srcs
  if s.has_key('if')
    if not is_variable(s['if'])
      continue
    endif
    _if = get_variable(s['if'])
    if is_disabler(_if)
      continue
    endif
  endif

  _stem = s.has_key('stem') ? s['stem'] : _if.name()
  custom_target('man page for @0@'.format(_stem),
    command: [_latex2man, '-t', _latex2man_t, '@INPUT@', '@OUTPUT@'],
    output: '@BASENAME@.1hpctoolkit',
    input: _gen_tmp.process(s['src']),
    install: true, install_dir: get_option('mandir') / 'man1',
    install_mode: 'rw-r--r--')
  custom_target('html documentation for @0@'.format(_stem),
    command: [_latex2man, '-H', '-t', _latex2man_t, '@INPUT@', '@OUTPUT@'],
    output: '@BASENAME@.html',
    input: _gen_tmp.process(s['src']),
    install: true, install_dir: get_option('datadir') / 'doc' / meson.project_name() / 'man',
    install_mode: 'rw-r--r--')
endforeach
