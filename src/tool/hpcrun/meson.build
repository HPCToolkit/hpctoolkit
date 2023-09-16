# Once HPCToolkit is installed, hpcrun will be available at this path. This wrapper executable
# allows Meson to "execute" it before installation, even though it will fail.
hpcrun = executable('hpcrun', exec_src,
                    c_args: ['-DCMD_PATH=@0@'.format(get_option('prefix') / 'bin' / 'hpcrun')])
