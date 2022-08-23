_run = find_program(files('run.py'), native: true)

_cpu_metrics = {
  'cputime': ['-e', 'CPUTIME'],
  'cycles': ['-e', 'cycles'],
}

_cpu_metrics_tracing = {}
foreach slug, metrics : _cpu_metrics
  _cpu_metrics_tracing += {slug+'-t': metrics + ['-t']}
endforeach

_bundles = []
testdata_meas = []
foreach slug, flags : (_cpu_metrics + _cpu_metrics_tracing)
  _name = f'1loop-@slug@.tar.xz'
  _bundles += custom_target(output: _name,
      command: [_run, '--', '@OUTPUT@'] + metrics + [exe_1loop],
      env: hpctoolkit_pyenv, build_always_stale: true, build_by_default: false)
  testdata_meas += [files(_name)]
endforeach

alias_target('gen-testdata', _bundles)
