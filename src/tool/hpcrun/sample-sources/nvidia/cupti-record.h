#ifndef _HPCTOOLKIT_CUPTI_RECORD_H_
#define _HPCTOOLKIT_CUPTI_RECORD_H_

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/bi_unordered_channel.h>

#include <cupti_activity.h>

#include "cupti-node.h"

typedef struct {
  s_element_ptr_t next;
  cupti_entry_correlation_t entry;
} typed_stack_elem(cupti_entry_correlation_t);

typedef struct {
  s_element_ptr_t next;
  cupti_entry_activity_t entry;
} typed_stack_elem(cupti_entry_activity_t);


typedef bi_unordered_channel_t typed_bi_unordered_channel(cupti_entry_activity_t);
typedef bi_unordered_channel_t typed_bi_unordered_channel(cupti_entry_correlation_t);

typed_bi_unordered_channel_declare(cupti_entry_correlation_t)
typed_bi_unordered_channel_declare(cupti_entry_activity_t)

typedef typed_stack_elem(cupti_entry_correlation_t) cupti_correlation_elem_t;
typedef typed_stack_elem(cupti_entry_activity_t) cupti_activity_elem_t;

typedef typed_bi_unordered_channel(cupti_entry_activity_t) activity_channel_t;
typedef typed_bi_unordered_channel(cupti_entry_correlation_t) correlation_channel_t;

#define correlation_bi_unordered_channel_pop typed_bi_unordered_channel_pop(cupti_entry_correlation_t)
#define correlation_bi_unordered_channel_push typed_bi_unordered_channel_push(cupti_entry_correlation_t)
#define correlation_bi_unordered_channel_steal typed_bi_unordered_channel_steal(cupti_entry_correlation_t)

#define activity_bi_unordered_channel_pop typed_bi_unordered_channel_pop(cupti_entry_activity_t)
#define activity_bi_unordered_channel_push typed_bi_unordered_channel_push(cupti_entry_activity_t)
#define activity_bi_unordered_channel_steal typed_bi_unordered_channel_steal(cupti_entry_activity_t)

// record type
typedef struct cupti_record {
  activity_channel_t activity_channel;
  correlation_channel_t correlation_channel;
} cupti_record_t;

// a list of thread records
typedef struct cupti_record_list {
  _Atomic(struct cupti_record_list *) next;
  cupti_record_t *record;
} cupti_record_list_t;

// apply functions
// cupti_worker_activity_apply
void
cupti_activities_consume
(
);

// worker_notification_apply
void
correlation_produce
(
 uint64_t host_op_id,
 cct_node_t *api_node,
 cct_node_t *func_node
);

//cupti-activity
void
cupti_activity_produce
(
 CUpti_Activity *activity,
 cct_node_t *cct_node,
 cupti_record_t *record
);

// cupti-notification
void
correlations_consume
(
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
