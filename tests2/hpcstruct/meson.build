_compile_options = {
  'dbg': ['debug=true', 'optimization=0'],
  'dbgopt': ['debug=true', 'optimization=3'],
  'unopt': ['debug=false', 'optimization=0'],
  'opt': ['debug=false', 'optimization=3'],
}

_tst = find_program(files('tst-lexical-structure'))
foreach name, opt : _compile_options
  _has_debug = 'debug=true' in opt
  assert(_has_debug or 'debug=false' in opt)
  test(f'Analysis of tstlib-inlines+loops-@name@ is lexically accurate',
      _tst, args: [hpctesttool, hpcstruct,
          shared_library(f'tstlib-inlines+loops-@name@', files('inlines+loops.c'),
                         build_by_default: false, override_options: opt),
          files('inlines+loops.c'), (_has_debug ? '--debug' : '--no-debug')],
      suite: 'hpcstruct')
endforeach

_tst = find_program(files('tst-consistent'))
foreach name, struct : testdata_struct
  _threads = [1, 16]
  _args = [
    hpctesttool, hpcstruct,
    struct['structfile'],
    struct['binary']['file'],
    '--gpucfg=' + (struct['gpucfg'] ? 'yes' : 'no'),
  ]
  _suffix = struct['gpucfg'] ? '+gpucfg' : ''

  # FIXME: nvdisasm must be on the PATH for hpcstruct to operate on CUDA binaries
  # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/799
  _env = environment()
  if has_cuda
    _env.prepend('PATH', fs.parent(nvdisasm.full_path()))
  endif

  # FIXME: Multithread cases frequently fail consistency for CUDA binaries.
  # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/733
  if struct['binary'].get('cuda', false)
    _threads = [1]

    # Skip in the required support is not present
    if not has_cuda
      _args += ['--skip']
    endif
  endif

  foreach threads : _threads
    test(f'Analysis of @name@ is consistent (-j@threads@@_suffix@)',
         _tst, args: _args + [f'-j@threads@'], env: _env,
         suite: 'hpcstruct', is_parallel: 'binary' not in struct or threads == 1,
    )
  endforeach
endforeach
