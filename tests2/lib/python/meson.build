# Almost all tests use Python as the main scripting language. This Python package contains useful
# utilities for writing precise and robust test-scripts.

# Install the Python module in the build directory somewhere
run_command(files('setup-python'), files('pyproject.toml'), meson.current_build_dir() / 'venv',
            check: true)
venv_bindir = meson.current_build_dir() / 'venv' / 'bin'
venv_python = find_program(venv_bindir / 'python3')

# Configure command used to transform the shebangs of test scripts
venv_shebang = [venv_python, files('rewrite-shebang'), venv_python.full_path(), '@INPUT@', '@OUTPUT@']

# The utilities use environment variables to identify the HPCToolkit applications
hpctoolkit_pyenv = environment()
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCRUN', hpcrun)
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCSTRUCT', hpcstruct)
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCPROF', hpcprof)
if is_variable('hpcprof_mpi')
  hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCPROF_MPI', hpcprof_mpi)
endif

hpctoolkit_pyenv.set('HPCTOOLKIT_DEV_MPIEXEC', mpiexec_str)

# Add the utilities environment to the devenv for manual execution
meson.add_devenv(hpctoolkit_pyenv)

# The utilities themselves also have unit tests based on pytest
_env = hpctoolkit_pyenv
_env.set('TEST_DATA_DIR', meson.current_source_dir()/'..'/'..'/'data')
_pytst = configure_file(input: files('tst-pytest'), output: '@PLAINNAME@.venv',
                        command: venv_shebang)
test('pytest', _pytst, timeout: 0, protocol: 'gtest',
  args: [meson.current_source_dir() / 'hpctoolkit'],
  env: _env, verbose: true, workdir: meson.current_build_dir())
