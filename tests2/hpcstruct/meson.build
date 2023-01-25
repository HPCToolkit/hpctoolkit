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
