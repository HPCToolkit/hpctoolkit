# hpcfnbounds is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcfnbounds = executable(
  'hpcfnbounds',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcfnbounds/hpcfnbounds')],
  link_depends: [autotools_tgt],
)
