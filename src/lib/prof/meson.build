_srcs = files(
  'Struct-Tree.cpp',
  'Struct-TreeIterator.cpp',
  'CallPath-Profile.cpp',
)

prof_lib = static_library('prof', config_h, _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden')
