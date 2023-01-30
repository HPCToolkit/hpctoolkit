_simple = find_program(files('simple'))

foreach exe : [exe_1loop]
  test('End-to-end on @0@'.format(exe.name()), _simple, args: [exe],
    env: hpctoolkit_pyenv, suite: 'end2end', timeout: 90)
endforeach
