if not add_languages('cuda', native: false, required: get_option('cuda'))
  subdir_done()
endif

_tstexe_vecadd_cuda = executable('tstexe-vecadd-cuda', files('vecadd.cu'))

_tst = find_program(files('tst-produces-profiles'))
test('Measurement of tstexe-vecadd-cuda produces profiles', _tst,
     args: [_tstexe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'])
test('Measurement of tstexe-vecadd-cuda w/ tracing produces profiles', _tst,
     args: ['--trace', _tstexe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'])
test('Measurement of tstexe-vecadd-cuda w/ PC sampling produces profiles', _tst,
     args: ['--pc', _tstexe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'],
     is_parallel: false)  # CUPTI does not allow multiple processes to sample the GPU
test('Measurement of tstexe-vecadd-cuda w/ PC + tracing produces profiles', _tst,
     args: ['--pc', '--trace', _tstexe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'],
     is_parallel: false)  # CUPTI does not allow multiple processes to sample the GPU
