_tstexe_vecadd_hip = custom_target(output: 'tstexe-vecadd-hip', input: files('vecadd.hip.cpp'),
    command: hipcc_cmd)

_tst = configure_file(input: files('tst-produces-profiles-amd'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
test('Measurement of tstexe-vecadd-hip produces profiles',
     _tst, args: [_tstexe_vecadd_hip],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
test('Measurement of tstexe-vecadd-hip with tracing produces profiles',
     _tst, args: ['--trace', _tstexe_vecadd_hip],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
test('Measurement of tstexe-vecadd-hip with boosted tracing produces profiles',
     _tst, args: ['--ttrace', _tstexe_vecadd_hip],
     env: hpctoolkit_pyenv, suite: ['hpcrun', 'rocm'])
