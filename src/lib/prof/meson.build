_srcs = files(
  'Struct-Tree.cpp',
  'Struct-TreeIterator.cpp',
  'CallPath-Profile.cpp',
)

prof_lib = static_library('prof', _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden')
