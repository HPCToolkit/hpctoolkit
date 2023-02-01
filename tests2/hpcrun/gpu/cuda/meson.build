if not add_languages('cuda', native: false, required: get_option('cuda'))
  subdir_done()
endif

_exe_vecadd_cuda = executable('test-vecadd-cuda', files('vecadd.cu'))

_tst = find_program(files('tst-produces-profiles'))
test('Measurement of vecadd-cuda produces profiles', _tst,
     args: [_exe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'])
test('Measurement of vecadd-cuda w/ tracing produces profiles', _tst,
     args: ['--trace', _exe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'])
test('Measurement of vecadd-cuda w/ PC sampling produces profiles', _tst,
     args: ['--pc', _exe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'],
     is_parallel: false)  # CUPTI does not allow multiple processes to sample the GPU
test('Measurement of vecadd-cuda w/ PC + tracing produces profiles', _tst,
     args: ['--pc', '--trace', _exe_vecadd_cuda], env: hpctoolkit_pyenv, suite: ['hpcrun', 'cuda'],
     is_parallel: false)  # CUPTI does not allow multiple processes to sample the GPU
