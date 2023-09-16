if not get_option('hpcprof_mpi').enable_auto_if(mpicxx.found()).enabled()
  subdir_done()
endif

# Once HPCToolkit is installed, hpcrun will be available at this path. This wrapper executable
# allows Meson to "execute" it before installation, even though it will fail.
hpcprof_mpi = executable('hpcprof_mpi', exec_src,
    c_args: ['-DCMD_PATH=@0@'.format(get_option('prefix') / 'bin' / 'hpcprof-mpi')])
