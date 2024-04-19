if not get_option('hpcprof_mpi').enable_auto_if(mpi_dep.found()).enabled()
  subdir_done()
endif

_srcs = files(
  '../hpcprof/args.cpp',
  'main.cpp',
  'mpi.cpp',
  'tree.cpp',
)

hpcprof_mpi = executable('hpcprof-mpi', version_cpp, _srcs,
  prof_lean_srcs,
  profile_srcs,
  support_lean_srcs,
  dependencies: [
    boost_dep,
    libdw_dep,
    libelf_dep,
    libiberty_dep,
    lzma_dep,
    mpi_dep,
    openmp_dep,
    prof_lean_deps,
    profile_deps,
    support_lean_deps,
    xerces_dep,
    xxhash_dep,
    yaml_cpp_dep,
  ],
  install: true)
