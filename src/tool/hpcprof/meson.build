# hpcprof is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcprof = executable(
  'hpcprof',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcprof/hpcprof')],
  link_depends: [autotools_tgt],
)

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
