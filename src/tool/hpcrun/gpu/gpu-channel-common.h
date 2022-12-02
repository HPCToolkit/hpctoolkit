#ifndef GPU_CHANNEL_COMMON_H
#define GPU_CHANNEL_COMMON_H

// GPU_CHANNEL_TOTAL specifies the total number
// of correlation and activity channels an application
// thread will create.
// This is created for supporting AMD GPUs,
// where roctracer and rocprofiler will each create
// one monitoring thread.
// As the implementation of the channel is one-proceduer-one-consumer,
// we need an array of correlation and
// activity channel for each application thread.
// For platforms where there is just one monitoring
// thread, such as NVIDIA, the implementation maintains
// backward compatibility, where we will just use
// the first channel pair.
// Implementation wise, channel operations without _with_idx suffix
// represent old operations and will use channel 0
// Channel operations with _with_idx suffix requires a channel
// index to specify which channel to operate with

#define GPU_CHANNEL_TOTAL 2

#endif
