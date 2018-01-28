#ifndef _HPCTOOLKIT_CUPTI_ACTIVITY_QUEUE_
#define _HPCTOOLKIT_CUPTI_ACTIVITY_QUEUE_

#include <cupti.h>
#include <cupti_activity.h>
#include <hpcrun/cct2metrics.h>

typedef struct cupti_activity_queue_entry {
  struct cupti_activity_queue_entry *next;
  CUpti_Activity *activity;
  cct_node_t *cct_node;
} cupti_activity_queue_entry_t;

typedef void (*cupti_activity_queue_fn)(CUpti_Activity* activity, cct_node_t *cct_node);

extern cupti_activity_queue_entry_t **cupti_activity_queue_head();
extern void cupti_activity_queue_push(cupti_activity_queue_entry_t **queue, CUpti_Activity *activity, cct_node_t *cct_node);
extern void cupti_activity_queue_apply(cupti_activity_queue_fn fn);

#endif

