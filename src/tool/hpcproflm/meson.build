_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproflm = executable('hpcproflm', config_h, _srcs,
  link_with: [prof_lean_lib, support_lib],
  install: true,
  install_dir: get_option('libexecdir') / meson.project_name())
