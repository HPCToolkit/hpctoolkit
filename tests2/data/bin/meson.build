testdata_bin = {}

keyval = import('keyval')
fs = import('fs')

foreach binary, tags : keyval.load(files('test-binaries' / 'contents'))
  testdata_bin += {binary: {
    'binary': files('test-binaries' / binary),
    'arch_tags': tags.split(),
  }}
endforeach
