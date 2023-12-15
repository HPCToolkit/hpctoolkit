# Once HPCToolkit is installed, hpcstruct will be available at this path. This wrapper executable
# allows Meson to "execute" it before installation, even though it will fail.
hpcstruct = executable(
  'hpcstruct',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(get_option('prefix') / 'bin' / 'hpcstruct')],
)
