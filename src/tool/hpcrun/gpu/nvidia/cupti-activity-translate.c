

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

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct_addr.h>
#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>

#include "cupti-activity-translate.h"
#include "cupti-analysis.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0


#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif


#if CUPTI_API_VERSION >= 10
#define CUpti_ActivityPCSamplingVersion CUpti_ActivityPCSampling3 
#else
#define CUpti_ActivityPCSamplingVersion CUpti_ActivityPCSampling2 
#endif



//******************************************************************************
// private operations
//******************************************************************************

static void
set_gpu_instruction_fields
(
 gpu_instruction_t *instruction,
 uint32_t activity_correlation_id,
 uint32_t pc_offset
) 
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(activity_correlation_id);
  assert(cid_map_entry);

  uint64_t host_correlation_id =
    gpu_correlation_id_map_entry_external_id_get(cid_map_entry);

  gpu_host_correlation_map_entry_t *host_correlation_entry =
    gpu_host_correlation_map_lookup(host_correlation_id);
  assert(host_correlation_entry);

  // get a normalized IP for the function enclosing the instruction
  cct_node_t *func_node = 
    gpu_host_correlation_map_entry_op_function_get(host_correlation_entry); 
  ip_normalized_t pc = hpcrun_cct_addr(func_node)->ip_norm;

  // compute a normalized IP for the instruction by adding the
  // instruction's offset in the routine to the address of the routine
  pc.lm_ip += pc_offset;

  instruction->correlation_id = activity_correlation_id;
  instruction->pc = pc;
}

static void
convert_pcsampling
(
 gpu_activity_t *ga,
 CUpti_ActivityPCSamplingVersion *activity 
)
{
  PRINT("source %u, functionId %u, pc 0x%x, corr %u, "
	"samples %u, latencySamples %u, stallReason %u\n",
	activity->sourceLocatorId,
	activity->functionId,
	activity->pcOffset,
	activity->correlationId,
	activity->samples,
	activity->latencySamples,
	activity->stallReason);
  
  ga->kind = GPU_ACTIVITY_KIND_PC_SAMPLING;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId, 
			     activity->pcOffset); 

  ga->details.pc_sampling.stallReason = activity->stallReason;
  ga->details.pc_sampling.samples = activity->samples;
  ga->details.pc_sampling.latencySamples = activity->latencySamples;
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

  ga->details.pc_sampling_record_info.correlation_id = activity_info->correlationId;
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
  CUpti_ActivityMemcpy *activity
)
{
  PRINT("Memcpy copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy copy kind %u\n", activity->copyKind);
  PRINT("Memcpy copy bytes %lu\n", activity->bytes);

  ga->kind = GPU_ACTIVITY_KIND_MEMCPY;
  ga->details.memcpy.correlation_id = activity->correlationId;
  ga->details.memcpy.context_id = activity->contextId;
  ga->details.memcpy.stream_id = activity->streamId;
  ga->details.memcpy.copyKind = activity->copyKind;
  ga->details.memcpy.bytes = activity->bytes;

  set_gpu_activity_interval(&ga->details.interval, activity->start, activity->end);  
}


static void
convert_kernel
(
  gpu_activity_t *ga,
  CUpti_ActivityKernel4 *activity
)
{
  ga->kind = GPU_ACTIVITY_KIND_KERNEL;

  ga->details.kernel.correlation_id = activity->correlationId;

  ga->details.kernel.dynamicSharedMemory = activity->dynamicSharedMemory;

  ga->details.kernel.staticSharedMemory = activity->staticSharedMemory;
  ga->details.kernel.localMemoryTotal = activity->localMemoryTotal;
  ga->details.kernel.context_id = activity->contextId;
  ga->details.kernel.stream_id = activity->streamId;

  set_gpu_activity_interval(&ga->details.interval, activity->start, activity->end);

  uint32_t activeWarpsPerSM = 0;
  uint32_t maxActiveWarpsPerSM = 0;
  uint32_t threadRegisters = 0;
  uint32_t blockThreads = 0;
  uint32_t blockSharedMemory = 0;
  cupti_occupancy_analyze(activity, &activeWarpsPerSM, &maxActiveWarpsPerSM, 
			  &threadRegisters, &blockThreads, &blockSharedMemory);

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
  CUpti_ActivityGlobalAccess3 *activity
)
{
  ga->kind = GPU_ACTIVITY_KIND_GLOBAL_ACCESS;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId, 
			     activity->pcOffset); 

  ga->details.global_access.l2_transactions = 
    activity->l2_transactions;

  ga->details.global_access.theoreticalL2Transactions =
    activity->theoreticalL2Transactions;

  gpu_global_access_type_t type;
  if (activity->flags & (1<<8)) {
    if (activity->flags & (1<<9)) {
      type = GPU_GLOBAL_ACCESS_LOAD_CACHED;
    } else {
      type = GPU_GLOBAL_ACCESS_LOAD_UNCACHED;
    }
  } else {
    type = GPU_GLOBAL_ACCESS_STORE;
  }
  ga->details.global_access.type = type;

  uint64_t bytes = (activity->flags & 0xFF) * activity->threadsExecuted;

  ga->details.global_access.bytes = bytes;
}


