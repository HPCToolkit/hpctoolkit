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
cupti_activity_entry_set
(
        entry_activity_t *entry,
        CUpti_Activity *activity,
        cct_node_t *cct_node
)
{
    entry->cct_node = cct_node;
    if (entry->data == NULL)
    {
        entry->data = (entry_data_t *)hpcrun_malloc_safe(sizeof(entry_data_t));
    }
    switch (activity->kind) {
        case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
        {
#if CUPTI_API_VERSION >= 10
            CUpti_ActivityPCSampling3 *activity_sample = (CUpti_ActivityPCSampling3 *)activity;
#else
            CUpti_ActivityPCSampling2 *activity_sample = (CUpti_ActivityPCSampling2 *)activity;
#endif
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING;
            entry->data->pc_sampling.stallReason = activity_sample->stallReason;
            entry->data->pc_sampling.samples = activity_sample->samples;
            entry->data->pc_sampling.latencySamples = activity_sample->latencySamples;
            break;
        }
        case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
        {
            CUpti_ActivityPCSamplingRecordInfo *activity_info =
                    (CUpti_ActivityPCSamplingRecordInfo *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO;
            entry->data->pc_sampling_record_info.totalSamples = activity_info->totalSamples;
            entry->data->pc_sampling_record_info.droppedSamples = activity_info->droppedSamples;
            entry->data->pc_sampling_record_info.samplingPeriodInCycles = activity_info->samplingPeriodInCycles;
            uint64_t totalSamples = 0;
            uint64_t fullSMSamples = 0;
            cupti_sm_efficiency_analyze(activity_info, &totalSamples, &fullSMSamples);
            entry->data->pc_sampling_record_info.fullSMSamples = fullSMSamples;
            break;
        }
        case CUPTI_ACTIVITY_KIND_MEMCPY:
        {
            CUpti_ActivityMemcpy *activity_memcpy = (CUpti_ActivityMemcpy *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMCPY;
            entry->data->memcpy.copyKind = activity_memcpy->copyKind;
            entry->data->memcpy.bytes = activity_memcpy->bytes;
            entry->data->memcpy.start = activity_memcpy->start;
            entry->data->memcpy.end = activity_memcpy->end;
            break;
        }
        case CUPTI_ACTIVITY_KIND_KERNEL:
        {
            CUpti_ActivityKernel4 *activity_kernel = (CUpti_ActivityKernel4 *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_KERNEL;
            entry->data->kernel.dynamicSharedMemory = activity_kernel->dynamicSharedMemory;
            entry->data->kernel.staticSharedMemory = activity_kernel->staticSharedMemory;
            entry->data->kernel.localMemoryTotal = activity_kernel->localMemoryTotal;
            entry->data->kernel.start = activity_kernel->start;
            entry->data->kernel.end = activity_kernel->end;
            uint32_t activeWarpsPerSM = 0;
            uint32_t maxActiveWarpsPerSM = 0;
            uint32_t threadRegisters = 0;
            uint32_t blockThreads = 0;
            uint32_t blockSharedMemory = 0;
            cupti_occupancy_analyze(activity_kernel, &activeWarpsPerSM, &maxActiveWarpsPerSM,
                                    &threadRegisters, &blockThreads, &blockSharedMemory);
            entry->data->kernel.activeWarpsPerSM = activeWarpsPerSM;
            entry->data->kernel.maxActiveWarpsPerSM = maxActiveWarpsPerSM;
            entry->data->kernel.threadRegisters = threadRegisters;
            entry->data->kernel.blockThreads = blockThreads;
            entry->data->kernel.blockSharedMemory = blockSharedMemory;
            break;
        }
        case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
        {
            CUpti_ActivityGlobalAccess3 *activity_global_access = (CUpti_ActivityGlobalAccess3 *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS;
            entry->data->global_access.l2_transactions = activity_global_access->l2_transactions;
            entry->data->global_access.theoreticalL2Transactions =
                    activity_global_access->theoreticalL2Transactions;
            gpu_global_access_type_t type;
            if (activity_global_access->flags & (1<<8)) {
                if (activity_global_access->flags & (1<<9)) {
                    type = GPU_GLOBAL_ACCESS_LOAD_CACHED;
                } else {
                    type = GPU_GLOBAL_ACCESS_LOAD_UNCACHED;
                }
            } else {
                type = GPU_GLOBAL_ACCESS_STORE;
            }
            entry->data->global_access.type = type;
            uint64_t bytes = (activity_global_access->flags & 0xFF) * activity_global_access->threadsExecuted;
            entry->data->global_access.bytes = bytes;
            break;
        }
        case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
        {
            CUpti_ActivitySharedAccess *activity_shared_access = (CUpti_ActivitySharedAccess *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_SHARED_ACCESS;
            entry->data->shared_access.sharedTransactions = activity_shared_access->sharedTransactions;
            entry->data->shared_access.theoreticalSharedTransactions =
                    activity_shared_access->theoreticalSharedTransactions;
            gpu_shared_access_type_t type;
            if (activity_shared_access->flags & (1<<8)) {
                type = GPU_SHARED_ACCESS_LOAD;
            } else {
                type = GPU_SHARED_ACCESS_STORE;
            }
            entry->data->shared_access.type = type;
            uint64_t bytes = (activity_shared_access->flags & 0xFF) * activity_shared_access->threadsExecuted;
            entry->data->shared_access.bytes = bytes;
            break;
        }
        case CUPTI_ACTIVITY_KIND_BRANCH:
        {
            CUpti_ActivityBranch2 *activity_branch = (CUpti_ActivityBranch2 *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_BRANCH;
            entry->data->branch.diverged = activity_branch->diverged;
            entry->data->branch.executed = activity_branch->executed;
            break;
        }
        case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
        {
            CUpti_ActivitySynchronization *activity_sync = (CUpti_ActivitySynchronization *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_SYNCHRONIZATION;
            entry->data->synchronization.syncKind = activity_sync->type;
            entry->data->synchronization.start = activity_sync->start;
            entry->data->synchronization.end = activity_sync->end;
            break;
        }
        case CUPTI_ACTIVITY_KIND_MEMORY:
        {
            CUpti_ActivityMemory *activity_mem = (CUpti_ActivityMemory *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMORY;
            entry->data->memory.memKind = activity_mem->memoryKind;
            entry->data->memory.bytes = activity_mem->bytes;
            entry->data->memory.start = activity_mem->start;
            entry->data->memory.end = activity_mem->end;
            break;
        }
        case CUPTI_ACTIVITY_KIND_MEMSET:
        {
            CUpti_ActivityMemset *activity_memset = (CUpti_ActivityMemset *)activity;
            entry->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMORY;
            entry->data->memset.memKind = activity_memset->memoryKind;
            entry->data->memset.bytes = activity_memset->bytes;
            entry->data->memset.start = activity_memset->start;
            entry->data->memset.end = activity_memset->end;
            break;
        }
        default:
            break;
    }
}





/*
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
*/

/*void
cupti_correlation_entry_set
(
        cupti_entry_correlation_t *entry,
        uint64_t host_op_id,
        cct_node_t *cct_node,
        void *record
)
{
    entry->host_op_id = host_op_id;
    entry->cct_node = cct_node;
    entry->record = record;
}*/

/*
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
  cupti_entry_correlation_t *entry = (cupti_entry_correlation_t *)hpcrun_malloc_safe(sizeof(cupti_entry_correlation_t));
  node->entry = entry;
  cupti_notification_node_set(node, host_op_id, cct_node, record, next);
  return node;
}
*/