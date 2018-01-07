#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-activity-queue.h"

static __thread cupti_activity_queue_entry_t *cupti_activity_queue = NULL;

cupti_activity_queue_entry_t **
cupti_activity_queue_head()
{
  return &cupti_activity_queue;
}

void
cupti_activity_queue_push(cupti_activity_queue_entry_t **queue, CUpti_Activity *activity, cct_node_t *cct_node)
{
  cupti_activity_queue_entry_t *entry = (cupti_activity_queue_entry_t *)hpcrun_malloc(sizeof(cupti_activity_queue_entry_t));
  entry->activity = activity;
  entry->cct_node = cct_node;
  entry->next = *queue;
  *queue = entry;
}

void
cupti_activity_queue_apply(cupti_activity_queue_fn fn)
{
  cupti_activity_queue_entry_t *entry = cupti_activity_queue;
  while (entry != NULL) {
    fn(entry->activity, entry->cct_node);
    entry = entry->next;
  }
}
