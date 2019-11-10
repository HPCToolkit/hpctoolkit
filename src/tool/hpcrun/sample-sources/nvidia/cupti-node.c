

//******************************************************************************
// Description:
//   Read fields from a CUpti_Activity and assign to a 
//   GPU-independent gpu_activity_t.
//
//   This interface is only used by the CUPTI GPU monitoring thread.
//   It is thread-safe as long as it does not access details structures
//   shared by worker threads.
//******************************************************************************


//******************************************************************************
// local includes
//******************************************************************************

// #include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <cupti_version.h>
#include <cupti_activity.h>
#include "cupti-node.h"
#include "cupti-analysis.h"



//******************************************************************************
// macros
//******************************************************************************

#if CUPTI_API_VERSION >= 10
#define CUpti_ActivityPCSamplingVersion CUpti_ActivityPCSampling3 
#else
#define CUpti_ActivityPCSamplingVersion CUpti_ActivityPCSampling2 
#endif



//******************************************************************************
// private operations
//******************************************************************************

static void
convert_pcsampling
(
 gpu_activity_t *ga,
 CUpti_ActivityPCSamplingVersion *activity_sample 
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING;

  ga->details.pc_sampling.stallReason = activity_sample->stallReason;
  ga->details.pc_sampling.samples = activity_sample->samples;
  ga->details.pc_sampling.latencySamples = activity_sample->latencySamples;
}


static void
convert_pcsampling_record_info
(
 gpu_activity_t *ga,
 CUpti_ActivityPCSamplingRecordInfo *activity_info 
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO;

  ga->details.pc_sampling_record_info.totalSamples = 
    activity_info->totalSamples;
  ga->details.pc_sampling_record_info.droppedSamples = 
    activity_info->droppedSamples;
  ga->details.pc_sampling_record_info.samplingPeriodInCycles = 
    activity_info->samplingPeriodInCycles;
  
  uint64_t totalSamples = 0;
  uint64_t fullSMSamples = 0;
  cupti_sm_efficiency_analyze(activity_info, &totalSamples, &fullSMSamples);
  
  ga->details.pc_sampling_record_info.fullSMSamples = fullSMSamples;
}


static void
convert_memcpy
(
  gpu_activity_t *ga,
  CUpti_ActivityMemcpy *activity_memcpy
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMCPY;
  ga->details.memcpy.copyKind = activity_memcpy->copyKind;
  ga->details.memcpy.bytes = activity_memcpy->bytes;
  ga->details.memcpy.start = activity_memcpy->start;
  ga->details.memcpy.end = activity_memcpy->end;
}


static void
convert_kernel
(
  gpu_activity_t *ga,
  CUpti_ActivityKernel4 *activity_kernel
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_KERNEL;

  ga->details.kernel.dynamicSharedMemory = 
    activity_kernel->dynamicSharedMemory;

  ga->details.kernel.staticSharedMemory = activity_kernel->staticSharedMemory;
  ga->details.kernel.localMemoryTotal = activity_kernel->localMemoryTotal;
  ga->details.kernel.start = activity_kernel->start;
  ga->details.kernel.end = activity_kernel->end;

  uint32_t activeWarpsPerSM = 0;
  uint32_t maxActiveWarpsPerSM = 0;
  uint32_t threadRegisters = 0;
  uint32_t blockThreads = 0;
  uint32_t blockSharedMemory = 0;
  cupti_occupancy_analyze(activity_kernel, &activeWarpsPerSM, 
			  &maxActiveWarpsPerSM, &threadRegisters, 
			  &blockThreads, &blockSharedMemory);

  ga->details.kernel.activeWarpsPerSM = activeWarpsPerSM;
  ga->details.kernel.maxActiveWarpsPerSM = maxActiveWarpsPerSM;
  ga->details.kernel.threadRegisters = threadRegisters;
  ga->details.kernel.blockThreads = blockThreads;
  ga->details.kernel.blockSharedMemory = blockSharedMemory;
}


static void
convert_global_access
(
  gpu_activity_t *ga,
  CUpti_ActivityGlobalAccess3 *activity_global_access
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS;

  ga->details.global_access.l2_transactions = 
    activity_global_access->l2_transactions;

  ga->details.global_access.theoreticalL2Transactions =
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
  ga->details.global_access.type = type;

  uint64_t bytes = 
    (activity_global_access->flags & 0xFF) * 
    activity_global_access->threadsExecuted;

  ga->details.global_access.bytes = bytes;
}


