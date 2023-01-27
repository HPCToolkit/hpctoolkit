_binprefix = 'small/bin/'
_structprefix = 'small-'

_s_none = {'gpucfg': false, 'suite': 'none'}
_s_x86_64 = {'gpucfg': false, 'suite': 'x86-64'}
_s_cuda = {'gpucfg': true, 'suite': 'sw-cuda'}
_structfiles = {
  'inlines+loops-sm_62-nvcc102-0': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_62-nvcc102-dr': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_62-nvcc102-r': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_75-nvcc112-0': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_75-nvcc112-dr': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_75-nvcc112-r': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_87-nvcc118-0': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_87-nvcc118-dr': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-sm_87-nvcc118-r': {'': _s_none, '+gpucfg': _s_cuda},
  'inlines+loops-x86_64-gcc10-0': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc10-d': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc10-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc10-r': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc12-0': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc12-d': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc12-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc12-r': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc8-0': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc8-d': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc8-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-gcc8-r': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm11-0': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm11-d': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm11-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm11-r': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm15-0': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm15-d': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm15-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm15-r': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm7-0': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm7-d': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm7-dr': {'': _s_x86_64},
  'inlines+loops-x86_64-llvm7-r': {'': _s_x86_64},
}

_root = testdata_root + 'struct/v4.8/small/'
foreach name, variants : _structfiles
  _binname = _binprefix + name
  foreach suffix, features : variants
    _out_structfile = name + suffix + '.hpcstruct'
    assert(fs.is_samepath(testdata_srcroot / _root+_out_structfile, files(_out_structfile)))

    _attrs = {}
    _gen = disabler()
    if _binname in testdata_bin
      _bin = testdata_bin[_binname]
      _attrs += {'binary': _bin}
      _gen = custom_target(output: _out_structfile,
          command: [_struct, '@INPUT@', '@OUTPUT@',
                    features.get('gpucfg', false) ? '--gpucfg' : '--no-gpucfg',
          ], input: _bin['binary'], env: hpctoolkit_pyenv,
          build_always_stale: true, build_by_default: false)
    endif

    _testdata_struct += {_structprefix + name + suffix: _attrs + {
      'structfile': files(_out_structfile),
      'xfail': features.get('xfail', false),
      'gpucfg': features.get('gpucfg', false),
      'gen': {
        'suite': features['suite'],
        'files': {_root+_out_structfile: _gen},
      },
    }}
  endforeach
endforeach
