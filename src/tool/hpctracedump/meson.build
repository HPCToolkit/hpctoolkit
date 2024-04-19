_srcs = files(
  'main.cpp'
)

hpctracedump = executable('hpctracedump', _srcs,
  prof_lean_srcs,
  support_srcs,
  dependencies: [
    libiberty_dep,
    lzma_dep,
    prof_lean_deps,
    support_deps,
  ])

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
