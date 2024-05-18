_srcs = files(
  'main.cpp'
)

hpctracedump = executable('hpctracedump', _srcs,
  common_srcs,
  dependencies: [
    libiberty_dep,
    lzma_dep,
    common_deps,
  ])

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
