_tst = configure_file(input: files('tst-list-metrics'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
test('Listing metrics does not error', _tst, env: hpctoolkit_pyenv, suite: ['hpcrun'],
     should_fail:
      # FIXME: hpcrun -L currently doesn't work if `librocprofiler64.so` is a linker script, which changed
      # in the ROCm 5.6.0 release.
      # See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/784
      has_rocm and not fs.is_symlink(rocm_profiler_exdep.get_variable(internal: 'prefix') / 'lib' / 'librocprofiler64.so'))

subdir('cpu')
subdir('gpu/cuda')
subdir('gpu/hip')

if get_option('python').enable_auto_if(python.found()).enabled()
  subdir('python')
endif
