# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

_sphinx = find_program(
  'sphinx-build',
  version: '>=4.3',
  native: true,
  required: get_option('manual').enable_if(get_option('manual_pdf').enabled(),
    error_message: 'PDF manual cannot be enabled without HTML manual'),
  disabler: true,
)

_project_name = 'HPCToolkit'
assert(_project_name.to_lower() == meson.project_name())

_sphinx_args = [
  '-q',
  '-E',
  '-W',
  '--keep-going',
  '-d', '@PRIVATE_DIR@/.doctrees',
  '-Dproject=@0@'.format(_project_name),
  '-Dauthor=The @0@ Developers'.format(_project_name),
  '-Dversion=@0@'.format(meson.project_version()),
  '-Drelease=@0@'.format(meson.project_version()),
  '-Ddepfile=@DEPFILE@',
  '@CURRENT_SOURCE_DIR@',
]
_sphinx_depends = files('conf.py')

# The main documentation is generated as a static HTML webpage.
custom_target(
  output: 'html',
  depfile: 'html.d',
  command: [_sphinx, '-bhtml', _sphinx_args, '@OUTPUT@'],
  depend_files: _sphinx_depends,
  install: true,
  install_dir: docdir,
  install_tag: 'docs',
)

# An alternative format is EPUB, which is (essentially) a zip file containing HTML.
# This format may be slightly nicer to copy across machines when necessary.
custom_target(
  output: '@0@.epub'.format(meson.project_name()),
  depfile: '@0@.epub.d'.format(meson.project_name()),
  command: [
    python, files('sphinx-singlefile.py'), '@PRIVATE_DIR@/build', f'@_project_name@.epub', '@OUTPUT@',
    _sphinx, '-bepub', _sphinx_args,
  ],
  depend_files: _sphinx_depends,
  install: true,
  install_dir: docdir,
  install_tag: 'docs',
)

# A more common format for offline documentation is PDF, but this requires a fairly complete
# Latex installation, so it has a separate option and is disabled by default.
_latexmk = find_program('latexmk', native: true, required: get_option('manual_pdf'), disabler: true)
custom_target(
  output: '@0@.pdf'.format(meson.project_name()),
  depfile: '@0@.pdf.d'.format(meson.project_name()),
  command: [
    python, files('sphinx-pdf.py'), '@PRIVATE_DIR@/build', '@0@.pdf'.format(_project_name.to_lower()), '@OUTPUT@',
    _latexmk, '@0@.tex'.format(_project_name.to_lower()),
    _sphinx, '-blatex', _sphinx_args,
  ],
  depend_files: _sphinx_depends,
  install: true,
  install_dir: docdir,
  install_tag: 'docs',
)
