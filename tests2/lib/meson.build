subdir('python/hpctoolkit')

# Running (cuda|hip)-filter <command>... will run the command in an environment where
# (CUDA|HIP)_VISIBLE_DEVICES is set to limit device
cuda_filter = executable('testutil-cuda-filter', files('filter.cu'),
    dependencies: cuda_dep)
