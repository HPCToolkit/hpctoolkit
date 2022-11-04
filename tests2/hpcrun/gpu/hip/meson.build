hipcc = find_program('hipcc', required: get_option('rocm'))
if not hipcc.found()
  subdir_done()
endif

exe_vecadd_hip = custom_target(
  output: 'test-vecadd-hip',
  input: files('vecadd.hip.cpp'),
  command: [hipcc, '-o', '@OUTPUT@', '@INPUT@'],
)
test('vecadd-hip', find_program('sh'), args: ['-c', '$0', exe_vecadd_hip])
