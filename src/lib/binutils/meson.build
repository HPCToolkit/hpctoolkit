_srcs = files(
  'BinUtils.cpp',
  'ElfHelper.cpp',
  'Fatbin.cpp',
  'InputFile.cpp',
  'intel/IntelGPUBinutils.cpp',
  'RelocateCubin.cpp',
  'VMAInterval.cpp',
)

binutils_lib = static_library('binutils', _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden',
  dependencies: [boost_dep, libdw_dep, libelf_dep, dyninst_dep, igc_dep, iga_dep])
