_srcs = files(
  'timer.c',
  'OSUtil.c',
  'compress_lzma.c',
  'demangle.c',
)

support_lean_lib = static_library('support-lean', config_h, _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden',
  dependencies: [libiberty_dep, lzma_dep])
