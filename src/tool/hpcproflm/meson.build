_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproflm = executable('hpcproflm', _srcs,
  prof_lean_srcs,
  support_srcs,
  implicit_include_directories: false,
  dependencies: [
    libiberty_dep,
    lzma_dep,
    openmp_dep,
    prof_lean_deps,
    support_deps,
  ],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
