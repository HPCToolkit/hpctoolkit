project('gtpin', 'cpp', version: '3.7', meson_version: '>=1.0.0')

if not (host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64')
  error('GTPin is only available on Linux x86-64, and will not work for your system')
endif

cpp = meson.get_compiler('cpp')
incdirs = include_directories('Profilers/Include', 'Profilers/Include/ged/intel64')
gtpin_dep = declare_dependency(
  dependencies: cpp.find_library('gtpin', dirs: meson.current_source_dir() / 'Profilers/Lib/intel64'),
  include_directories: incdirs,
  version: meson.project_version(),
)

foreach h : ['api/gtpin_api.h', 'ged.h']
  # NB: On Meson <1.3, include directories from 'internal' dependencies don't get applied, so
  # explicitly add the include_directories to make this check work.
  assert(cpp.check_header(h, dependencies: gtpin_dep, include_directories: incdirs))
endforeach

meson.override_dependency('gtpin', gtpin_dep)
