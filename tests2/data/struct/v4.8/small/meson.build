_binprefix = 'small/bin/'
_structprefix = 'small-'

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

_root = testdata_root + 'struct/v4.8/small/'
foreach name, variants : _structfiles
  _binname = _binprefix + name
  foreach suffix, features : variants
    _out_structfile = name + suffix + '.hpcstruct'
    assert(fs.is_samepath(testdata_srcroot / _root+_out_structfile, files(_out_structfile)))

    _gen_attrs = {}
    _attrs = {}
    if _binname in testdata_bin
      _bin = testdata_bin[_binname]
      _attrs += {'binary': _bin}

      _cmd = [_struct, '@INPUT@', '@OUTPUT@', features.get('gpucfg', false) ? '--gpucfg' : '--no-gpucfg']
      if has_cuda
        _cmd += ['--nvdisasm', nvdisasm.full_path()]
      endif

      _gen = custom_target(output: _out_structfile,
          command: _cmd, input: _bin['binary'], env: hpctoolkit_pyenv,
          build_always_stale: true, build_by_default: false)
      _gen_attrs = {
        'gen': {
          'files': {_root+_out_structfile: _gen},
        },
      }
    endif

    _testdata_struct += {_structprefix + name + suffix: _attrs + {
      'structfile': files(_out_structfile),
      'xfail': features.get('xfail', false),
      'gpucfg': features.get('gpucfg', false),
    } + _gen_attrs}
  endforeach
endforeach
