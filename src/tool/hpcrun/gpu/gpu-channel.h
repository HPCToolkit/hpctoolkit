#ifndef _HPCTOOLKIT_GPU_CHANNEL_H_
#define _HPCTOOLKIT_GPU_CHANNEL_H_

#include <lib/prof-lean/bichannel.h>

#define declare_typed_channel(type, ...) \
  typedef struct { \
    s_element_ptr_t next; \
    gpu_##type##_t entry; \
  } typed_stack_elem(gpu_entry_##type##_t); \
\
  typedef bi_unordered_channel_t typed_bi_unordered_channel(gpu_entry_##type##_t); \
\
  typedef typed_stack_elem(gpu_entry_##type##_t) gpu_##type##_channel_elem_t; \
\
  typedef typed_bi_unordered_channel(gpu_entry_##type##_t) gpu_##type##_channel_t; \
\
  typed_bi_unordered_channel_declare(gpu_entry_##type##_t); \
\
  void gpu_##type##_channel_produce(gpu_##type##_channel_t *channel, __VA_ARGS__); \
  void gpu_##type##_channel_consume(gpu_##type##_channel_t *channel); \
  void gpu_##type##_channel_init(); \
  gpu_##type##_channel_t *gpu_##type##_channel_get()

#endif
