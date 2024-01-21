_latex2man = find_program(meson.project_source_root() / 'tools/latex2man/latex2man')
_latex2man_trans = files('hpctoolkit-latex2man.trans')

_ext = '1hpctoolkit'
_stems = [
  'hpctoolkit',
  'hpcrun',
  'hpcstruct',
  'hpcprof',
  'hpcproftt',
  'hpcviewer'
]
foreach stem : _stems
  _pp = configure_file(input: f'@stem@.tex', output: f'@stem@.tex.pp',
    configuration: {'PACKAGE_VERSION': meson.project_version()})
  custom_target(output: f'@stem@.@_ext@', input: _pp,
    command: [_latex2man, '-t', _latex2man_trans, '@INPUT@', '@OUTPUT@'],
    install: true, install_dir: get_option('mandir') / 'man1',
    install_tag: 'man')
  custom_target(output: f'@stem@.html', input: _pp,
    command: [_latex2man, '-H', '-t', _latex2man_trans, '@INPUT@', '@OUTPUT@'],
    install: true, install_dir: docdir / 'man',
    install_tag: 'man')
endforeach
