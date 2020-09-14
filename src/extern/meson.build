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

_redef = find_program('redefine.sh', native: true)

_victims = [
  { 'name': 'libunwind', 'code': '''
  #define UNW_LOCAL_ONLY
  #include <sys/types.h>
  #include <libunwind.h>
  unw_word_t my_get_ip_addr(unw_context_t *context) {
    unw_cursor_t cursor;
    unw_word_t ip;
    unw_init_local(&cursor, context);
    unw_step(&cursor);
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    return ip;
  }
  ''' },
  { 'name': 'liblzma', 'code': '''
  #include <sys/types.h>
  #include <inttypes.h>
  #include <lzma.h>
  long myencode(uint8_t *in_buf, uint8_t *out_buf) {
    lzma_stream stream = LZMA_STREAM_INIT;
    stream.next_in = in_buf;
    stream.next_out = out_buf;
    lzma_code(&stream, LZMA_FINISH);
    return (long) stream.avail_out;
  }
  ''' },
]

foreach v : _victims
  if cc.links(v['code'], name: '@0@.a -fPIC compatibility'.format(v['name']),
    args: ['-shared', '-fPIC'],
    dependencies: get_variable(v['name']+'_static_dep_libs'),
    include_directories: get_variable(v['name']+'_static_dep_idirs'))

    _objcopy = find_program('objcopy')

    # Meson makes it very difficult to get our mitts on the actual paths to
    # libraries, so we take a bit of a gamble using the fact that (currently)
    # the pkgconfig module writes it out (for static libs) during configure-time.
    # Its a hack and a half, and only works for static libs.
    _pkgconfig = find_program('pkg-config', native: true)
    pkgconfig.generate(name: 'externalization_'+v['name'],
      description: 'temporary externalization shim for '+v['name'],
      libraries: get_variable(v['name']+'_static_dep_libs'))

    _new_libs = []
    foreach _ : get_variable(v['name']+'_static_dep_libs')
      _new_libs += 'internalized_@0@_@1@.a'.format(v['name'], _new_libs.length())
    endforeach
    _new_libs = custom_target('internalized '+v['name'],
      command: [_redef, _objcopy, _pkgconfig,
        meson.build_root() / 'meson-uninstalled', 'externalization_'+v['name'],
        '@OUTPUT@'],
      output: _new_libs)
    _new_lib_paths = []
    foreach l : _new_libs.to_list()
      _new_lib_paths += l.full_path()
    endforeach

    foreach var : [v['name']+'_static', v['name']]
      set_variable(var+'_dep_libs', _new_libs.to_list())
      set_variable(var+'_dep', declare_dependency(
        link_args: _new_lib_paths,
        dependencies: get_variable(var+'_dep').partial_dependency(compile_args: true, includes: true),
      ))
      set_variable(var+'_d_dep', get_variable(var+'_dep').found() ? get_variable(var+'_dep') : disabler())
      set_variable(var+'_dep_linkdeps', get_variable(var+'_dep').found() ? _new_libs : [])
    endforeach
  else
    error('foo!')
  endif
endforeach
