_srcs = files(
  'main.cpp'
)

hpctracedump = executable('hpctracedump', _srcs,
  link_with: [prof_lean_lib, support_lib, support_lean_lib])

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
