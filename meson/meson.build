# Provide a linter that detects when headers are inadvertently used without the proper deps.
# This requires a lot of settings that must come from the machine file, so it is only enabled
# when the lint_depends_enable flag is set.
if meson.get_external_property('lint_depends_enable', false)
_lint_deps_args = [
  meson.global_build_root(),
  '--meson', find_program('meson').full_path(),
  '--cc', meson.get_compiler('c').cmd_array(),
  '--ignore', meson.project_source_root(), meson.global_build_root(),
  meson.get_external_property('lint_depends_ignore', []),
]
_essential_pkgs = meson.get_external_property('lint_depends_essential_pkgs', [])
if _essential_pkgs != []
  _lint_deps_args += ['--essential-pkgs'] + _essential_pkgs
endif
foreach spec : meson.get_external_property('lint_depends_pkgs_provide', [])
  _lint_deps_args += ['--pkg-provides', spec]
endforeach
foreach spec : meson.get_external_property('lint_depends_dep_provides', [])
  _lint_deps_args += ['--dep-provides', spec]
endforeach
test(
  'lint dependencies',
  python,
  args: [files('lint-depends'), _lint_deps_args],
  depends: config_h[1],
  priority: 90,   # Less priority than a serial test (100), but still really long
  timeout: 0,
  suite: 'lint',
)
endif
