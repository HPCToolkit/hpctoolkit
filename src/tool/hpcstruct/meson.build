_srcs = files(
  'gpu/CudaCFGParser.cpp',
  'gpu/GPUBlock.cpp',
  'gpu/GPUCFG.cpp',
  'gpu/GPUCFGFactory.cpp',
  'gpu/GPUCodeSource.cpp',
  'gpu/GPUFunction.cpp',
  'Struct-Inline.cpp',
  'Struct-Output.cpp',
  'Struct.cpp',
)
_srcs += xml_srcs

if nvdisasm.found()
  _srcs += files(
    'gpu/GPUCFG_Cuda.cpp',
    'gpu/GraphReader.cpp',
  )
endif

if igc_dep.found()
  _srcs += files('gpu/GPUCFG_Intel.cpp')
endif

_deps = [
  boost_dep,
  dyninst_dep,
  iga_dep,
  igc_dep,
  libdw_dep,
  libelf_dep,
  openmp_dep,
  xed_dep,
]

hpcstruct = executable('hpcstruct', version_cpp, _srcs,
  'Args.cpp',
  'main.cpp',
  'MeasDir.cpp',
  'SingleBin.cpp',
  'Structure-Cache.cpp',
  'Structure-Version.cpp',
  analysis_srcs,
  binutils_srcs,
  prof_lean_srcs,
  prof_srcs,
  support_srcs,
  xml_srcs,
  dependencies: [
    _deps,
    analysis_deps,
    binutils_deps,
    prof_deps,
    prof_lean_deps,
    support_deps,
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


dotgraph = executable('dotgraph', 'DotGraph.cpp', _srcs,
  analysis_srcs,
  binutils_srcs,
  prof_lean_srcs,
  prof_srcs,
  support_srcs,
  xml_srcs,
  dependencies: [
    _deps,
    analysis_deps,
    binutils_deps,
    prof_deps,
    prof_lean_deps,
    support_deps,
  ])
