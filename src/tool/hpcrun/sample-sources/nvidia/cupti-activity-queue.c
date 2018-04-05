#include <hpcrun/memory/hpcrun-malloc.h>
#include <lib/prof-lean/stdatomic.h>

#include "cupti-activity-queue.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

static __thread cupti_activity_queue_t cupti_activity_queue_worker = {
  .head = NULL,
  .tail = NULL
};

static __thread cupti_activity_queue_t cupti_activity_queue_cupti = {
  .head = NULL,
  .tail = NULL
};

static __thread bool cupti_activity_queue_worker_init = false;
static __thread bool cupti_activity_queue_cupti_init = false;


cupti_activity_queue_t *
cupti_activity_queue_worker_get()
{
  if (cupti_activity_queue_worker_init == false) {
    // TODO(keren): init
    atomic_init(&cupti_activity_queue_worker.head, NULL);
    atomic_init(&cupti_activity_queue_worker.tail, NULL);
    cupti_activity_queue_worker_init = true;
  }
  return &cupti_activity_queue_worker;
}


cupti_activity_queue_t *
cupti_activity_queue_cupti_get()
{
  if (cupti_activity_queue_cupti_init == false) {
    // TODO(keren): init
    atomic_init(&cupti_activity_queue_cupti.head, NULL);
    atomic_init(&cupti_activity_queue_cupti.tail, NULL);
    cupti_activity_queue_cupti_init = true;
  }
  return &cupti_activity_queue_cupti;
}


static cupti_activity_queue_entry_t *cupti_activity_queue_entry_new(CUpti_ActivityKind kind, void *activity, cct_node_t *cct_node)
{
  cupti_activity_queue_entry_t *entry = (cupti_activity_queue_entry_t *)hpcrun_malloc(sizeof(cupti_activity_queue_entry_t));
  atomic_init(&entry->next, NULL);
  entry->cct_node = cct_node;
  switch (kind) {
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    {
#if CUPTI_API_VERSION >= 10
      entry->activity = (CUpti_ActivityPCSampling3 *)hpcrun_malloc(sizeof(CUpti_ActivityPCSampling3));
      memcpy(entry->activity, activity, sizeof(CUpti_ActivityPCSampling3));
#else
      entry->activity = (CUpti_ActivityPCSampling2 *)hpcrun_malloc(sizeof(CUpti_ActivityPCSampling2));
      memcpy(entry->activity, activity, sizeof(CUpti_ActivityPCSampling2));
#endif
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      entry->activity = (CUpti_ActivityMemcpy *)hpcrun_malloc(sizeof(CUpti_ActivityMemcpy));
      memcpy(entry->activity, activity, sizeof(CUpti_ActivityMemcpy));
      break;
    }
    case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    {
      CUpti_ActivityUnifiedMemoryCounter *activity_unified = (CUpti_ActivityUnifiedMemoryCounter *)activity;
      entry->activity = (CUpti_ActivityUnifiedMemoryCounter *)hpcrun_malloc(sizeof(CUpti_ActivityUnifiedMemoryCounter));
      ((CUpti_ActivityUnifiedMemoryCounter *)entry->activity)->kind = CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER;
      ((CUpti_ActivityUnifiedMemoryCounter *)entry->activity)->counterKind = activity_unified->counterKind;
      ((CUpti_ActivityUnifiedMemoryCounter *)entry->activity)->timestamp = activity_unified->timestamp;
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      CUpti_ActivityKernel4 *activity_kernel = (CUpti_ActivityKernel4 *)activity;
      entry->activity = (CUpti_ActivityKernel4 *)hpcrun_malloc(sizeof(CUpti_ActivityKernel4));
      ((CUpti_ActivityKernel4 *)entry->activity)->kind = CUPTI_ACTIVITY_KIND_KERNEL;
      ((CUpti_ActivityKernel4 *)entry->activity)->start = activity_kernel->start;
      ((CUpti_ActivityKernel4 *)entry->activity)->end = activity_kernel->end;
      break;
    }
    default:
      break;
  }
  return entry;
}


cupti_activity_queue_entry_t *
cupti_activity_queue_pop(cupti_activity_queue_t *queue)
{
  cupti_activity_queue_entry_t *first = atomic_load(&queue->head);
  if (first) {
    cupti_activity_queue_entry_t *succ = atomic_load(&first->next);
    atomic_store(&queue->head, succ);
    return first;
  } else {
    return NULL;
  }
} 

// TODO(keren): replace explicit copy with memcpy
void
cupti_activity_queue_push(cupti_activity_queue_t *queue, CUpti_ActivityKind kind, void *activity, cct_node_t *cct_node)
{
  cupti_activity_queue_entry_t *entry = cupti_activity_queue_entry_new(kind, activity, cct_node);
  cupti_activity_queue_entry_t *last = atomic_load(&queue->tail);
  atomic_store(&entry->next, last);
  if (!atomic_compare_exchange_strong(&queue->tail, &last, entry)) {
    atomic_store(&entry->next, NULL);
    atomic_store(&queue->tail, entry);
  }
}


void
cupti_activity_queue_splice(cupti_activity_queue_t *queue, cupti_activity_queue_t *other)
{
  cupti_activity_queue_entry_t *other_last = atomic_exchange(&other->tail, NULL);
  cupti_activity_queue_entry_t *first = atomic_load(&queue->head);
  cupti_activity_queue_entry_t *last = atomic_load(&queue->tail);
  if (other_last) {
    cupti_activity_queue_entry_t *prev = NULL;
    cupti_activity_queue_entry_t *node = other_last;
    while (node) {
      cupti_activity_queue_entry_t *next = atomic_load(&node->next);
      atomic_store(&node->next, prev);
      prev = node;
      node = next;
    }
    if (first) {
      atomic_store(&last->next, prev);
    } else {
      atomic_store(&queue->head, prev);
    }
    atomic_store(&queue->tail, other_last);
  }
} 


void
cupti_activity_queue_apply(cupti_activity_queue_t *queue, cupti_activity_queue_fn fn)
{
  cupti_activity_queue_entry_t *entry = NULL;
  while ((entry = cupti_activity_queue_pop(queue))) {
    fn(entry->activity, entry->cct_node);
    // TODO(keren): add free-list
  }
}
