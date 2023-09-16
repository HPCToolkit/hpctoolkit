if not has_rocm
  subdir_done()
endif

_tstexe_vecadd_hip = custom_target(output: 'tstexe-vecadd-hip', input: files('vecadd.hip.cpp'),
    command: hipcc_cmd)

_preargs = ['--rocprofiler-path', rocm_profiler_exdep.get_variable(internal: 'prefix')]

_tst = find_program(files('tst-amd-vecadd-produces-profiles'))
test('Measurement of tstexe-vecadd-hip produces profiles',
     _tst, args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, ''] + _preargs,
     suite: ['hpcrun', 'rocm'])
test('Measurement of tstexe-vecadd-hip with tracing produces profiles',
     _tst, args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, '-t'] + _preargs,
     suite: ['hpcrun', 'rocm'])
test('Measurement of tstexe-vecadd-hip with boosted tracing produces profiles',
     _tst, args: [hpctesttool, hpcrun, _tstexe_vecadd_hip, '-tt'] + _preargs,
     suite: ['hpcrun', 'rocm'])
