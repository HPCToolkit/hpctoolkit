_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproflm = executable('hpcproflm', _srcs,
  common_srcs,
  implicit_include_directories: false,
  dependencies: [
    common_deps,
    libiberty_dep,
    lzma_dep,
    openmp_dep,
  ],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
