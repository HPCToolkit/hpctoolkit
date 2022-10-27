project('hpctsuite', 'cpp',
  meson_version: '>=0.62.2',
  license: 'BSD-3-Clause')

cpp = meson.get_compiler('cpp')
fs = import('fs')

# Ensure our dependencies are available
python3 = find_program('python3', native: true, version: ['>=3.10', '<4'])
run_command(python3, '-c', 'import pytest', check: true)

# The path to the HPCToolkit install must be specified with -Dhpctoolkit
if get_option('hpctoolkit') == ''
  error('-Dhpctoolkit must be specified and absolute!')
endif
_hpctoolkit_root = fs.expanduser(get_option('hpctoolkit'))
if not fs.is_absolute(_hpctoolkit_root)
  error('-Dhpctoolkit must be specified and absolute!')
endif

# Build paths to the HPCToolkit applications for later use
hpcrun = _hpctoolkit_root / 'bin' / 'hpcrun'
hpcstruct = _hpctoolkit_root / 'bin' / 'hpcstruct'
hpcprof = _hpctoolkit_root / 'bin' / 'hpcprof'

# Load CUDA language support
has_cuda = add_languages('cuda', native: false, required: get_option('cuda'))
if has_cuda
  _cuda_version = meson.get_compiler('cuda').version().split()[0]
  _cuda_min_driver = import('unstable-cuda').min_driver_version(_cuda_version)
  message(f'Compiling for CUDA @_cuda_version@, driver must be at least @_cuda_min_driver@')
endif

# Find hipcc
hipcc = find_program('hipcc', required: get_option('rocm'))
has_hip = hipcc.found()

subdir('lib/python/hpctoolkit')
subdir('hpcrun')
subdir('data')  # data comes after hpcrun to reuse binaries used in hpcrun testing
subdir('hpcstruct')
subdir('hpcprof')
subdir('end2end')
