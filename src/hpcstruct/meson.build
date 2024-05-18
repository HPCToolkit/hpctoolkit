subdir('hpcproflm')

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
  'xml.cpp',
  'BinUtils.cpp',
  'ElfHelper.cpp',
  'Fatbin.cpp',
  'InputFile.cpp',
  'intel/IntelGPUBinutils.cpp',
  'RelocateCubin.cpp',
  'VMAInterval.cpp',
)

_struct_dtd = fs.read(dtd_hpc_structure)
foreach line : _struct_dtd.split('\n')
  if line.contains('Version')
    _struct_dtd_ver = line.strip().split()[2]
    break
  endif
endforeach
_srcs += [
  configure_file(output: 'static.data.cpp', input: 'static.data.cpp.in',
    configuration: {'HPCSTRUCT_DTD': _struct_dtd, 'HPCSTRUCT_DTD_VERSION': _struct_dtd_ver}),
  fs.copyfile('static.data.h'),  # Needed to compile the above
]

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

_srcs += common_srcs
_deps += common_deps

hpcstruct = executable('hpcstruct', version_cpp, _srcs,
  'Args.cpp',
  'main.cpp',
  'MeasDir.cpp',
  'SingleBin.cpp',
  'Structure-Cache.cpp',
  'Structure-Version.cpp',
  dependencies: _deps,
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
  dependencies: _deps)
