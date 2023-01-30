_single_bin = find_program(files('single-bin'))

foreach bin : [exe_1loop]
  test('hpcstruct on @0@'.format(bin.name()), _single_bin, args: [bin],
    env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach
