_exe_vecadd_hip = custom_target(output: 'test-vecadd-hip', input: files('vecadd.hip.cpp'),
    command: hipcc_cmd)
test('vecadd-hip', find_program('sh'), args: ['-c', '$0', _exe_vecadd_hip])
