_srcs = files(
  'Args.cpp',
  'main.cpp',
  'MeasDir.cpp',
  'SingleBin.cpp',
  'Structure-Cache.cpp',
  'Structure-Version.cpp',
)

hpcstruct = executable('hpcstruct', version_cpp, _srcs,
  analysis_srcs,
  banal_srcs,
  binutils_srcs,
  prof_lean_srcs,
  prof_srcs,
  support_srcs,
  xml_srcs,
  dependencies: [
    analysis_deps,
    banal_deps,
    binutils_deps,
    boost_dep,
    dyninst_dep,
    iga_dep,
    igc_dep,
    libdw_dep,
    libelf_dep,
    lzma_dep,
    openmp_dep,
    prof_deps,
    prof_lean_deps,
    support_deps,
    xed_dep,
    xerces_dep,
    xml_deps,
  ],
  install: true)

hpcstruct_test_depends = [hpcproflm]
test_depends += hpcstruct_test_depends
_env = {
  'HPCTOOLKIT_HPCSTRUCT': hpcstruct.full_path(),
  'HPCTOOLKIT_HPCPROFLM': hpcproflm.full_path(),
}
hpcstruct_test_env = environment(_env)
meson.add_devenv(hpcstruct_test_env)
foreach k,v : _env
  test_env.set(k, v)
endforeach


dotgraph = executable('dotgraph', 'DotGraph.cpp',
  analysis_srcs,
  banal_srcs,
  binutils_srcs,
  prof_lean_srcs,
  prof_srcs,
  support_srcs,
  xml_srcs,
  dependencies: [
    analysis_deps,
    banal_deps,
    binutils_deps,
    boost_dep,
    dyninst_dep,
    iga_dep,
    igc_dep,
    libdw_dep,
    libelf_dep,
    lzma_dep,
    openmp_dep,
    prof_deps,
    prof_lean_deps,
    support_deps,
    xed_dep,
    xerces_dep,
  ])
