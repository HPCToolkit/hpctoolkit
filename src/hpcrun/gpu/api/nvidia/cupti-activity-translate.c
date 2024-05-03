// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include <cupti_version.h>
#include <cupti_activity.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../cct/cct.h"
#include "../../../cct/cct_addr.h"
#include "../../../utilities/ip-normalized.h"
#include "../../activity/gpu-activity.h"

#include "cuda-device-map.h"
#include "cupti-activity-translate.h"
#include "cupti-analysis.h"
#include "cubin-id-map.h"
#include "gpu-function-id-map.h"
#include "cuda-correlation-id-map.h"
#include "cuda-kernel-data-map.h"



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

static gpu_inst_stall_t
convert_stall_type
(
 CUpti_ActivityPCSamplingStallReason reason
)
{
  switch(reason) {
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_NONE:
    return GPU_INST_STALL_NONE;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_INST_FETCH:
    return GPU_INST_STALL_IFETCH;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_EXEC_DEPENDENCY:
    return GPU_INST_STALL_IDEPEND;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_MEMORY_DEPENDENCY:
    return GPU_INST_STALL_GMEM;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_TEXTURE:
    return GPU_INST_STALL_TMEM;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_SYNC:
    return GPU_INST_STALL_SYNC;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_CONSTANT_MEMORY_DEPENDENCY:
    return GPU_INST_STALL_CMEM;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_PIPE_BUSY:
    return GPU_INST_STALL_PIPE_BUSY;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_MEMORY_THROTTLE:
    return GPU_INST_STALL_MEM_THROTTLE;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_NOT_SELECTED:
    return GPU_INST_STALL_NOT_SELECTED;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_OTHER:
    return GPU_INST_STALL_OTHER;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_SLEEPING:
    return GPU_INST_STALL_SLEEP;
  case CUPTI_ACTIVITY_PC_SAMPLING_STALL_INVALID:
  default:
    return GPU_INST_STALL_INVALID;
  }
}


static gpu_memcpy_type_t
convert_memcpy_type
(
 CUpti_ActivityMemcpyKind kind
)
{
  switch (kind) {
  case CUPTI_ACTIVITY_MEMCPY_KIND_HTOD:      return GPU_MEMCPY_H2D;
  case CUPTI_ACTIVITY_MEMCPY_KIND_DTOH:      return GPU_MEMCPY_D2H;
  case CUPTI_ACTIVITY_MEMCPY_KIND_HTOA:      return GPU_MEMCPY_H2A;
  case CUPTI_ACTIVITY_MEMCPY_KIND_ATOH:      return GPU_MEMCPY_A2H;
  case CUPTI_ACTIVITY_MEMCPY_KIND_ATOA:      return GPU_MEMCPY_A2A;
  case CUPTI_ACTIVITY_MEMCPY_KIND_ATOD:      return GPU_MEMCPY_A2D;
  case CUPTI_ACTIVITY_MEMCPY_KIND_DTOA:      return GPU_MEMCPY_D2A;
  case CUPTI_ACTIVITY_MEMCPY_KIND_DTOD:      return GPU_MEMCPY_D2D;
  case CUPTI_ACTIVITY_MEMCPY_KIND_HTOH:      return GPU_MEMCPY_H2H;
  case CUPTI_ACTIVITY_MEMCPY_KIND_PTOP:      return GPU_MEMCPY_P2P;
  case CUPTI_ACTIVITY_MEMCPY_KIND_UNKNOWN:
  default:                                   return GPU_MEMCPY_UNK;
  }
}


#if TRACK_SYNCHRONIZATION
static gpu_sync_type_t
convert_sync_type
(
 uint32_t cupti_sync_kind
)
{
  switch(cupti_sync_kind) {
  case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_EVENT_SYNCHRONIZE:
    return GPU_SYNC_EVENT;
  case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_STREAM_WAIT_EVENT:
    return GPU_SYNC_STREAM_EVENT_WAIT;
  case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_STREAM_SYNCHRONIZE:
    return GPU_SYNC_STREAM;
  case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_CONTEXT_SYNCHRONIZE:
    return GPU_SYNC_CONTEXT;
  case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_UNKNOWN:
  default:
    return GPU_SYNC_UNKNOWN;
  }
}
#endif


