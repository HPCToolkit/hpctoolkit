testdata_bin = {}

keyval = import('keyval')
fs = import('fs')

has_testbin = fs.is_file(meson.current_source_dir() / 'test-binaries' / 'contents')

if has_testbin
  foreach binary, tags : keyval.load(files('test-binaries' / 'contents'))
    testdata_bin += {binary: {
      'binary': files('test-binaries' / binary),
      'arch_tags': tags.split(),
    }}
  endforeach
endif
