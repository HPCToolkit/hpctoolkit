#ifndef _HPCTOOLKIT_CUPTI_CHANNEL_H_
#define _HPCTOOLKIT_CUPTI_CHANNEL_H_

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/bi_unordered_channel.h>

#include <cupti_activity.h>

#include "cupti-node.h"

#define declare_typed_channel(type, ...) \
  typedef struct { \
    s_element_ptr_t next; \
    cupti_entry_##type##_t entry; \
  } typed_stack_elem(cupti_entry_##type##_t); \
\
  typedef bi_unordered_channel_t typed_bi_unordered_channel(cupti_entry_##type##_t); \
\
  typedef typed_stack_elem(cupti_entry_##type##_t) cupti_##type##_channel_elem_t; \
\
  typedef typed_bi_unordered_channel(cupti_entry_##type##_t) cupti_##type##_channel_t; \
\
  typed_bi_unordered_channel_declare(cupti_entry_##type##_t); \
\
  void cupti_##type##_channel_produce(cupti_##type##_channel_t *channel, __VA_ARGS__); \
  void cupti_##type##_channel_consume(cupti_##type##_channel_t *channel); \
  void cupti_##type##_channel_init(); \
  cupti_##type##_channel_t *cupti_##type##_channel_get()


declare_typed_channel(activity, CUpti_Activity *activity, cct_node_t *cct_node);
declare_typed_channel(correlation, uint64_t host_op_id,
  cupti_activity_channel_t *activity_channel, cct_node_t *api_node, cct_node_t *func_node, cct_node_t *sync_node);
declare_typed_channel(trace, uint64_t start, uint64_t end, cct_node_t *cct_node);

#endif
