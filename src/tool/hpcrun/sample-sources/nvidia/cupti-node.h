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

// activity entry
typedef struct cupti_entry_activity {
  CUpti_Activity *activity;
  cct_node_t *cct_node;
} cupti_entry_activity_t;

// notification entry
typedef struct cupti_entry_notification {
  int64_t host_op_id;
  cct_node_t *cct_node;
  void *record;
} cupti_entry_notification_t;

// activity allocator
extern
cupti_node_t *
cupti_activity_node_new
(
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cct_node_t *next
);


extern void
cupti_activity_node_set
(
 cupti_node_t *cupti_node,
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cct_node_t *next
);

// notification allocator
extern
cupti_node_t *
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
