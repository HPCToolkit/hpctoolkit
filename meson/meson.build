# Provide a dependency linter, which identifies when targets are missing dependencies for headers
# they #include. There are a lot of settings required, which come as part of the native/cross file.
_lint_deps_cmd = [
  python, files('lint-depends'),
  '@BUILD_ROOT@',
  '--meson', find_program('meson'),
  '--cc', meson.get_compiler('c').cmd_array(),
  '--ignore', '@SOURCE_ROOT@', '@BUILD_ROOT@',
  meson.get_external_property('lint_depends_ignore', []),
]
_essential_pkgs = meson.get_external_property('lint_depends_essential_pkgs', [])
if _essential_pkgs != []
  _lint_deps_cmd += ['--essential-pkgs'] + _essential_pkgs
endif
foreach spec : meson.get_external_property('lint_depends_pkgs_provide', [])
  _lint_deps_cmd += ['--pkg-provides', spec]
endforeach
foreach spec : meson.get_external_property('lint_depends_dep_provides', [])
  _lint_deps_cmd += ['--dep-provides', spec]
endforeach
run_target('lint-depends', command: _lint_deps_cmd, depends: config_h[1])
