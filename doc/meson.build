docdir = get_option('datadir') / 'doc' / meson.project_name()

install_data(
  'FORMATS.md',
  'METRICS.yaml',
  install_dir: docdir,
  install_tag: 'docs',
)

install_data(
  'HPCToolkit-users-manual.pdf',
  install_dir: docdir / 'manual',
  install_tag: 'docs',
)

subdir('man')
