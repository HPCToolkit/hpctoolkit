if not add_languages('cuda', native: false, required: get_option('cuda'))
  subdir_done()
endif

_exe_vecadd_cuda = executable('test-vecadd-cuda', files('vecadd.cu'))
test('vecadd-cuda', _exe_vecadd_cuda)
