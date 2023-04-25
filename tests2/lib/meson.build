subdir('python')

# Running thread-disruptive <command>... will run the command in an environment where only a single
# random thread is available to the command. Note that this uses affinity, so the command may run
# far slower than expected in this environment.
#
# Running `meson test --setup=thread-disruptive` runs all tests under this wrapper. Note that doing
# this slows down the tests and as such test timeouts are automatically increased.
thread_disruptive = find_program(files('thread-disruptive'))
add_test_setup('threaddisruptive',
    exe_wrapper: thread_disruptive,
    timeout_multiplier: 100)
