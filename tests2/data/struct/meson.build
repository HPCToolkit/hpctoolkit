subdir('v4.8')

testdata_struct_current = testdata_struct_4_8

testdata_struct = {}
foreach ver : ['4.8']
  foreach name, spec : get_variable('testdata_struct_'+ver.replace('.', '_'))
    testdata_struct += {f'@name@-@ver@': spec}
  endforeach
endforeach
