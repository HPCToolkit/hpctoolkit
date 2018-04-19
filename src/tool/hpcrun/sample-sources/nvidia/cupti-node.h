#ifndef _HPCTOOLKIT_CUPTI_NODE_H_
#define _HPCTOOLKIT_CUPTI_NODE_H_

#include <cupti_activity.h>
#include <hpcrun/cct2metrics.h>

typedef enum {
  CUPTI_ENTRY_TYPE_ACTIVITY = 1,
  CUPTI_ENTRY_TYPE_NOTIFICATION = 2,
  CUPTI_ENTRY_TYPE_COUNT = 3
} cupti_entry_type_t;

// generic entry
typedef struct cupti_node {
  struct cupti_node *next;
  void *entry;
  cupti_entry_type_t type;
} cupti_node_t;

// pc sampling
typedef struct cupti_pc_sampling {
  uint32_t samples;
  uint32_t latencySamples;
  CUpti_ActivityPCSamplingStallReason stallReason;
} cupti_pc_sampling_t;

// memory
typedef struct cupti_memcpy {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint8_t copyKind;
} cupti_memcpy_t;

// kernel
typedef struct cupti_kernel {
  uint64_t start;
  uint64_t end;
  int32_t dynamicSharedMemory;
  int32_t staticSharedMemory;
  int32_t localMemoryTotal;
} cupti_kernel_t;

// generic activity entry
typedef struct cupti_activity {
  CUpti_ActivityKind kind;
  union {
    cupti_pc_sampling_t pc_sampling;
    cupti_memcpy_t memcpy;
    cupti_kernel_t kernel;
  } data;
} cupti_activity_t;

// activity entry
typedef struct cupti_entry_activity {
  cupti_activity_t activity;
  cct_node_t *cct_node;
} cupti_entry_activity_t;

// notification entry
typedef struct cupti_entry_notification {
  int64_t host_op_id;
  cct_node_t *cct_node;
  void *record;
} cupti_entry_notification_t;

// activity allocator
extern cupti_node_t *
cupti_activity_node_new
(
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cupti_node_t *next
);


extern void
cupti_activity_node_set
(
 cupti_node_t *cupti_node,
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cupti_node_t *next
);

// notification allocator
extern cupti_node_t *
cupti_notification_node_new
(
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
);


extern void
cupti_notification_node_set
(
 cupti_node_t *cupti_node,
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
);

#endif