static void
convert_shared_access
(
  gpu_activity_t *ga,
  CUpti_ActivitySharedAccess *activity
)
{ 
  ga->kind = GPU_ACTIVITY_KIND_SHARED_ACCESS;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId, 
			     activity->pcOffset); 

  ga->details.shared_access.sharedTransactions = 
    activity->sharedTransactions;

  ga->details.shared_access.theoreticalSharedTransactions =
    activity->theoreticalSharedTransactions;

  ga->details.shared_access.type = 
    ((activity->flags & (1<<8)) ? 
     GPU_SHARED_ACCESS_LOAD : 
     GPU_SHARED_ACCESS_STORE);

  ga->details.shared_access.bytes = (activity->flags & 0xFF) * 
    activity->threadsExecuted;
}

static void
convert_branch
(
 gpu_activity_t *ga,
 CUpti_ActivityBranch2 *activity
)
{
  ga->kind = GPU_ACTIVITY_KIND_BRANCH;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId, 
			     activity->pcOffset); 

  ga->details.branch.diverged = activity->diverged;
  ga->details.branch.executed = activity->executed;
}


static void
convert_synchronization
(
 gpu_activity_t *ga,
 CUpti_ActivitySynchronization *activity_sync
)
{
  ga->kind = GPU_ACTIVITY_KIND_SYNCHRONIZATION;
  ga->details.synchronization.correlation_id = activity_sync->correlationId;
  ga->details.synchronization.context_id = activity_sync->contextId;
  ga->details.synchronization.stream_id = activity_sync->streamId;
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
  CUpti_ActivityMemset *activity
)
{
  ga->kind = GPU_ACTIVITY_KIND_MEMSET;
  ga->details.memset.correlation_id = activity->correlationId;
  ga->details.memset.context_id = activity->contextId;
  ga->details.memset.stream_id = activity->streamId;
  ga->details.memset.memKind = activity->memoryKind;
  ga->details.memset.bytes = activity->bytes;

  set_gpu_activity_interval(&ga->details.interval, activity->start, activity->end);  
}

static void
convert_correlation
(
  gpu_activity_t *ga, 
  CUpti_ActivityExternalCorrelation* activity
)
{
  PRINT("External CorrelationId %lu\n", activity->externalId);
  PRINT("CorrelationId %u\n", activity->correlationId);

  ga->kind = GPU_ACTIVITY_KIND_EXTERNAL_CORRELATION;
  ga->details.correlation.correlation_id = activity->correlationId;
  ga->details.correlation.host_correlation_id = activity->externalId;
}

static void
convert_cdpkernel
(
  gpu_activity_t *ga,
  CUpti_ActivityCdpKernel *activity
)
{
  ga->kind = GPU_ACTIVITY_KIND_CDP_KERNEL;
  
  ga->details.cdpkernel.correlation_id = activity->correlationId;
  ga->details.cdpkernel.context_id = activity->contextId;
  ga->details.cdpkernel.stream_id = activity->streamId;

  set_gpu_activity_interval(&ga->details.interval, activity->start, activity->end);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_activity_translate
(
 gpu_activity_t *ga,
 CUpti_Activity *activity
)
{
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    convert_pcsampling (ga, (CUpti_ActivityPCSamplingVersion *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    convert_pcsampling_record_info
      (ga, (CUpti_ActivityPCSamplingRecordInfo *)activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY:
  case CUPTI_ACTIVITY_KIND_MEMCPY2:
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