static uint64_t
get_host_correlation_id
(
 uint64_t gpu_correlation_id,
 uint64_t *host_correlation_id
)
{
  cuda_correlation_id_map_entry_t *correlation_entry =
    cuda_correlation_id_map_lookup(gpu_correlation_id);

  if (correlation_entry == NULL) {
    TMSG(CUPTI_ACTIVITY, "Cannot find mapping for CorrelationId %u",
      gpu_correlation_id);
    return false;
  }

  *host_correlation_id =
    cuda_correlation_id_map_entry_get_host_id(correlation_entry);

  return true;
}


static void
set_gpu_instruction_fields
(
 gpu_instruction_t *instruction,
 uint32_t activity_correlation_id,
 uint32_t function_id,
 uint32_t pc_offset,
 uint64_t *correlation_id
)
{
  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity_correlation_id, &host_correlation_id)) {
    hpcrun_terminate();
  }
  *correlation_id = host_correlation_id;

  gpu_function_id_map_entry_t *fid_map_entry =
    gpu_function_id_map_lookup(function_id);
  if (!fid_map_entry)
    hpcrun_terminate();

  ip_normalized_t pc = gpu_function_id_map_entry_pc_get(fid_map_entry);

  // compute a normalized IP for the instruction by adding the
  // instruction's offset in the routine to the address of the routine
  pc.lm_ip += pc_offset;

  instruction->correlation_id = host_correlation_id;
  instruction->pc = pc;
}


static bool
convert_pcsampling
(
 gpu_activity_t *ga,
 CUpti_ActivityPCSamplingVersion *activity,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "source %u, functionId %u, pc 0x%x, corr %u, "
        "samples %u, latencySamples %u, stallReason %u\n",
        activity->sourceLocatorId,
        activity->functionId,
        activity->pcOffset,
        activity->correlationId,
        activity->samples,
        activity->latencySamples,
        activity->stallReason);

  ga->kind = GPU_ACTIVITY_PC_SAMPLING;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId,
           activity->functionId, activity->pcOffset, correlation_id);

  ga->details.pc_sampling.stallReason = convert_stall_type(activity->stallReason);
  ga->details.pc_sampling.samples = activity->samples;
  ga->details.pc_sampling.latencySamples = activity->latencySamples;

  cuda_correlation_id_map_entry_t *correlation_entry =
    cuda_correlation_id_map_lookup(activity->correlationId);
  bool remove = cuda_correlation_id_map_entry_increment_samples(
    correlation_entry, activity->samples);

  if (remove) {
    cuda_correlation_id_map_delete(activity->correlationId);
  }

  return true;
}


static bool
convert_pcsampling_record_info
(
 gpu_activity_t *ga,
 CUpti_ActivityPCSamplingRecordInfo *activity_info,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "corr %u, totalSamples %llu, droppedSamples %llu",
   activity_info->correlationId,
   (unsigned long long)activity_info->totalSamples,
   (unsigned long long)activity_info->droppedSamples);

  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity_info->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;

  ga->kind = GPU_ACTIVITY_PC_SAMPLING_INFO;

  ga->details.pc_sampling_info.correlation_id = host_correlation_id;
  ga->details.pc_sampling_info.totalSamples = activity_info->totalSamples;
  ga->details.pc_sampling_info.droppedSamples = activity_info->droppedSamples;
  ga->details.pc_sampling_info.samplingPeriodInCycles =
    activity_info->samplingPeriodInCycles;

  uint64_t totalSamples = 0;
  uint64_t fullSMSamples = 0;
  cupti_sm_efficiency_analyze(activity_info, &totalSamples, &fullSMSamples);

  ga->details.pc_sampling_info.fullSMSamples = fullSMSamples;


  cuda_correlation_id_map_entry_t *correlation_entry =
    cuda_correlation_id_map_lookup(activity_info->correlationId);

  bool remove = cuda_correlation_id_map_entry_set_total_samples(
    correlation_entry,
    activity_info->totalSamples - activity_info->droppedSamples);

  if (remove) {
    cuda_correlation_id_map_delete(activity_info->correlationId);
    cuda_kernel_data_map_delete(activity_info->correlationId);
  }

  return true;
}


