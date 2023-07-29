testdata_root = 'tests2/data/'
testdata_srcroot = meson.project_source_root()

subdir('meas')
subdir('dbase')
subdir('bin')
subdir('struct')

_contents = {
  'none': [
    testdata_meas['loops-cputime-t'], testdata_dbase_current['loops-cputime-t'],
    testdata_struct_current['small-inlines+loops-sm_62-nvcc102-0'],
    testdata_struct_current['small-inlines+loops-sm_62-nvcc102-dr'],
    testdata_struct_current['small-inlines+loops-sm_62-nvcc102-r'],
    testdata_struct_current['small-inlines+loops-sm_75-nvcc112-0'],
    testdata_struct_current['small-inlines+loops-sm_75-nvcc112-dr'],
    testdata_struct_current['small-inlines+loops-sm_75-nvcc112-r'],
    testdata_struct_current['small-inlines+loops-sm_87-nvcc118-0'],
    testdata_struct_current['small-inlines+loops-sm_87-nvcc118-dr'],
    testdata_struct_current['small-inlines+loops-sm_87-nvcc118-r'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc10-0'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc10-d'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc10-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc10-r'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc12-0'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc12-d'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc12-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc12-r'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc8-0'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc8-d'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc8-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-gcc8-r'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm11-0'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm11-d'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm11-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm11-r'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm15-0'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm15-d'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm15-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm15-r'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm7-0'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm7-d'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm7-dr'],
    testdata_struct_current['small-inlines+loops-x86_64-llvm7-r'],
  ],
  'cpu': [
    testdata_meas['small'], testdata_dbase_current['small'],
    testdata_meas['loops-perf-t'], testdata_dbase_current['loops-perf-t'],
  ],
}
if has_cuda
  _contents += {
    'nvidia': [
      testdata_meas['loops-cuda-nvidia-t'], testdata_dbase_current['loops-cuda-nvidia-t'],
      testdata_meas['loops-cuda-nvidiapc-t'], testdata_dbase_current['loops-cuda-nvidiapc-t'],
    ],
    'sw-cuda': [
      testdata_struct_current['small-inlines+loops-sm_62-nvcc102-0+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_62-nvcc102-dr+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_62-nvcc102-r+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_75-nvcc112-0+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_75-nvcc112-dr+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_75-nvcc112-r+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_87-nvcc118-0+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_87-nvcc118-dr+gpucfg'],
      testdata_struct_current['small-inlines+loops-sm_87-nvcc118-r+gpucfg'],
    ],
  }
endif
if has_rocm
  _contents += {
    'amd': [
      testdata_meas['loops-hip-amd-t'], testdata_dbase_current['loops-hip-amd-t'],
    ],
  }
endif

_make_tar = configure_file(input: files('make-tar'), output: '@PLAINNAME@.venv',
                           command: venv_shebang)
foreach suite, contents : _contents
  debug(f'Creating custom_target for fresh-testdata-@suite@.tar.xz')
  _cmd = [_make_tar, '@OUTPUT@']
  _i = 0
  foreach content : contents
    assert('gen' in content, f'Invalid content #@_i@ in suite @suite@')
    _i += 1
    foreach dstpath, srcgen : content['gen']['files']
      _cmd += ['--add', srcgen, dstpath]
    endforeach
  endforeach
  custom_target(output: f'fresh-testdata-@suite@.tar.xz', command: _cmd, build_by_default: false)
endforeach
