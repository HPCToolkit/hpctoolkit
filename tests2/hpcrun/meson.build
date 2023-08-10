_args = []
if has_rocm
  _args += ['--rocprofiler-path', rocm_profiler_exdep.get_variable(internal: 'prefix')]
endif

_tst = configure_file(input: files('tst-list-metrics'), output: '@PLAINNAME@.venv',
                      command: venv_shebang)
test('Listing metrics does not error', _tst, args: _args, env: hpctoolkit_pyenv, suite: ['hpcrun'])

subdir('cpu')
subdir('gpu/cuda')
subdir('gpu/hip')

if get_option('python').enable_auto_if(python.found()).enabled()
  subdir('python')
endif
