# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

docdir = get_option('datadir') / 'doc' / meson.project_name()

install_data(
  'FORMATS.md',
  'METRICS.yaml',
  install_dir: docdir,
  install_tag: 'docs',
)

dtd_hpc_structure = files('dtd/hpc-structure.dtd')

install_data(
  'dtd/hpc-experiment.dtd',
  'dtd/hpc-structure.dtd',
  'dtd/hpcprof-config.dtd',
  install_dir: get_option('datadir') / meson.project_name() / 'dtd',
  install_tag: 'dtd',
)
install_data(
  'dtd/mathml/xhtml1-transitional-mathml.dtd',
  'dtd/mathml/mathml.dtd',
  'dtd/mathml/isoamsa.ent',
  'dtd/mathml/isoamsb.ent',
  'dtd/mathml/isoamsc.ent',
  'dtd/mathml/isoamsn.ent',
  'dtd/mathml/isoamso.ent',
  'dtd/mathml/isoamsr.ent',
  'dtd/mathml/isobox.ent',
  'dtd/mathml/isocyr1.ent',
  'dtd/mathml/isocyr2.ent',
  'dtd/mathml/isodia.ent',
  'dtd/mathml/isogrk3.ent',
  'dtd/mathml/isolat1.ent',
  'dtd/mathml/isolat2.ent',
  'dtd/mathml/isomfrk.ent',
  'dtd/mathml/isomopf.ent',
  'dtd/mathml/isomscr.ent',
  'dtd/mathml/isonum.ent',
  'dtd/mathml/isopub.ent',
  'dtd/mathml/isotech.ent',
  'dtd/mathml/mmlalias.ent',
  'dtd/mathml/mmlextra.ent',
  install_dir: get_option('datadir') / meson.project_name() / 'dtd/mathml',
  install_tag: 'dtd',
)

subdir('src')
subdir('man')
