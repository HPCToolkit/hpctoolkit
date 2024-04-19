_srcs = files(
  'debug_fn.c',
  'fnbounds.c',
  'scan.c',
  'server.c',
)

hpcfnbounds = executable('hpcfnbounds', version_h, _srcs,
  dependencies: [libelf_dep, libdw_dep],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
