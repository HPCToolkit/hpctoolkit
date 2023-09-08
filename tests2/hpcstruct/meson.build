_compile_options = {
  'dbg': ['debug=true', 'optimization=0'],
  'dbgopt': ['debug=true', 'optimization=3'],
  'unopt': ['debug=false', 'optimization=0'],
  'opt': ['debug=false', 'optimization=3'],
}

_tst = configure_file(input: files('tst-lexical-structure'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
foreach name, opt : _compile_options
  _has_debug = 'debug=true' in opt
  assert(_has_debug or 'debug=false' in opt)
  test(f'Analysis of tstlib-inlines+loops-@name@ is lexically accurate',
       _tst, args: [
         shared_library(f'tstlib-inlines+loops-@name@', files('inlines+loops.c'),
                        build_by_default: false, override_options: opt),
         '--from', files('inlines+loops.c')] + (_has_debug ? [] : ['--debugless']),
       env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach

_tst = configure_file(input: files('tst-consistent'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
foreach name, struct : testdata_struct_current
  _threads = [1, 16]
  _args = [
    struct.get('binary', {'binary': '--skip=test-binaries submodule was not cloned'})['binary'],
    struct['structfile'],
  ]
  _suffix = ''
  if struct['gpucfg']
    _args += ['--gpucfg']
    _suffix = ' +gpucfg'
  endif

  if has_cuda
    _args += ['--nvdisasm', nvdisasm.full_path()]
  endif

  # FIXME: Multithread cases frequently fail consistency for CUDA binaries.
  # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/733
  if 'binary' in struct and struct['binary']['arch_tags'][0].startswith('cuda::')
    _threads = [1]
  endif

  foreach threads : _threads
    test(f'Analysis of @name@ is consistent (-j@threads@@_suffix@)',
         _tst, args: _args + [f'-j@threads@'],
         env: hpctoolkit_pyenv, suite: 'hpcstruct', is_parallel: 'binary' not in struct or threads == 1,
    )
  endforeach
endforeach
