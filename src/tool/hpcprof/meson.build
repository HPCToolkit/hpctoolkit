_srcs = files(
  'args.cpp',
  'main.cpp',
)

hpcprof = executable('hpcprof', config_h, _srcs,
  link_with: [profile_lib, profile_standalone_lib, prof_lean_lib, support_lean_lib],
  dependencies: [xerces_dep, boost_dep, lzma_dep, yaml_cpp_dep, libelf_dep, libdw_dep, libiberty_dep],
  install: true)
