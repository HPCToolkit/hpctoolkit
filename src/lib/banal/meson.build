banal_srcs = files(
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
banal_srcs += xml_srcs

if nvdisasm.found()
  banal_srcs += files(
    'gpu/GPUCFG_Cuda.cpp',
    'gpu/GraphReader.cpp',
  )
endif

if igc_dep.found()
  banal_srcs += files('gpu/GPUCFG_Intel.cpp')
endif

banal_deps = [
  boost_dep,
  dyninst_dep,
  iga_dep,
  igc_dep,
  libdw_dep,
  libelf_dep,
  openmp_dep,
  xed_dep,
]