static void
convert_shared_access
(
  gpu_activity_t *ga,
  CUpti_ActivitySharedAccess *activity_shared_access
)
{ 
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_SHARED_ACCESS;

  ga->details.shared_access.sharedTransactions = 
    activity_shared_access->sharedTransactions;

  ga->details.shared_access.theoreticalSharedTransactions =
    activity_shared_access->theoreticalSharedTransactions;

  ga->details.shared_access.type = 
    ((activity_shared_access->flags & (1<<8)) ?
     GPU_SHARED_ACCESS_LOAD :
     GPU_SHARED_ACCESS_STORE);

  ga->details.shared_access.bytes = 
    (activity_shared_access->flags & 0xFF) * 
    activity_shared_access->threadsExecuted;
}

static void
convert_branch
(
 gpu_activity_t *ga,
 CUpti_ActivityBranch2 *activity_branch
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_BRANCH;
  ga->details.branch.diverged = activity_branch->diverged;
  ga->details.branch.executed = activity_branch->executed;
}


static void
convert_synchronization
(
 gpu_activity_t *ga,
 CUpti_ActivitySynchronization *activity_sync
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_SYNCHRONIZATION;
  ga->details.synchronization.syncKind = activity_sync->type;
  ga->details.synchronization.start = activity_sync->start;
  ga->details.synchronization.end = activity_sync->end;
}



static void
convert_memory
(
<<<<<<< HEAD
  gpu_activity_t *ga,
  CUpti_ActivityMemory *activity_mem 
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMORY;
  ga->details.memory.memKind = activity_mem->memoryKind;
  ga->details.memory.bytes = activity_mem->bytes;
  ga->details.memory.start = activity_mem->start;
  ga->details.memory.end = activity_mem->end;
=======
 cupti_entry_correlation_t *entry,
 uint64_t host_op_id,
 void *activity_channel,
 cct_node_t *copy_node,
 cct_node_t *copyin_node,
 cct_node_t *copyout_node,
 cct_node_t *alloc_node,
 cct_node_t *delete_node,
 cct_node_t *sync_node,
 cct_node_t *kernel_node,
 cct_node_t *trace_node
)
{
  entry->activity_channel = activity_channel;
  entry->host_op_id = host_op_id;
  entry->copy_node = copy_node;
  entry->copyin_node = copyin_node;
  entry->copyout_node = copyout_node;
  entry->alloc_node = alloc_node;
  entry->delete_node = delete_node;
  entry->sync_node = sync_node;
  entry->kernel_node = kernel_node;
  entry->trace_node = trace_node;
>>>>>>> master-gpu-trace
}


static void
convert_memset
(
  gpu_activity_t *ga,
  CUpti_ActivityMemset *activity_memset
)
{
  ga->cupti_kind.kind = CUPTI_ACTIVITY_KIND_MEMORY;
  ga->details.memset.memKind = activity_memset->memoryKind;
  ga->details.memset.bytes = activity_memset->bytes;
  ga->details.memset.start = activity_memset->start;
  ga->details.memset.end = activity_memset->end;
}



//******************************************************************************
// private operations
//******************************************************************************

void
cupti_ga_activity_set
(
 gpu_activity_t *ga,
 CUpti_Activity *activity,
 cct_node_t *cct_node
)
{
  ga->cct_node = cct_node;
  
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    convert_pcsampling (ga, (CUpti_ActivityPCSamplingVersion *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    convert_pcsampling_record_info
      (ga, (CUpti_ActivityPCSamplingRecordInfo *)activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY:
    convert_memcpy(ga, (CUpti_ActivityMemcpy *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_KERNEL:
    convert_kernel(ga, (CUpti_ActivityKernel4 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
    convert_global_access(ga, (CUpti_ActivityGlobalAccess3 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
    convert_shared_access(ga, (CUpti_ActivitySharedAccess *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_BRANCH:
    convert_branch(ga, (CUpti_ActivityBranch2 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    convert_synchronization(ga, (CUpti_ActivitySynchronization *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMORY:
    convert_memory(ga, (CUpti_ActivityMemory *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMSET:
    convert_memset(ga, (CUpti_ActivityMemset *) activity);
    break;

  default:
    break;
  }
}
