_srcs = files(
  'debug_fn.c',
  'fnbounds.c',
  'scan.c',
  'server.c',
)

hpcfnbounds = executable('hpcfnbounds', version_cpp, _srcs,
  implicit_include_directories: false,
  dependencies: [
    libdw_dep,
    libelf_dep,
  ],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
