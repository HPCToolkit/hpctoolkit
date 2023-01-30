foreach exe : [exe_1loop]
  test('End-to-end on @0@'.format(exe.name()), python3, args: [files('simple'), exe],
    env: hpctoolkit_pyenv, suite: 'end2end', timeout: 90)
endforeach
