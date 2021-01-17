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

_srcs = [files(
  'software-instructions.html',
  'spack-issues.html',

  'index.html',
  'documentation.html',
  'download.html',
  'examples.html',
  'overview.html',
  'software.html',

  'info-acks.html',
  'info-people.html',

  'hpctoolkit.org.sitemap.txt',

  'googleeeb6a75d4102e1ef.html',
)]
_figsrcs = [files(
  'fig/index.html',
  'fig/hpctoolkit-workflow.png',
  'fig/hpcviewer-annotated-screenshot.jpg',
  'fig/spacer.gif',
)]
_stylesrcs = [files(
  'style/index.html',
  'style/style.css',
  'style/footer-hpctoolkit.js',
  'style/header-hpctoolkit.js',
  'style/header.gif',
)]

# If we have bib2xhtml, we can generate publications.html
_bib2xhtml = find_program('bib2xhtml', native: true, required: false)
if _bib2xhtml.found()
  _awk = find_program('awk', native: true)
  _gen = find_program('pubgen.sh', native: true)
  _gen2 = find_program('pub2gen.sh', native: true)
  _tmp = configure_file(
    input: files('../manual/texmf/bibtex/bib/pubs.bib'),
    output: 'publications.html.in',
    command: [_gen, _bib2xhtml, '@OUTPUT@', '@INPUT@']
  )
  _srcs += configure_file(
    input: [files('style/header-hpctoolkit.html'),
            files('style/footer-hpctoolkit.html'),
            _tmp],
    output: 'publications.html',
    command: [_gen2, _awk, '@INPUT@'],
    capture: true,
  )
else
  warning('bib2xhtml not found, publications listing may be out of date!')
  _srcs += files('publications.html')
endif

install_data(_srcs,
  install_dir: get_option('datadir') / 'doc' / meson.project_name(),
  install_mode: 'rw-r--r--')
install_data(_figsrcs,
  install_dir: get_option('datadir') / 'doc' / meson.project_name() / 'fig',
  install_mode: 'rw-r--r--')
install_data(_stylesrcs,
  install_dir: get_option('datadir') / 'doc' / meson.project_name() / 'style',
  install_mode: 'rw-r--r--')
