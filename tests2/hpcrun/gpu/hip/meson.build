exe_vecadd_hip = custom_target(
  output: 'test-vecadd-hip',
  input: files('vecadd.hip.cpp'),
  command: [hipcc, '-o', '@OUTPUT@', '@INPUT@'],
)
test('vecadd-hip', find_program('sh'), args: ['-c', '$0', exe_vecadd_hip])
