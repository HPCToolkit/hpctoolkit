# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

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
  test(
    f'Analysis of tstlib-inlines+loops-@name@ is lexically accurate',
    _tst,
    args: [
      hpctesttool,
      hpcstruct,
      shared_library(
        f'tstlib-inlines+loops-@name@',
        files('inlines+loops.c'),
        build_by_default: false,
        override_options: opt,
      ),
      files('inlines+loops.c'),
      (_has_debug ? '--debug' : '--no-debug'),
    ],
    suite: 'hpcstruct',
  )
endforeach

_tst = find_program(files('tst-consistent'))
foreach name, struct : testdata_struct
  _threads = [1, 3]
  _args = [
    hpctesttool,
    hpcstruct,
    struct['structfile'],
    struct['binary']['file'],
    '--gpucfg=' + (struct['gpucfg'] ? 'yes' : 'no'),
  ]
  _suffix = struct['gpucfg'] ? '+gpucfg' : ''

  # FIXME: Multithread cases frequently fail consistency for CUDA binaries.
  # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/733
  if struct['binary'].get('cuda', false)
    _threads = [1]

    # If the required support is not available, do not emit any tests.
    if struct['gpucfg'] and not nvdisasm.found()
      continue
    endif
  endif

  foreach threads : _threads
    _should_fail = false
    # FIXME: As of Elfutils 0.191, the handling for Nvidia binaries (without nvdisasm) is
    # no longer functional. Mark these cases as XFAIL until a fix comes about.
    # https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/831
    if libdw_dep.version().version_compare('>=0.191') and struct['binary'].get(
      'cuda',
      false,
    )
      _should_fail = true
    endif
    # FIXME: Dyninst 12.3 has a bug that makes it unable to parse certain inline sequences
    # from LLVM binaries, see https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/754.
    # This is fixed in 13.0, but mark as XFAIL to support older Dyninst.
    if dyninst_dep.version().version_compare('<13.0.0') and name in [
      'inlines+loops-x86_64-llvm15-0',
      'inlines+loops-x86_64-llvm15-d',
      'inlines+loops-x86_64-llvm15-dr',
    ]
      _should_fail = true
    endif
    # NB: CUDA 12.3 and below has a bug where `CALL.REL` instructions are improperly handled
    # by nvdisasm and are missing control flow edges. Very few binaries have these instructions.
    # This is fixed in CUDA 12.4 but mark as XFAIL to support older versions of CUDA.
    if cupti_dep.found() and cupti_dep.version().version_compare('<12.4') and name in [
      'quicksilver-sm_87-dr+gpucfg',
    ]
      _should_fail = true
    endif

    test(
      f'Analysis of @name@ is consistent (-j@threads@@_suffix@)',
      _tst,
      args: _args + [f'-j@threads@'],
      suite: 'hpcstruct',
      should_fail: _should_fail,
    )
  endforeach
endforeach
