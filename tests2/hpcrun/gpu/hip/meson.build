_tstexe_vecadd_hip = custom_target(output: 'tstexe-vecadd-hip', input: files('vecadd.hip.cpp'),
    command: hipcc_cmd)

_tst = find_program(files('tst-produces-profiles'))
test('Measurement of tstexe-vecadd-hip produces profiles', _tst,
     args: [_tstexe_vecadd_hip], env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
test('Measurement of tstexe-vecadd-hip with tracing produces profiles', _tst,
     args: ['--trace', _tstexe_vecadd_hip], env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
