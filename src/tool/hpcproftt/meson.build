_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproftt = executable('hpcproftt', version_cpp, _srcs,
  analysis_srcs,
  binutils_srcs,
  prof_lean_srcs,
  prof_srcs,
  support_srcs,
  xml_srcs,
  implicit_include_directories: false,
  dependencies: [
    analysis_deps,
    binutils_deps,
    iga_dep,
    igc_dep,
    libdw_dep,
    libelf_dep,
    lzma_dep,
    prof_deps,
    prof_lean_deps,
    support_deps,
    xerces_dep,
    xml_deps,
  ])

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
