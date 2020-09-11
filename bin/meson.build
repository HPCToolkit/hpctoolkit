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

# While we go to some lengths to ensure the build_rpath is set, we don't
# bother with the install_rpath, which means a normal install (probably)
# won't be functional. So instead, we make it easier to run bits from the build
# tree by copying everything here into bin/.

# This part handles the front-end executables and scripts

_cp_exec = find_program('cp-exec.sh')

foreach exe : all_apps
  custom_target('in-tree copy of ' + fs.name('@0@'.format(exe)),
    command: [_cp_exec, '@INPUT@', '@OUTPUT@'],
    input: exe, output: '@PLAINNAME@',
    build_by_default: true)
endforeach

# The hpcrun and hpclink scripts are also handled here, because they require
# a bit of extra tuning to be useful.
if is_variable('hpcrun_script')
  _hpcrun_intree_launch_config = configuration_data()
  _hpcrun_intree_launch_config.merge_from(launch_config)
  _hpcrun_intree_launch_config.set('export_hpctoolkit', '''
  hpcrun_dir='src/tool/hpcrun'
  hpcfnbounds_dir='bin'
  HPCTOOLKIT="$(dirname "$0")/.."
  ''')
  custom_target('in-tree copy of hpcrun',
    command: [_cp_exec, '@INPUT@', '@OUTPUT@'],
    input: configure_file(input: hpcrun_script, output: 'hpcrun.sh',
                          configuration: _hpcrun_intree_launch_config),
    output: 'hpcrun', build_by_default: true)
endif

if is_variable('hpclink_script')
  _hpclink_intree_launch_config = configuration_data()
  _hpclink_intree_launch_config.merge_from(launch_config)
  _hpclink_intree_launch_config.set('export_hpctoolkit', '''
  hpcfnbounds_dir='bin'
  plugin_dir='src/tool/hpcrun'
  pc_dir='meson-uninstalled'
  HPCTOOLKIT="$(dirname "$0")/.."
  ''')
  custom_target('in-tree copy of hpclink',
    command: [_cp_exec, '@INPUT@', '@OUTPUT@'],
    input: configure_file(input: hpclink_script, output: 'hpclink.sh',
                          configuration: _hpclink_intree_launch_config),
    output: 'hpclink', build_by_default: true)
endif
