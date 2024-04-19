_srcs = files(
  'xml.cpp',
)

xml_lib = static_library('xml', _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden')

_makedtd = [find_program(files('makedtdstring')), '@INPUT@']
exp_dtd = configure_file(output: 'hpc-experiment.dtd.h', input: meson.project_source_root() / 'lib/dtd/hpc-experiment.dtd',
  command: _makedtd, capture: true)
struct_dtd = configure_file(output: 'hpc-structure.dtd.h', input: meson.project_source_root() / 'lib/dtd/hpc-structure.dtd',
  command: _makedtd, capture: true)

struct_dtd_ver = configure_file(output: 'hpc-structure.dtd.version.h', input: meson.project_source_root() / 'lib/dtd/hpc-structure.dtd',
  command: [find_program('awk'), '-f', files('dtd-version.awk'), '@INPUT@'], capture: true)
