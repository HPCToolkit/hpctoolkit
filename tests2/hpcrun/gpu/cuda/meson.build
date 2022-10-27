if not add_languages('cuda', native: false, required: get_option('cuda'))
  subdir_done()
endif

exe_vecadd_cuda = executable('test-vecadd-cuda', files('vecadd.cu'))
test('vecadd-cuda', exe_vecadd_cuda)
