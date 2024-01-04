if not get_option('hpcprof_mpi').enable_auto_if(mpicxx.found()).enabled()
  subdir_done()
endif

# hpcprof-mpi is built by Autotools. This executable wraps it so we can use functions that
# expect an executable instead of a string-path.
hpcprof_mpi = executable(
  'hpcprof-mpi',
  exec_src,
  c_args: ['-DCMD_PATH=@0@'.format(autotools_build / 'src/tool/hpcprof-mpi/hpcprof-mpi')],
  link_depends: [autotools_tgt],
)

_devenv = environment()
_devenv.prepend('PATH', meson.current_build_dir())
meson.add_devenv(_devenv)
