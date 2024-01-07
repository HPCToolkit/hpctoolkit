# This directory provides `hpctesttool`, a super-command for a series of helpful tools for writing
# precise and robust test-scripts. And other debugging tool.

# Instantiate a venv to install the Python package into, at setup-time.
_venv = meson.current_build_dir() / '.venv'
run_command('rm', '-f', meson.current_build_dir() / 'hpctesttool', check: true)
run_command(python, '-m', 'venv', '--clear', _venv, check: true)
_venv_python = find_program(_venv / 'bin' / 'python')

# At build time, install the Python package into the venv.
hpctesttool = custom_target(
  'hpctesttool',
  output: 'hpctesttool',
  build_by_default: false,
  command: [find_program(files('install')), _venv, '@CURRENT_SOURCE_DIR@', '@OUTPUT@'],
  depend_files: [files('pyproject.toml')],
)
meson.add_devenv({'PATH': _venv / 'bin'}, method: 'prepend')

# Also run pytest to test the utilities themselves
_env = environment()
_env.set('TEST_DATA_DIR', meson.current_source_dir() / '../../tests2/data')
test(
  'pytest',
  _venv_python,
  timeout: 0,
  protocol: 'gtest',
  verbose: true,
  env: _env,
  args: [
    files('pytest-as-gtest.py'),
    '--override-ini=cache_dir=@0@'.format(meson.current_build_dir() / '.pytest_cache'),
    meson.current_source_dir() / 'hpctesttool',
  ],
  workdir: meson.current_build_dir(),
)
