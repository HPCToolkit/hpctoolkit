#ifndef _HPCTOOLKIT_SANITIZER_NODE_H_
#define _HPCTOOLKIT_SANITIZER_NODE_H_

#include <hpcrun/cct/cct.h>
#include <cuda.h>
#include <vector_types.h>

#include "cstack.h"

#define MAX_ACCESS_SIZE (16 + 1)

typedef enum {
  SANITIZER_ACTIVITY_TYPE_MEMORY = 0,
  SANITIZER_ACTIVITY_TYPE_COUNT = 1
} sanitizer_activity_type_t;


// sanitizer buffers
typedef struct sanitizer_memory_buffer {
  uint64_t pc;
  uint64_t address;
  uint32_t size;
  uint32_t flags;
  uint8_t value[MAX_ACCESS_SIZE];  // STS.128->16 bytes
  dim3 thread_ids;
  dim3 block_ids;
} sanitizer_memory_buffer_t;


typedef struct sanitizer_buffer {
  uint32_t cur_index;
  uint32_t max_index;
  uint32_t *thread_hash_locks;
  uint32_t block_sampling_frequency;
  void **prev_buffers;
  void *buffers;
} sanitizer_buffer_t;


// notification: host only
typedef struct sanitizer_entry_notification {
  CUmodule module;
  CUstream stream;
  uint32_t function_index;
  cct_node_t *host_op_node;
  sanitizer_activity_type_t type;
  cstack_node_t *buffer_device;
} sanitizer_entry_notification_t;


// buffer: device only
typedef struct sanitizer_entry_buffer {
  void *buffer;
} sanitizer_entry_buffer_t;


// sanitizer activities
typedef struct sanitizer_memory {
  uint64_t hits;
} sanitizer_memory_t;


typedef struct sanitizer_activity {
  sanitizer_activity_type_t type;
  union {
    sanitizer_memory_t memory;
  } data;
} sanitizer_activity_t;


cstack_node_t *
sanitizer_notification_node_new
(
 CUmodule module,
 CUstream stream,
 uint32_t function_index,
 cct_node_t *host_op_node,
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
);


void
sanitizer_notification_node_set
(
 cstack_node_t *node,
 CUmodule module,
 CUstream stream,
 uint32_t function_index,
 cct_node_t *host_op_node,
 sanitizer_activity_type_t type,
 cstack_node_t *buffer_device
);



cstack_node_t *
sanitizer_buffer_node_new
(
 void *buffer
);


void
sanitizer_buffer_node_set
(
 cstack_node_t *node,
 void *buffer
);

#endif