static bool
convert_memcpy
(
 gpu_activity_t *ga,
 CUpti_ActivityMemcpy *activity,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "Memcpy copy CorrelationId %u", activity->correlationId);
  TMSG(CUPTI_ACTIVITY, "Memcpy copy kind %u", activity->copyKind);
  TMSG(CUPTI_ACTIVITY, "Memcpy copy bytes %lu", activity->bytes);

  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_id_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_MEMCPY;
  ga->details.memcpy.correlation_id = host_correlation_id;
  ga->details.memcpy.device_id = activity->deviceId;
  ga->details.memcpy.context_id = activity->contextId;
  ga->details.memcpy.stream_id = activity->streamId;
  ga->details.memcpy.copyKind = convert_memcpy_type(activity->copyKind);
  ga->details.memcpy.bytes = activity->bytes;

  gpu_interval_set(&ga->details.interval, activity->start, activity->end);

  return true;
}


static bool
convert_memcpy2
(
 gpu_activity_t *ga,
 CUpti_ActivityMemcpy2 *activity,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "Memcpy2 copy CorrelationId %u", activity->correlationId);
  TMSG(CUPTI_ACTIVITY, "Memcpy2 copy kind %u", activity->copyKind);
  TMSG(CUPTI_ACTIVITY, "Memcpy2 copy bytes %lu", activity->bytes);

  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_id_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_MEMCPY;
  ga->details.memcpy.correlation_id = host_correlation_id;
  ga->details.memcpy.device_id = activity->deviceId;
  ga->details.memcpy.context_id = activity->contextId;
  ga->details.memcpy.stream_id = activity->streamId;
  ga->details.memcpy.copyKind = convert_memcpy_type(activity->copyKind);
  ga->details.memcpy.bytes = activity->bytes;

  gpu_interval_set(&ga->details.interval, activity->start, activity->end);

  return true;
}


static bool
convert_kernel
(
 gpu_activity_t *ga,
 CUpti_ActivityKernel4 *activity,
 uint64_t *correlation_id
)
{
  if (cuda_device_map_lookup(activity->deviceId) == NULL) {
    cuda_device_map_insert(activity->deviceId);
  }

  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;

  ga->kind = GPU_ACTIVITY_KERNEL;
  ga->details.kernel.correlation_id = host_correlation_id;
  ga->details.kernel.dynamicSharedMemory = activity->dynamicSharedMemory;
  ga->details.kernel.staticSharedMemory = activity->staticSharedMemory;
  ga->details.kernel.localMemoryTotal = activity->localMemoryTotal;
  ga->details.kernel.device_id = activity->deviceId;
  ga->details.kernel.context_id = activity->contextId;
  ga->details.kernel.stream_id = activity->streamId;
  ga->details.kernel.blocks = activity->blockX * activity->blockY * activity->blockZ;

  gpu_interval_set(&ga->details.interval, activity->start, activity->end);

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
  ga->details.kernel.kernel_first_pc = ip_normalized_NULL;

  cuda_kernel_data_map_insert(activity->correlationId,
    (cuda_kernel_data_map_entry_value_t) {
      .device_id = ga->details.kernel.device_id,
      .start = ga->details.interval.start,
      .end = ga->details.interval.end
  });

  return true;
}


static bool
convert_function
(
 gpu_activity_t *ga,
 CUpti_ActivityFunction *activity,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "Function id %u", activity->id);

  ip_normalized_t pc = cubin_id_transform(activity->moduleId,
    activity->functionIndex, 0);

  gpu_function_id_map_insert(activity->id, pc);

  return false;
}


