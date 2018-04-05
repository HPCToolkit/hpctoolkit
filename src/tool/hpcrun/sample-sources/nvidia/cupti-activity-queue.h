#ifndef _HPCTOOLKIT_CUPTI_ACTIVITY_QUEUE_
#define _HPCTOOLKIT_CUPTI_ACTIVITY_QUEUE_

#include <cupti.h>
#include <cupti_activity.h>
#include <hpcrun/cct2metrics.h>
#include <lib/prof-lean/stdatomic.h>

typedef struct cupti_activity_queue_entry {
  _Atomic(struct cupti_activity_queue_entry *) next;
  void *activity;
  cct_node_t *cct_node;
} cupti_activity_queue_entry_t;


typedef struct cupti_activity_queue {
  _Atomic(struct cupti_activity_queue_entry *) head;
  _Atomic(struct cupti_activity_queue_entry *) tail;
} cupti_activity_queue_t;


typedef void (*cupti_activity_queue_fn)(CUpti_Activity* activity, cct_node_t *cct_node);

extern cupti_activity_queue_t *cupti_activity_queue_cupti_get();
extern cupti_activity_queue_t *cupti_activity_queue_worker_get();
extern cupti_activity_queue_entry_t *cupti_activity_queue_pop(cupti_activity_queue_t *queue);
extern void cupti_activity_queue_splice(cupti_activity_queue_t *queue, cupti_activity_queue_t *other);
extern void cupti_activity_queue_push(cupti_activity_queue_t *queue, CUpti_ActivityKind kind, void *activity, cct_node_t *cct_node);
extern void cupti_activity_queue_apply(cupti_activity_queue_t *queue, cupti_activity_queue_fn fn);

#endif

