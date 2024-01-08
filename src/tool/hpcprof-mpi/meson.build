if not get_option('hpcprof_mpi').enable_auto_if(mpi_dep.found()).enabled()
  subdir_done()
endif

_srcs = files(
  '../hpcprof/args.cpp',
  'main.cpp',
  'mpi.cpp',
  'tree.cpp',
)

hpcprof_mpi = executable('hpcprof-mpi', config_h, _srcs,
  link_with: [profile_lib, prof_lean_lib, support_lean_lib],
  dependencies: [mpi_dep, boost_dep, xerces_dep, lzma_dep, yaml_cpp_dep, libelf_dep, libdw_dep, libiberty_dep],
  install: true)
