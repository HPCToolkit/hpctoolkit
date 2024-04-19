_srcs = files(
  'xml.cpp',
)

_struct_dtd = fs.read(meson.project_source_root() / 'lib/dtd/hpc-structure.dtd')
foreach line : _struct_dtd.split('\n')
  if line.contains('Version')
    _struct_dtd_ver = line.strip().split()[2]
    break
  endif
endforeach
_srcs += [
  configure_file(output: 'static.data.cpp', input: 'static.data.cpp.in',
    configuration: {'HPCSTRUCT_DTD': _struct_dtd, 'HPCSTRUCT_DTD_VERSION': _struct_dtd_ver}),
  fs.copyfile('xml.hpp'),  # Needed to compile the above
]

xml_lib = static_library('xml', _srcs,
  pic: true,
  gnu_symbol_visibility: 'hidden')
