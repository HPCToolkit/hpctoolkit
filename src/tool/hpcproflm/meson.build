_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproflm = executable('hpcproflm', _srcs,
  link_with: [prof_lean_lib, support_lib],
  dependencies: [openmp_dep],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
