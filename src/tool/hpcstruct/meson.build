# hpcstruct is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcstruct = executable(
  'hpcstruct',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcstruct/hpcstruct')],
  link_depends: [autotools_tgt],
)

hpcstruct_test_depends = [hpcproflm]
test_depends += hpcstruct_test_depends
_env = {
  'HPCTOOLKIT_HPCSTRUCT': hpcstruct.full_path(),
  'HPCTOOLKIT_HPCPROFLM': hpcproflm.full_path(),
}
hpcstruct_test_env = environment(_env)
foreach k,v : _env
  test_env.set(k, v)
endforeach

_devenv = hpcstruct_test_env
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
