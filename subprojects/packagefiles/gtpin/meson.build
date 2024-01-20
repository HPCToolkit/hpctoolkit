project('gtpin', 'cpp', version: '3.7', meson_version: '>=1.0.0')

if not (host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64')
  error('GTPin is only available on Linux x86-64, and will not work for your system')
endif

cpp = meson.get_compiler('cpp')
gtpin_dep = declare_dependency(
  dependencies: cpp.find_library('gtpin', dirs: meson.current_source_dir() / 'Profilers/Lib/intel64'),
  include_directories: include_directories('Profilers/Include', 'Profilers/Include/ged/intel64'),
  version: meson.project_version(),
)

foreach h : ['api/gtpin_api.h', 'ged.h']
  assert(cpp.check_header(h, dependencies: gtpin_dep))
endforeach

meson.override_dependency('gtpin', gtpin_dep)
