subdir('python/hpctoolkit')

# Running (cuda|hip)-filter <command>... will run the command in an environment where
# (CUDA|HIP)_VISIBLE_DEVICES is set to limit device
cuda_filter = executable('testutil-cuda-filter', files('filter.cu'),
    dependencies: cuda_dep)

# Running thread-disruptive <command>... will run the command in an environment where only a single
# random thread is available to the command. Note that this uses affinity, so the command may run
# far slower than expected in this environment.
#
# Running `meson test --setup=thread-disruptive` runs all tests under this wrapper. Note that doing
# this slows down the tests and as such test timeouts are automatically increased.
thread_disruptive = [python3, files('thread-disruptive.py')]
add_test_setup('threaddisruptive',
    exe_wrapper: [python3, meson.current_source_dir()/'thread-disruptive.py'],
    timeout_multiplier: 100)