static bool
convert_global_access
(
 gpu_activity_t *ga,
 CUpti_ActivityGlobalAccess3 *activity,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_GLOBAL_ACCESS;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId,
           activity->functionId, activity->pcOffset, correlation_id);

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

  return true;
}


static bool
convert_shared_access
(
 gpu_activity_t *ga,
 CUpti_ActivitySharedAccess *activity,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_LOCAL_ACCESS;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId,
           activity->functionId, activity->pcOffset, correlation_id);

  ga->details.local_access.sharedTransactions =
    activity->sharedTransactions;

  ga->details.local_access.theoreticalSharedTransactions =
    activity->theoreticalSharedTransactions;

  ga->details.local_access.type =
    ((activity->flags & (1<<8)) ?
     GPU_LOCAL_ACCESS_LOAD :
     GPU_LOCAL_ACCESS_STORE);

  ga->details.local_access.bytes = (activity->flags & 0xFF) *
    activity->threadsExecuted;

  return true;
}


static bool
convert_branch
(
 gpu_activity_t *ga,
 CUpti_ActivityBranch2 *activity,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_BRANCH;

  set_gpu_instruction_fields(&ga->details.instruction, activity->correlationId,
           activity->functionId, activity->pcOffset, correlation_id);

  ga->details.branch.diverged = activity->diverged;
  ga->details.branch.executed = activity->executed;

  return true;
}


#if TRACK_SYNCHRONIZATION
static bool
convert_synchronization
(
 gpu_activity_t *ga,
 CUpti_ActivitySynchronization *activity_sync,
 uint64_t *correlation_id
)
{
  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_SYNCHRONIZATION;
  ga->details.synchronization.correlation_id = host_correlation_id;
  ga->details.synchronization.context_id = activity_sync->contextId;
  ga->details.synchronization.stream_id = activity_sync->streamId;
  ga->details.synchronization.event_id = activity_sync->cudaEventId;
  ga->details.synchronization.syncKind = convert_sync_type(activity_sync->type);

  gpu_interval_set(&ga->details.interval, activity_sync->start, activity_sync->end);

  return true;
}
#endif


static bool
convert_memory
(
 gpu_activity_t *ga,
 CUpti_ActivityMemory *activity_mem,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_MEMORY;
  // TODO CUPTI_ACTIVITY_KIND_MEMORY record do not have correlation ID field
  // ga->details.memory.correlation_id = host_correlation_id;
  ga->details.memory.memKind = activity_mem->memoryKind;
  ga->details.memory.bytes = activity_mem->bytes;
  ga->details.memory.device_id = activity_mem->deviceId;
  ga->details.memory.context_id = activity_mem->contextId;

  gpu_interval_set(&ga->details.interval, activity_mem->start, activity_mem->end);

  return true;
}


static bool
convert_memset
(
 gpu_activity_t *ga,
 CUpti_ActivityMemset *activity,
 uint64_t *correlation_id
)
{
  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_id_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_MEMSET;
  ga->details.memset.correlation_id = activity->correlationId;
  ga->details.memset.device_id = activity->deviceId;
  ga->details.memset.context_id = activity->contextId;
  ga->details.memset.stream_id = activity->streamId;
  ga->details.memset.memKind = activity->memoryKind;
  ga->details.memset.bytes = activity->bytes;

  gpu_interval_set(&ga->details.interval, activity->start, activity->end);

  return true;
}


static bool
convert_correlation
(
 gpu_activity_t *ga,
 CUpti_ActivityExternalCorrelation* activity,
 uint64_t *correlation_id
)
{
  TMSG(CUPTI_ACTIVITY, "External CorrelationId %lu", activity->externalId);
  TMSG(CUPTI_ACTIVITY, "CorrelationId %u", activity->correlationId);

  if (cuda_correlation_id_map_lookup(activity->correlationId) == NULL) {
    cuda_correlation_id_map_insert(activity->correlationId, activity->externalId);
  } else {
    TMSG(CUPTI_ACTIVITY,
      "Duplicate External CorrelationId %lu", activity->externalId);
  }

  return false;
}

