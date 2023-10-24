project('spack-deps', meson_version: '>=1.1.0', license: 'BSD-3-Clause')

fs = import('fs')

# Parse the specs from the spack.yaml file. The bits are marked to make parsing of the YAML easy.
_markers = ['SPEC']
if host_machine.cpu_family() in ['x86', 'x86_64']
  _markers += ['SPECX86']
endif
specs = {}
foreach line : fs.read(files('spack.yaml')).split('\n')
  foreach marker : _markers
    if line.strip().endswith(marker)
      _spec_bits = line.strip().split('#')[0].strip().strip('-').strip().strip('\'').split()
      specs += {_spec_bits[0]: ' '.join(_spec_bits)}
      break
    endif
  endforeach
endforeach

if get_option('spack_mode') == 'no'
  warning('spack_mode == no, not probing Spack for dependencies')
  subdir_done()
endif

# Decide on the Spack environment to use
if get_option('spack_env') == ''
  _spack_env = meson.current_build_dir()
  run_command('cp', files('spack.yaml'), _spack_env / 'spack.yaml', check: true)
else
  assert(fs.is_absolute(get_option('spack_env')), 'spack_env option must be an absolute path')
  assert(fs.is_dir(get_option('spack_env')), 'spack_env must refer to a directory')
  _spack_env = get_option('spack_env')
endif
spack = [find_program('spack', version: '>=0.20.1', native: true), '--env-dir', _spack_env]

# If we generated the environment, concretize it. Otherwise leave it to the user.
if get_option('spack_env') == '' \
   or (get_option('spack_mode') == 'install' and not fs.exists(_spack_env / 'spack.lock'))
  message(f'Spack concretizing the environment: @_spack_env@')
  run_command(spack, 'concretize', check: true)
endif

# Fetch install prefixes (and other goodies) for all the packages we want from the environment
_basic_deps = {}
foreach package, spec : specs
  _find_hash = run_command(spack, 'find', '--format={hash}', spec, check: false)
  if _find_hash.returncode() != 0 and get_option('spack_mode') == 'install'
    message(f'Spack installing package: @package@')
    run_command(spack, 'install', '--fail-fast', '--no-add', '--only-concrete', spec, check: true)
    _find_hash = run_command(spack, 'find', '--format={hash}', spec, check: false)
  endif
  if _find_hash.returncode() != 0
    error(f'Unable to find Spack installation for \'@package@\', is the environment installed?')
  else
    _hash = _find_hash.stdout().strip().split()[0]
    _find_prefix = run_command(spack, 'find', '--format={prefix}', f'/@_hash@', check: true)
    _find_version = run_command(spack, 'find', '--format={version}', f'/@_hash@', check: true)

    _link_args = []
    foreach libdir : ['lib64', 'lib']
      if fs.is_dir(_find_prefix.stdout().strip() / libdir)
        _link_args += ['-Wl,-rpath=@0@'.format(_find_prefix.stdout().strip() / libdir)]
      endif
    endforeach

    _basic_deps += {
      package: declare_dependency(
        version: _find_version.stdout().strip(),
        variables: {
          'prefix': _find_prefix.stdout().strip(),
          'link_args': ' '.join(_link_args),
        },
      )
    }
    message('Located Spack installation for @0@: @1@'.format(package, _find_prefix.stdout().strip()))
  endif
endforeach

# Expose the dependencies under their internal names (since they differ from Spack names)
# Normally we would expose these via meson.override_dependency, but since there are multiple
# dependencies here that runs into issues if a user tries to override any one. So we use the legacy
# variable-exposure syntax.
boost_dep = _basic_deps['boost']
bzip_dep = _basic_deps['bzip2']
dyninst_dep = _basic_deps['dyninst']
elfutils_dep = _basic_deps['elfutils']
tbb_dep = _basic_deps['intel-tbb']
libmonitor_dep = _basic_deps['libmonitor']
lzma_dep = _basic_deps['xz']
zlib_dep = _basic_deps['zlib']
libunwind_dep = _basic_deps['libunwind']
perfmon_dep = _basic_deps['libpfm4']
xerces_dep = _basic_deps['xerces-c']
libiberty_dep = _basic_deps['libiberty']
memkind_dep = _basic_deps['memkind']
yaml_cpp_dep = _basic_deps['yaml-cpp']
if 'intel-xed' in specs
  xed_dep = _basic_deps['intel-xed']
endif
