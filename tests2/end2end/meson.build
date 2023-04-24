_tst = configure_file(input: files('tst-default-pipeline'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
foreach exe : [tstexe_1loop]
  test('Default analysis pipeline on @0@ works'.format(exe.name()),
       _tst, args: [exe],
       env: hpctoolkit_pyenv, suite: 'end2end', timeout: 90)
endforeach
