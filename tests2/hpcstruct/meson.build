foreach bin : [exe_1loop]
  test('hpcstruct on @0@'.format(bin.name()), python3, args: [files('single-bin'), bin],
    env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach
