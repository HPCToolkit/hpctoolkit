_tst = find_program(files('tst-default-pipeline'))
foreach exe : [tstexe_1loop]
  test('Default analysis pipeline on @0@ works'.format(exe.name()), _tst, args: [exe],
       env: hpctoolkit_pyenv, suite: 'end2end', timeout: 90)
endforeach
