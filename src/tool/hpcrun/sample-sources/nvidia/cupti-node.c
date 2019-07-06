#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>
#include <cupti_activity.h>
#include "cupti-node.h"
#include "cupti-analysis.h"

//-------------------------------------------------------------
// Read fields from a CUpti_Activity and assign to a cupti_node
// This function is only used by CUPTI thread.
// It is thread-safe as long as it does not access data structures
// shared by worker threads.
// ------------------------------------------------------------
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
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    {
      CUpti_ActivityPCSamplingRecordInfo *activity_info =
        (CUpti_ActivityPCSamplingRecordInfo *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO;
      entry->activity.data.pc_sampling_record_info.totalSamples = activity_info->totalSamples;
      entry->activity.data.pc_sampling_record_info.droppedSamples = activity_info->droppedSamples;
      entry->activity.data.pc_sampling_record_info.samplingPeriodInCycles = activity_info->samplingPeriodInCycles;
      uint64_t totalSamples = 0;
      uint64_t fullSMSamples = 0;
      cupti_sm_efficiency_analyze(activity_info, &totalSamples, &fullSMSamples);
      entry->activity.data.pc_sampling_record_info.fullSMSamples = fullSMSamples;
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
      uint32_t activeWarpsPerSM = 0;
      uint32_t maxActiveWarpsPerSM = 0;
      uint32_t threadRegisters = 0;
      uint32_t blockThreads = 0;
      uint32_t blockSharedMemory = 0;
      cupti_occupancy_analyze(activity_kernel, &activeWarpsPerSM, &maxActiveWarpsPerSM,
        &threadRegisters, &blockThreads, &blockSharedMemory);
      entry->activity.data.kernel.activeWarpsPerSM = activeWarpsPerSM;
      entry->activity.data.kernel.maxActiveWarpsPerSM = maxActiveWarpsPerSM;
      entry->activity.data.kernel.threadRegisters = threadRegisters;
      entry->activity.data.kernel.blockThreads = blockThreads;
      entry->activity.data.kernel.blockSharedMemory = blockSharedMemory;
      break;
    }
    case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
    {
      CUpti_ActivityGlobalAccess3 *activity_global_access = (CUpti_ActivityGlobalAccess3 *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS;
      entry->activity.data.global_access.l2_transactions = activity_global_access->l2_transactions;
      entry->activity.data.global_access.theoreticalL2Transactions =
        activity_global_access->theoreticalL2Transactions;
      cupti_global_access_type_t type;
      if (activity_global_access->flags & (1<<8)) {
        if (activity_global_access->flags & (1<<9)) {
          type = CUPTI_GLOBAL_ACCESS_LOAD_CACHED;
        } else {
          type = CUPTI_GLOBAL_ACCESS_LOAD_UNCACHED;
        }
      } else {
        type = CUPTI_GLOBAL_ACCESS_STORE;
      }
      entry->activity.data.global_access.type = type;
      uint64_t bytes = (activity_global_access->flags & 0xFF) * activity_global_access->threadsExecuted;
      entry->activity.data.global_access.bytes = bytes;
      break;
    }
    case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
    {
      CUpti_ActivitySharedAccess *activity_shared_access = (CUpti_ActivitySharedAccess *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_SHARED_ACCESS;
      entry->activity.data.shared_access.sharedTransactions = activity_shared_access->sharedTransactions;
      entry->activity.data.shared_access.theoreticalSharedTransactions =
        activity_shared_access->theoreticalSharedTransactions;
      cupti_shared_access_type_t type;
      if (activity_shared_access->flags & (1<<8)) {
        type = CUPTI_SHARED_ACCESS_LOAD;
      } else {
        type = CUPTI_SHARED_ACCESS_STORE;
      }
      entry->activity.data.shared_access.type = type;
      uint64_t bytes = (activity_shared_access->flags & 0xFF) * activity_shared_access->threadsExecuted;
      entry->activity.data.shared_access.bytes = bytes;
      break;
    }
    case CUPTI_ACTIVITY_KIND_BRANCH:
    {
      CUpti_ActivityBranch2 *activity_branch = (CUpti_ActivityBranch2 *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_BRANCH;
      entry->activity.data.branch.diverged = activity_branch->diverged;
      entry->activity.data.branch.executed = activity_branch->executed;
      break;
    }
    case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    {
      CUpti_ActivitySynchronization *activity_sync = (CUpti_ActivitySynchronization *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_SYNCHRONIZATION;
      entry->activity.data.synchronization.syncKind = activity_sync->type;
      entry->activity.data.synchronization.start = activity_sync->start;
      entry->activity.data.synchronization.end = activity_sync->end;
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMORY:
    {
      CUpti_ActivityMemory *activity_mem = (CUpti_ActivityMemory *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_MEMORY;
      entry->activity.data.memory.memKind = activity_mem->memoryKind;
      entry->activity.data.memory.bytes = activity_mem->bytes;
      entry->activity.data.memory.start = activity_mem->start;
      entry->activity.data.memory.end = activity_mem->end;
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMSET:
    {
      CUpti_ActivityMemset *activity_memset = (CUpti_ActivityMemset *)activity;
      entry->activity.kind = CUPTI_ACTIVITY_KIND_MEMORY;
      entry->activity.data.memset.memKind = activity_memset->memoryKind;
      entry->activity.data.memset.bytes = activity_memset->bytes;
      entry->activity.data.memset.start = activity_memset->start;
      entry->activity.data.memset.end = activity_memset->end;
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
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc_safe(sizeof(cupti_node_t));
  node->entry = (cupti_entry_activity_t *)hpcrun_malloc_safe(sizeof(cupti_entry_activity_t));
  cupti_activity_node_set(node, activity, cct_node, next);
  return node;
}


void
cupti_notification_node_set
(
 cupti_node_t *cupti_node,
 uint64_t host_op_id,
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
 uint64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
)
{
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc_safe(sizeof(cupti_node_t));
  cupti_entry_notification_t *entry = (cupti_entry_notification_t *)hpcrun_malloc_safe(sizeof(cupti_entry_notification_t));
  node->entry = entry;
  cupti_notification_node_set(node, host_op_id, cct_node, record, next);
  return node;
}
