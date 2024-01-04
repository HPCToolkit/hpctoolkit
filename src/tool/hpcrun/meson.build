# hpcrun is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcrun = executable(
  'hpcrun',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcrun/hpcrun')],
  link_depends: [autotools_tgt],
)

hpcrun_test_depends = [hpcfnbounds]
test_depends += hpcrun_test_depends
_env = {'HPCRUN_FNBOUNDS_CMD': hpcfnbounds.full_path()}
hpcrun_test_env = environment(_env)
foreach k,v : _env
  test_env.set(k, v)
endforeach

_devenv = hpcrun_test_env
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