static bool
convert_cdpkernel
(
 gpu_activity_t *ga,
 CUpti_ActivityCdpKernel *activity,
 uint64_t *correlation_id
)
{
  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_id_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_CDP_KERNEL;
  ga->details.cdpkernel.correlation_id = host_correlation_id;
  ga->details.cdpkernel.device_id = activity->deviceId;
  ga->details.cdpkernel.context_id = activity->contextId;
  ga->details.cdpkernel.stream_id = activity->streamId;

  gpu_interval_set(&ga->details.interval, activity->start, activity->end);

  return true;
}

static bool
convert_event
(
 gpu_activity_t *ga,
 CUpti_ActivityCudaEvent *activity,
 uint64_t *correlation_id
)
{
  uint64_t host_correlation_id;
  if (!get_host_correlation_id(activity->correlationId, &host_correlation_id)) {
    return false;
  }
  *correlation_id = host_correlation_id;
  cuda_correlation_id_map_delete(activity->correlationId);

  ga->kind = GPU_ACTIVITY_EVENT;
  ga->details.event.correlation_id = host_correlation_id;
  ga->details.event.event_id = activity->eventId;
  ga->details.event.context_id = activity->contextId;
  ga->details.event.stream_id = activity->streamId;

  return true;
}

bool
convert_unknown
(
 gpu_activity_t *ga,
 CUpti_Activity *activity,
 uint64_t *correlation_id
)
{
  return false;
}



//******************************************************************************
// interface operations
//******************************************************************************

bool
cupti_activity_translate
(
 gpu_activity_t *ga,
 CUpti_Activity *activity,
 uint64_t *correlation_id
)
{
  gpu_activity_init(ga);

  bool translated = false;
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    translated =
      convert_pcsampling (ga, (CUpti_ActivityPCSamplingVersion *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    translated = convert_pcsampling_record_info
      (ga, (CUpti_ActivityPCSamplingRecordInfo *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY2:
    translated = convert_memcpy2(ga, (CUpti_ActivityMemcpy2 *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY:
    translated = convert_memcpy(ga, (CUpti_ActivityMemcpy *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_KERNEL:
  case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
    translated = convert_kernel(ga, (CUpti_ActivityKernel4 *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_FUNCTION:
    translated = convert_function(ga, (CUpti_ActivityFunction *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
    translated =
      convert_global_access(ga, (CUpti_ActivityGlobalAccess3 *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
    translated =
      convert_shared_access(ga, (CUpti_ActivitySharedAccess *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_BRANCH:
    translated = convert_branch(ga, (CUpti_ActivityBranch2 *) activity, correlation_id);
    break;

#if TRACK_SYNCHRONIZATION
  // Ignore synchronization events as the time stamps are from the host.
  case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    translated =
      convert_synchronization(ga, (CUpti_ActivitySynchronization *) activity, correlation_id);
    break;
#endif

  case CUPTI_ACTIVITY_KIND_MEMORY:
    translated = convert_memory(ga, (CUpti_ActivityMemory *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_MEMSET:
    translated = convert_memset(ga, (CUpti_ActivityMemset *) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION:
    translated =
      convert_correlation(ga, (CUpti_ActivityExternalCorrelation*) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_CDP_KERNEL:
    translated = convert_cdpkernel(ga, (CUpti_ActivityCdpKernel*) activity, correlation_id);
    break;

  case CUPTI_ACTIVITY_KIND_CUDA_EVENT:
    translated = convert_event(ga, (CUpti_ActivityCudaEvent *) activity, correlation_id);
    break;

  default:
    translated = convert_unknown(ga, activity, correlation_id);
    break;
  }

  cstack_ptr_set(&(ga->next), 0);
  return translated;
}
