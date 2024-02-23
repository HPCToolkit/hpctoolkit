_srcs = files(
  'gpu/CudaCFGParser.cpp',
  'gpu/GPUBlock.cpp',
  'gpu/GPUCFG_Intel.cpp',
  'gpu/GPUCFG.cpp',
  'gpu/GPUCFGFactory.cpp',
  'gpu/GPUCodeSource.cpp',
  'gpu/GPUFunction.cpp',
  'gpu/GraphReader.cpp',
  'Struct-Inline.cpp',
  'Struct-Output.cpp',
  'Struct.cpp',
)

if nvdisasm.found()
  _srcs += files('gpu/GPUCFG_Cuda.cpp')
endif

banal_lib = static_library('banal', config_h, _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden',
  dependencies: [xed_dep, boost_dep, libdw_dep, libelf_dep, dyninst_dep, igc_dep, iga_dep, openmp_dep])
