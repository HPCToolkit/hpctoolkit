# hpcproflm is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcproflm = executable(
  'hpcproflm',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcproflm/hpcproflm')],
  link_depends: [autotools_tgt],
)
