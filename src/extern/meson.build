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

_redef = find_program('redefine.sh', native: true)

_victims = {
  'libunwind': '''
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
  ''',
  'liblzma': '''
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
  ''',
}

foreach name, code : _victims
  _dep = dependency(name, required: false, static: true)
  if not _dep.found()
    continue
  endif
  assert(ex_libs.has_key(name), 'dependency @0@ is not (fully) external, cannot internalize!'.format(name))

  if cc.links(code, name: '@0@.a -fPIC compatibility'.format(name),
    args: ['-shared', '-fPIC'],
    dependencies: ex_libs[name], include_directories: ex_idirs[name])

    _objcopy = find_program('objcopy')

    # Meson makes it very difficult to get our mitts on the actual paths to
    # libraries, so we take a bit of a gamble using the fact that (currently)
    # the pkgconfig module writes it out (for static libs) during configure-time.
    # Its a hack and a half, and only works for static libs.
    _pkgconfig = find_program('pkg-config', native: true)
    pkgconfig.generate(name: 'hpctoolkit_external_'+name,
      description: 'temporary externalization shim for '+name,
      libraries: ex_libs[name])

    _new_lib_names = []
    foreach _ : ex_libs[name]
      _new_lib_names += 'internalized_@0@_@1@.a'.format(name, _new_lib_names.length())
    endforeach
    _new_libs = custom_target('internalized '+name,
      command: [_redef, _objcopy, _pkgconfig,
        meson.build_root() / 'meson-uninstalled', 'hpctoolkit_external_'+name,
        '@OUTPUT@'],
      output: _new_lib_names,
      install: true, install_dir: get_option('libdir') / 'hpctoolkit/extern')

    # In the dynamic case we can just let Meson handle it. It gets it right in the end.
    meson.override_dependency('internalized '+name, declare_dependency(
      link_with: _new_libs,
      dependencies: _dep.partial_dependency(compile_args: true, includes: true),
      version: 'internalized '+_dep.version(),
    ))

    # In the static case things are more difficult. We create an empty static
    # library to act as a target, and then edit the pkgconfig output to ensure
    # the paths end up right in the end.
    _shim_lib = static_library('hpctoolkit_internalized_'+name,
      install: true, install_dir: get_option('libdir') / 'hpctoolkit/extern')
    _shim_paths = []
    foreach n : _new_lib_names
      _shim_paths += '${extprefix}/' + n
    endforeach
    pkgconfig.generate(_shim_lib,
      libraries: _shim_paths,
      variables: 'extprefix=' + '${prefix}'/get_option('libdir')/'hpctoolkit/extern',
      uninstalled_variables: 'extprefix='+meson.current_build_dir(),
      install_dir: get_option('libdir') / 'hpctoolkit/pkgconfig')
    meson.override_dependency('internalized '+name, declare_dependency(
      link_with: _shim_lib,
      dependencies: _dep.partial_dependency(compile_args: true, includes: true),
      version: 'internalized '+_dep.version(),
    ), static: true)
  else
    error('''
The dependency @0@ does not provide an archive (.a) compiled with -fPIC.
hpcrun is unable to properly hide this libraries from the application which
may cause deadlocks or segfaults. Please recompile with -fPIC (+pic for Spack).
'''.strip().format(name))
  endif
endforeach
