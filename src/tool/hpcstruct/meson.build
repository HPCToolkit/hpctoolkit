_srcs = files(
  'Args.cpp',
  'main.cpp',
  'MeasDir.cpp',
  'SingleBin.cpp',
  'Structure-Cache.cpp',
  'Structure-Version.cpp',
)

hpcstruct = executable('hpcstruct', version_cpp, _srcs,
  link_with: [analysis_lib, banal_lib, prof_lib, binutils_lib, prof_lean_lib, xml_lib, support_lib, support_lean_lib],
  dependencies: [xed_dep, dyninst_dep, boost_dep, libelf_dep, libdw_dep, igc_dep, iga_dep, lzma_dep, xerces_dep, openmp_dep],
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
  link_with: [analysis_lib, banal_lib, prof_lib, binutils_lib, prof_lean_lib, xml_lib, support_lib, support_lean_lib],
  dependencies: [xed_dep, dyninst_dep, boost_dep, libelf_dep, libdw_dep, igc_dep, iga_dep, lzma_dep, xerces_dep, openmp_dep])
