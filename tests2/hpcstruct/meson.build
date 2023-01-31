_tst = find_program(files('tst-runs'))
foreach bin : [exe_1loop]
  test('Analysis of @0@ runs'.format(bin.name()), _tst, args: [bin],
       env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach
