_tst = find_program(files('tst-default-pipeline'))
foreach exe : [tstexe_1loop]
  test(
    'Default analysis pipeline on @0@ works'.format(exe.name()),
    _tst,
    args: [hpctesttool, hpcrun, hpcstruct, hpcprof, exe],
    suite: 'end2end',
    timeout: 90,
    env: test_env,
    depends: test_depends,
  )
endforeach
