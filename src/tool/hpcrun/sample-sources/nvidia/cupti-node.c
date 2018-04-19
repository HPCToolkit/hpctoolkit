#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>
#include <cupti_activity.h>
#include "cupti-node.h"

void
cupti_activity_node_set
(
 cupti_node_t *cupti_node,
 CUpti_Activity *activity, 
 cct_node_t *cct_node,
 cupti_node_t *next
)
{
  cupti_entry_activity_t *entry = (cupti_entry_activity_t *)(cupti_node->entry);
  entry->cct_node = cct_node;
  cupti_node->next = next;
  cupti_node->type = CUPTI_ENTRY_TYPE_ACTIVITY;
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    {
#if CUPTI_API_VERSION >= 10
      CUpti_ActivityPCSampling3 *activity_sample = (CUpti_ActivityPCSampling3 *)activity;
#else
      CUpti_ActivityPCSampling2 *activity_sample = (CUpti_ActivityPCSampling2 *)activity;
#endif
      entry->activity.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING;
      entry->activity.data.pc_sampling.stallReason = activity_sample->stallReason;
      entry->activity.data.pc_sampling.samples = activity_sample->samples;
      entry->activity.data.pc_sampling.latencySamples = activity_sample->latencySamples;
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      CUpti_ActivityMemcpy *activity_memcpy = (CUpti_ActivityMemcpy *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_MEMCPY;
      entry->activity.data.memcpy.copyKind = activity_memcpy->copyKind;
      entry->activity.data.memcpy.bytes = activity_memcpy->bytes;
      entry->activity.data.memcpy.start = activity_memcpy->start;
      entry->activity.data.memcpy.end = activity_memcpy->end;
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      CUpti_ActivityKernel4 *activity_kernel = (CUpti_ActivityKernel4 *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_KERNEL;
      entry->activity.data.kernel.dynamicSharedMemory = activity_kernel->dynamicSharedMemory;
      entry->activity.data.kernel.staticSharedMemory = activity_kernel->staticSharedMemory;
      entry->activity.data.kernel.localMemoryTotal = activity_kernel->localMemoryTotal;
      entry->activity.data.kernel.start = activity_kernel->start;
      entry->activity.data.kernel.end = activity_kernel->end;
      break;
    }
    default:
      break;
  }
}


cupti_node_t *
cupti_activity_node_new
(
 CUpti_Activity *activity, 
 cct_node_t *cct_node,
 cupti_node_t *next
)
{
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc(sizeof(cupti_node_t));
  node->entry = (cupti_entry_activity_t *)hpcrun_malloc(sizeof(cupti_entry_activity_t));
  cupti_activity_node_set(node, activity, cct_node, next);
  return node;
}


void
cupti_notification_node_set
(
 cupti_node_t *cupti_node,
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
)
{
  cupti_entry_notification_t *entry = (cupti_entry_notification_t *)(cupti_node->entry);
  entry->host_op_id = host_op_id;
  entry->cct_node = cct_node;
  entry->record = record;
  cupti_node->next = next;
  cupti_node->type = CUPTI_ENTRY_TYPE_NOTIFICATION;
}


cupti_node_t *
cupti_notification_node_new
(
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
)
{
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc(sizeof(cupti_node_t));
  cupti_entry_notification_t *entry = (cupti_entry_notification_t *)hpcrun_malloc(sizeof(cupti_entry_notification_t));
  node->entry = entry;
  cupti_notification_node_set(node, host_op_id, cct_node, record, next);
  return node;
}
