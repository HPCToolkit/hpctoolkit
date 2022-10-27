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

# Add the utilities environment to the devenv for manual execution
meson.add_devenv(hpctoolkit_pyenv)

# The utilities themselves also have unit tests based on pytest
test('pytest', python3,
  args: ['-m', 'pytest', meson.current_source_dir(), '--junitxml=pytest.junit.xml'],
  verbose: true, workdir: meson.current_build_dir())
