_srcs = files(
  'Args.cpp',
  'main.cpp',
)

hpcproftt = executable('hpcproftt', version_cpp, _srcs,
  link_with: [analysis_lib, prof_lib, binutils_lib, prof_lean_lib, xml_lib, support_lib, support_lean_lib],
  dependencies: [libelf_dep, libdw_dep, igc_dep, iga_dep, lzma_dep, xerces_dep])

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
