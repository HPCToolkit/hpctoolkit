#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-activity-queue.h"

static __thread cupti_activity_queue_entry_t *cupti_activity_queue = NULL;

cupti_activity_queue_entry_t **
cupti_activity_queue_head()
{
  return &cupti_activity_queue;
}

// TODO(keren): replace explicit copy with memcpy
void
cupti_activity_queue_push(cupti_activity_queue_entry_t **queue, CUpti_Activity *activity, cct_node_t *cct_node)
{
  cupti_activity_queue_entry_t *entry = (cupti_activity_queue_entry_t *)hpcrun_malloc(sizeof(cupti_activity_queue_entry_t));
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    {
#if CUPTI_API_VERSION >= 10
      CUpti_ActivityPCSampling3 *activity_sample = (CUpti_ActivityPCSampling3 *)activity;
      entry->activity = (CUpti_ActivityPCSampling3 *)hpcrun_malloc(sizeof(CUpti_ActivityPCSampling3));
      ((CUpti_ActivityPCSampling3 *)entry->activity)->kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING;
      ((CUpti_ActivityPCSampling3 *)entry->activity)->stallReason = activity_sample->stallReason;
      ((CUpti_ActivityPCSampling3 *)entry->activity)->samples = activity_sample->samples;
      ((CUpti_ActivityPCSampling3 *)entry->activity)->latencySamples = activity_sample->latencySamples;
#else
      CUpti_ActivityPCSampling2 *activity_sample = (CUpti_ActivityPCSampling2 *)activity;
      entry->activity = (CUpti_ActivityPCSampling2 *)hpcrun_malloc(sizeof(CUpti_ActivityPCSampling2));
      ((CUpti_ActivityPCSampling2 *)entry->activity)->kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING;
      ((CUpti_ActivityPCSampling2 *)entry->activity)->stallReason = activity_sample->stallReason;
      ((CUpti_ActivityPCSampling2 *)entry->activity)->samples = activity_sample->samples;
      ((CUpti_ActivityPCSampling2 *)entry->activity)->latencySamples = activity_sample->latencySamples;
#endif
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      CUpti_ActivityMemcpy *activity_memcpy = (CUpti_ActivityMemcpy *)activity;
      entry->activity = (CUpti_ActivityMemcpy *)hpcrun_malloc(sizeof(CUpti_ActivityMemcpy));
      ((CUpti_ActivityMemcpy *)entry->activity)->kind = CUPTI_ACTIVITY_KIND_MEMCPY;
      ((CUpti_ActivityMemcpy *)entry->activity)->copyKind = activity_memcpy->copyKind;
      ((CUpti_ActivityMemcpy *)entry->activity)->bytes = activity_memcpy->bytes;
      ((CUpti_ActivityMemcpy *)entry->activity)->end = activity_memcpy->end;
      ((CUpti_ActivityMemcpy *)entry->activity)->start = activity_memcpy->start;
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
