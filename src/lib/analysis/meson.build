_srcs = files(
  'Raw.cpp',
  'Args.cpp',
  'Util.cpp',
)

analysis_lib = static_library('analysis', config_h, _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden',
  dependencies: [xerces_dep, boost_dep])
