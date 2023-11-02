testdata_struct = {}

_structfiles = {
  'inlines+loops-sm_62-nvcc102-0': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_62-nvcc102-dr': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_62-nvcc102-r': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_75-nvcc112-0': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_75-nvcc112-dr': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_75-nvcc112-r': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_87-nvcc118-0': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_87-nvcc118-dr': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-sm_87-nvcc118-r': {'': {}, '+gpucfg': {'gpucfg': true}},
  'inlines+loops-x86_64-gcc10-0': {'': {}},
  'inlines+loops-x86_64-gcc10-d': {'': {}},
  'inlines+loops-x86_64-gcc10-dr': {'': {}},
  'inlines+loops-x86_64-gcc10-r': {'': {}},
  'inlines+loops-x86_64-gcc12-0': {'': {}},
  'inlines+loops-x86_64-gcc12-d': {'': {}},
  'inlines+loops-x86_64-gcc12-dr': {'': {}},
  'inlines+loops-x86_64-gcc12-r': {'': {}},
  'inlines+loops-x86_64-gcc8-0': {'': {}},
  'inlines+loops-x86_64-gcc8-d': {'': {}},
  'inlines+loops-x86_64-gcc8-dr': {'': {}},
  'inlines+loops-x86_64-gcc8-r': {'': {}},
  'inlines+loops-x86_64-llvm11-0': {'': {}},
  'inlines+loops-x86_64-llvm11-d': {'': {}},
  'inlines+loops-x86_64-llvm11-dr': {'': {}},
  'inlines+loops-x86_64-llvm11-r': {'': {}},
  'inlines+loops-x86_64-llvm15-0': {'': {}},
  'inlines+loops-x86_64-llvm15-d': {'': {}},
  'inlines+loops-x86_64-llvm15-dr': {'': {}},
  'inlines+loops-x86_64-llvm15-r': {'': {}},
  'inlines+loops-x86_64-llvm7-0': {'': {}},
  'inlines+loops-x86_64-llvm7-d': {'': {}},
  'inlines+loops-x86_64-llvm7-dr': {'': {}},
  'inlines+loops-x86_64-llvm7-r': {'': {}},
  'recursion-sm_75-nvcc112-0': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-sm_75-nvcc112-dr': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-sm_75-nvcc112-r': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-sm_87-nvcc118-0': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-sm_87-nvcc118-dr': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-sm_87-nvcc118-r': {'': {}, '+gpucfg': {'gpucfg': true}},
  'recursion-x86_64-gcc10-0': {'': {}},
  'recursion-x86_64-gcc10-d': {'': {}},
  'recursion-x86_64-gcc10-dr': {'': {}},
  'recursion-x86_64-gcc10-r': {'': {}},
  'recursion-x86_64-gcc12-0': {'': {}},
  'recursion-x86_64-gcc12-d': {'': {}},
  'recursion-x86_64-gcc12-dr': {'': {}},
  'recursion-x86_64-gcc12-r': {'': {}},
  'recursion-x86_64-gcc8-0': {'': {}},
  'recursion-x86_64-gcc8-d': {'': {}},
  'recursion-x86_64-gcc8-dr': {'': {}},
  'recursion-x86_64-gcc8-r': {'': {}},
  'recursion-x86_64-llvm11-0': {'': {}},
  'recursion-x86_64-llvm11-d': {'': {}},
  'recursion-x86_64-llvm11-dr': {'': {}},
  'recursion-x86_64-llvm11-r': {'': {}},
  'recursion-x86_64-llvm15-0': {'': {}},
  'recursion-x86_64-llvm15-d': {'': {}},
  'recursion-x86_64-llvm15-dr': {'': {}},
  'recursion-x86_64-llvm15-r': {'': {}},
  'recursion-x86_64-llvm7-0': {'': {}},
  'recursion-x86_64-llvm7-d': {'': {}},
  'recursion-x86_64-llvm7-dr': {'': {}},
  'recursion-x86_64-llvm7-r': {'': {}},
}
foreach name, variants : _structfiles
  _binname = 'small/bin/' + name
  foreach suffix, features : variants
    _struct = {
      'structfile': meson.current_source_dir() / f'@name@@suffix@.hpcstruct',
      'gpucfg': features.get('gpucfg', false),
    }
    if _binname in testdata_bin
      _bin = testdata_bin[_binname]
      _struct += {'binary': _bin}
      _gen = custom_target(output: f'@name@@suffix@.hpcstruct', input: _bin['binary'],
        build_always_stale: true,  # This can be removed once we can actually track when hpstruct changes
        console: true, command:
          [hpcstruct, '--nocache', '@INPUT@', '-o', '@OUTPUT@',
           '-j1', '--gpucfg=' + (features.get('gpucfg', false) ? 'yes' : 'no')],
      )

      _tag = 'none'
      if _bin['arch_tags'][0].startswith('cuda::') and features.get('gpucfg', false)
        _tag = 'sw-cuda'
        if not has_cuda
          _gen = disabler()
        endif
      endif

      testdata_files += {_struct['structfile']: {
        'src': _gen,
        'tag': _tag,
      }}
    endif

    testdata_struct += {f'@name@@suffix@': _struct}
  endforeach
endforeach
