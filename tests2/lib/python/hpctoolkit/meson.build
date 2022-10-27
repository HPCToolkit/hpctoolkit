# Almost all tests use Python as the main scripting language. This Python package contains useful
# utilities for writing precise and robust test-scripts.

# PYTHONPATH to include to use the utilities listed here
hpctoolkit_pypath = fs.parent(meson.current_source_dir())
hpctoolkit_pyenv = environment()
hpctoolkit_pyenv.prepend('PYTHONPATH', hpctoolkit_pypath)

# The utilities use environment variables to identify the HPCToolkit applications
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCRUN', hpcrun)
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCSTRUCT', hpcstruct)
hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCPROF', hpcprof)
if is_variable('hpcprof_mpi')
  hpctoolkit_pyenv.set('HPCTOOLKIT_APP_HPCPROF_MPI', hpcprof_mpi)
endif

hpctoolkit_pyenv.set('HPCTOOLKIT_DEV_MPIEXEC', mpiexec_str)

# Add the utilities environment to the devenv for manual execution
meson.add_devenv(hpctoolkit_pyenv)

# Some of the unit tests need access to the test data dir
_env = hpctoolkit_pyenv
_env.set('TEST_DATA_DIR', meson.current_source_dir()/'..'/'..'/'..'/'data')

# The utilities themselves also have unit tests based on pytest
test('pytest', python3, timeout: 0,
  args: ['-m', 'pytest', meson.current_source_dir(), '--junitxml=pytest.junit.xml'],
  env: _env, verbose: true, workdir: meson.current_build_dir())
