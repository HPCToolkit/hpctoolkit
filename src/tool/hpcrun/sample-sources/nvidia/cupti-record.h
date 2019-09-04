#ifndef _HPCTOOLKIT_CUPTI_RECORD_H_
#define _HPCTOOLKIT_CUPTI_RECORD_H_

#include <lib/prof-lean/stdatomic.h>
#include <cupti_activity.h>
#include "cupti-stack.h"

// record type
typedef struct cupti_record {
  cupti_stack_t worker_notification_stack;
  cupti_stack_t worker_free_notification_stack;
  cupti_stack_t worker_activity_stack;
  cupti_stack_t worker_free_activity_stack;
  cupti_stack_t cupti_notification_stack;
  cupti_stack_t cupti_free_notification_stack;
  cupti_stack_t cupti_activity_stack;
  cupti_stack_t cupti_free_activity_stack;
  cupti_stack_t cupti_buffer_stack;
} cupti_record_t;

// a list of thread records
typedef struct cupti_record_list {
  _Atomic(struct cupti_record_list *) next;
  cupti_record_t *record;
} cupti_record_list_t;

// apply functions
void
cupti_worker_activity_apply
(
 cupti_stack_fn_t fn
);


void
cupti_worker_notification_apply
(
 uint64_t host_op_id,
 cct_node_t *cct_node
);


void
cupti_cupti_activity_apply
(
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cupti_record_t *record
);


void
cupti_cupti_notification_apply
(
 cupti_stack_fn_t fn
);

// getters
// called in target_callback before all events
void
cupti_worker_record_init
(
);


void
cupti_record_init
(
);


cupti_record_t *
cupti_record_get
(
);

#endif
