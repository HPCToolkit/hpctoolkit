#ifndef _HPCTOOLKIT_SANITIZER_NODE_H_
#define _HPCTOOLKIT_SANITIZER_NODE_H_

// gpu_patch_buffer_t
// gpu_patch_record_t
#include <gpu-patch.h>
#include <hpcrun/cct/cct.h>
#include <cuda.h>
#include <vector_types.h>

#include "cstack.h"

typedef enum {
  SANITIZER_ACTIVITY_TYPE_MEMORY = 0,
  SANITIZER_ACTIVITY_TYPE_COUNT = 1
} sanitizer_activity_type_t;


// notification: host only
typedef struct sanitizer_entry_notification {
  CUmodule module;
  CUcontext context;
  CUstream stream;
  uint64_t function_addr;
  cstack_node_t *buffer_device;
  cct_node_t *host_op_node;
  dim3 grid_size;
  dim3 block_size;
} sanitizer_entry_notification_t;


// buffer: device only
typedef struct sanitizer_entry_buffer {
  CUcontext context;
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
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
);


void
sanitizer_notification_node_set
(
 cstack_node_t *node,
 CUmodule module,
 CUcontext context,
 CUstream stream,
 uint64_t function_addr, 
 cstack_node_t *buffer_device,
 cct_node_t *host_op_node,
 dim3 grid_size,
 dim3 block_size
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
