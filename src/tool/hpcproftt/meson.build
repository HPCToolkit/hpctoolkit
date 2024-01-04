# hpcproftt is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcproftt = executable(
  'hpcproftt',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcproftt/hpcproftt')],
  link_depends: [autotools_tgt],
)

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
