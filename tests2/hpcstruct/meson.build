_tst = find_program(files('tst-runs'))
foreach bin : [tstexe_1loop]
  test('Analysis of @0@ runs'.format(bin.name()), _tst, args: [bin],
       env: hpctoolkit_pyenv, suite: 'hpcstruct')
endforeach
