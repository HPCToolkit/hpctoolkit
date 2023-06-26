project('hpctsuite', 'c', 'cpp',
  meson_version: '>=0.64.0',
  license: 'BSD-3-Clause',
  default_options: ['c_std=gnu11', 'cpp_std=gnu++17', 'cuda_std=c++17'])

cpp = meson.get_compiler('cpp')
fs = import('fs')

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
if get_option('mpi').enabled()
  hpcprof_mpi = _hpctoolkit_root / 'bin' / 'hpcprof-mpi'
endif

# Find CUDA language support.
has_cuda = add_languages('cuda', native: false, required: get_option('cuda'))

# Find HIP language support.
hipcc = find_program('hipcc', required: get_option('rocm'))
hipcc_cmd = [hipcc, '-o', '@OUTPUT@', '@INPUT@']

# Find MPI. This is overcomplicated because Meson built-in MPI support is a bit rough
_mpi_test_code = '''
  #include <mpi.h>
  #include <stddef.h>
  int main() {
    MPI_Init(NULL, NULL);
    MPI_Finalize();
    return 0;
  }
'''
foreach lang : ['c', 'cpp']
  if dependency('MPI', required: false, language: lang).found()
    continue
  endif

  # On some systems (Cray) MPI is integrated into the base compiler.
  _comp = meson.get_compiler(lang)
  if _comp.links(_mpi_test_code, name: 'MPI is available by default')
    _Mver = _comp.get_define('MPI_VERSION', prefix: '#include <mpi.h>')
    _mver = _comp.get_define('MPI_SUBVERSION', prefix: '#include <mpi.h>')
    _ver = _Mver + (_mver == '' ? '' : '.') + _mver
    _ver = _ver == '' ? 'unknown' : _ver
    meson.override_dependency('MPI', declare_dependency(version: _ver), language: lang)
    continue
  endif

  # Built-in Meson doesn't support MPICH, which responds to -compile_info and -link_info.
  # Note that this is a fragile uncached test. It would be more robust to upstream support.
  _printenv = find_program('printenv')
  if lang == 'c'
    _names = [run_command(_printenv, 'MPICC', check: false).stdout().strip(), 'mpicc']
  elif lang == 'cpp'
    _names = [run_command(_printenv, 'MPICXX', check: false).stdout().strip(), 'mpicxx']
  endif
  foreach name : _names
    if name == ''
      continue
    endif
    _mpicc = find_program(name, required: false)
    if _mpicc.found()
      _c_info = run_command(_mpicc, '-compile_info')
      _l_info = run_command(_mpicc, '-link_info')
      if _c_info.returncode() == 0 and _l_info.returncode() == 0
        # Skip the first argument, it should be the compiler itself
        _c_args = ['!!sentinel!!']
        foreach arg : _c_info.stdout().strip().split(' ')
          if _c_args == ['!!sentinel!!']
            _c_args = []
          else
            _c_args += arg
          endif
        endforeach
        _l_args = ['!!sentinel!!']
        foreach arg : _l_info.stdout().strip().split(' ')
          if _l_args == ['!!sentinel!!']
            _l_args = []
          else
            _l_args += arg
          endif
        endforeach

        # Check that the dependency actually provides MPI
        _dep = declare_dependency(compile_args: _c_args, link_args: _l_args, version: 'unknown mpich-like')
        assert(_comp.links(_mpi_test_code, name: 'MPI validation check', dependencies: _dep),
               'Found something like MPICH but it doesn\'t seem to work')
        meson.override_dependency('MPI', _dep, language: lang)
        _names = []
        break
      endif
    endif
  endforeach
  if _names == []
    continue
  endif
endforeach
mpic = dependency('MPI', language: 'c', required: get_option('mpi'))
mpicpp = dependency('MPI', language: 'cpp', required: get_option('mpi'))

# See if we have runtime support for MPI, via the semi-standard mpiexec interface.
# Usage: [mpiexec, '<# of ranks>', command...]
if mpic.found() or mpicpp.found()
  mpiexec = find_program('mpiexec', required: false)
  if mpiexec.found()
    mpiexec_str = ';'.join(mpiexec.full_path(), '-host', 'localhost', '-n')
    mpiexec = [mpiexec, '-host', 'localhost', '-n']
  else
    _sh = find_program('sh')
    mpiexec_str = ';'.join(_sh.full_path(), '-c', 'exit 77')
    mpiexec = [_sh, '-c', 'exit 77']
  endif
else
  _sh = find_program('sh')
  mpiexec_str = ';'.join(_sh.full_path(), '-c', 'exit 77')
  mpiexec = [_sh, '-c', 'exit 77']
endif

# Utilities for running tests and such
subdir('lib')

# Data for running tests
subdir('data')

# Tests themselves
subdir('hpcrun')
subdir('hpcstruct')
subdir('hpcprof')
subdir('end2end')
