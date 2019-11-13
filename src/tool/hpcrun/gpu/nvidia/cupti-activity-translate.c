

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
// nvidia includes
//******************************************************************************

#include <cupti_activity.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-activity.h>

#include "cupti-activity-translate.h"

// #include <cupti_version.h>
//#include "cupti-analysis.h"



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
 CUpti_ActivityPCSamplingVersion *sa 
)
{
#if CUPTI_API_VERSION >= 10
  CUpti_ActivityPCSampling3 *activity_sample = (CUpti_ActivityPCSampling3 *)sa;
#else
  CUpti_ActivityPCSampling2 *activity_sample = (CUpti_ActivityPCSampling2 *)sa;
#endif
PRINT("source %u, functionId %u, pc 0x%p, corr %u, "
   "samples %u, latencySamples %u, stallReason %u\n",
   sample->sourceLocatorId,
   sample->functionId,
   (void *)sample->pcOffset,
   sample->correlationId,
   sample->samples,
   sample->latencySamples,
   sample->stallReason);

  ga->kind = GPU_ACTIVITY_KIND_PC_SAMPLING;
  set_gpu_instruction_fields(&ga->details.instruction, activity_sample->pcOffset);  
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
  PRINT("corr %u, totalSamples %llu, droppedSamples %llu\n",
   activity_info->correlationId,
   (unsigned long long)activity_info->totalSamples,
   (unsigned long long)activity_info->droppedSamples);
  ga->kind = GPU_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO;

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
  PRINT("Memcpy copy CorrelationId %u\n", activity_memcpy->correlationId);
  PRINT("Memcpy copy kind %u\n", activity_memcpy->copyKind);
  PRINT("Memcpy copy bytes %lu\n", activity_memcpy->bytes);

  ga->kind = GPU_ACTIVITY_KIND_MEMCPY;
  ga->details.memcpy.copyKind = activity_memcpy->copyKind;
  ga->details.memcpy.bytes = activity_memcpy->bytes;
  set_gpu_activity_interval(&ga->details.interval, activity_memcpy->start, activity_memcpy->end);  
}


static void
convert_kernel
(
  gpu_activity_t *ga,
  CUpti_ActivityKernel4 *activity_kernel
)
{
  ga->kind = GPU_ACTIVITY_KIND_KERNEL;

  ga->details.kernel.dynamicSharedMemory = 
    activity_kernel->dynamicSharedMemory;

  ga->details.kernel.staticSharedMemory = activity_kernel->staticSharedMemory;
  ga->details.kernel.localMemoryTotal = activity_kernel->localMemoryTotal;
  set_gpu_activity_interval(&ga->details.interval, activity_kernel->start, activity_kernel->end);
  ga->details.kernel.contextId = activity_kernel->contextId;
  ga->details.kernel.streamId = activity_kernel->streamId;

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
  ga->kind = GPU_ACTIVITY_KIND_GLOBAL_ACCESS;
  set_gpu_instruction_fields(&ga->details.instruction, activity_global_access->pcOffset);  

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
  ga->kind = GPU_ACTIVITY_KIND_SHARED_ACCESS;
  set_gpu_instruction_fields(&ga->details.instruction, activity_shared_access->pcOffset); 

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
  ga->kind = GPU_ACTIVITY_KIND_BRANCH;
  set_gpu_instruction_fields(&ga->details.instruction, activity_branch->pcOffset); 
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
  ga->kind = GPU_ACTIVITY_KIND_SYNCHRONIZATION;
  ga->details.synchronization.syncKind = activity_sync->type;
  set_gpu_activity_interval(&ga->details.interval, activity_sync->start, activity_sync->end);
}


static void
convert_memory
(
  gpu_activity_t *ga,
  CUpti_ActivityMemory *activity_mem 
)
{
  ga->kind = GPU_ACTIVITY_KIND_MEMORY;
  ga->details.memory.memKind = activity_mem->memoryKind;
  ga->details.memory.bytes = activity_mem->bytes;
  set_gpu_activity_interval(&ga->details.interval, activity_mem->start, activity_mem->end);
}


static void
convert_memset
(
  gpu_activity_t *ga,
  CUpti_ActivityMemset *activity_memset
)
{
  ga->kind = GPU_ACTIVITY_KIND_MEMSET;
  ga->details.memset.memKind = activity_memset->memoryKind;
  ga->details.memset.bytes = activity_memset->bytes;
  set_gpu_activity_interval(&ga->details.interval, activity_memset->start, activity_memset->end);  
}

static void
convert_correlation
(
  gpu_activity_t *ga, 
  CUpti_ActivityExternalCorrelation* acitivity_cor
)
{
  PRINT("External CorrelationId %lu\n", acitivity_cor->externalId);
  PRINT("CorrelationId %u\n", acitivity_cor->correlationId);

  ga->kind = GPU_ACTIVITY_KIND_EXTERNAL_CORRELATION;
  ga->details.correlation.externalId = acitivity_cor->externalId;
}

static void
convert_cdpkernel
(
  gpu_activity_t *ga,
  CUpti_ActivityCdpKernel *activity_kernel
)
{
  ga->kind = GPU_ACTIVITY_KIND_CDP_KERNEL;
  
  set_gpu_activity_interval(&ga->details.interval, activity_kernel->start, activity_kernel->end);
  ga->details.cdpkernel.contextId = activity_kernel->contextId;
  ga->details.cdpkernel.streamId = activity_kernel->streamId;
}


//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_activity_translate
(
 gpu_activity_t *ga,
 CUpti_Activity *activity,
 cct_node_t *cct_node
)
{

  ga->cct_node = cct_node;
  ga->correlationId = activity->correlationId;
  
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    convert_pcsampling (ga, (CUpti_ActivityPCSamplingVersion *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    convert_pcsampling_record_info
      (ga, (CUpti_ActivityPCSamplingRecordInfo *)activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY:
  case CPPTI_ACTIVITY_KIND_MEMCPY2:
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
  case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION:
    convert_correlation(ga, (CUpti_ActivityExternalCorrelation*) activity);
    break;
  case CUPTI_ACTIVITY_KIND_CDP_KERNEL:
    convert_cdpkernel(ga, (CUpti_ActivityCdpKernel*) activity);
    break;
  default:
    break;
  }
}
