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
  test(f'Analysis of tstlib-inlines+loops-@name@ is lexically accurate', _tst, args: [
         shared_library(f'tstlib-inlines+loops-@name@', files('inlines+loops.c'),
                        build_by_default: false, override_options: opt),
         '--from', files('inlines+loops.c')] + (_has_debug ? [] : ['--debugless']),
       env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach

_tst = find_program(files('tst-consistent'))
foreach name, struct : testdata_struct_current
  _threads = [1, 16]
  _args = [struct.get('binary', {'binary': ''})['binary'], struct['structfile']]
  _suffix = ''
  if struct['gpucfg']
    _args += ['--gpucfg']
    _suffix = ' +gpucfg'
  endif

  if 'binary' in struct
    # FIXME: Multithread cases frequently fail consistency for CUDA binaries.
    # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/733
    if struct['binary']['arch_tags'][0].startswith('cuda::')
      _threads = [1]
    endif
  else
    # No binary available means we can't actually do this test
    _args += ['--skip', f'Missing input binary, maybe submodule is not cloned?']
  endif

  foreach threads : _threads
    test(f'Analysis of @name@ is consistent (-j@threads@@_suffix@)', _tst,
         args: _args + [f'-j@threads@'],
         env: hpctoolkit_pyenv, suite: 'hpcstruct', is_parallel: '--skip' in _args or threads == 1,
         should_fail:
           # FIXME: hpcstruct on ARM does not correctly parse CFGs on some x86-64 binaries.
           # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/734
           host_machine.cpu_family() == 'aarch64' and name in [
             'small-inlines+loops-x86_64-gcc10-0',
             'small-inlines+loops-x86_64-gcc10-d',
             'small-inlines+loops-x86_64-gcc10-dr',
             'small-inlines+loops-x86_64-gcc10-r',
             'small-inlines+loops-x86_64-gcc12-0',
             'small-inlines+loops-x86_64-gcc12-d',
             'small-inlines+loops-x86_64-gcc12-dr',
             'small-inlines+loops-x86_64-gcc12-r',
             'small-inlines+loops-x86_64-gcc8-0',
             'small-inlines+loops-x86_64-gcc8-d',
             'small-inlines+loops-x86_64-gcc8-dr',
             'small-inlines+loops-x86_64-gcc8-r',
             'small-inlines+loops-x86_64-llvm11-d',
             'small-inlines+loops-x86_64-llvm11-dr',
             'small-inlines+loops-x86_64-llvm11-r',
             'small-inlines+loops-x86_64-llvm15-d',
             'small-inlines+loops-x86_64-llvm15-dr',
             'small-inlines+loops-x86_64-llvm15-r',
             'small-inlines+loops-x86_64-llvm7-d',
             'small-inlines+loops-x86_64-llvm7-dr',
             'small-inlines+loops-x86_64-llvm7-r',
           ]
    )
  endforeach
endforeach
