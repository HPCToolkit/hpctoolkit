subdir('v4.0')

testdata_dbase_current = testdata_dbase_4_0

testdata_dbase = {}
foreach ver : ['4.0']
  foreach name, spec : get_variable('testdata_dbase_'+ver.replace('.', '_'))
    testdata_dbase += {f'@name@-@ver@': spec}
  endforeach
endforeach
