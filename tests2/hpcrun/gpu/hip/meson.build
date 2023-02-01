_exe_vecadd_hip = custom_target(output: 'test-vecadd-hip', input: files('vecadd.hip.cpp'),
    command: hipcc_cmd)

_tst = find_program(files('tst-produces-profiles'))
test('Measurement of vecadd-hip produces profiles', _tst,
     args: [_exe_vecadd_hip], env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
test('Measurement of vecadd-hip with tracing produces profiles', _tst,
     args: ['--trace', _exe_vecadd_hip], env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
