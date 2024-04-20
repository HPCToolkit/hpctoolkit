_srcs = files(
  'args.cpp',
  'main.cpp',
)

hpcprof = executable('hpcprof', version_cpp, _srcs,
  prof_lean_srcs,
  profile_standalone_srcs,
  support_lean_srcs,
  implicit_include_directories: false,
  dependencies: [
    boost_dep,
    libdw_dep,
    libelf_dep,
    libiberty_dep,
    lzma_dep,
    openmp_dep,
    prof_lean_deps,
    profile_deps,
    support_lean_deps,
    xerces_dep,
    xxhash_dep,
    yaml_cpp_dep,
  ],
  install: true)
