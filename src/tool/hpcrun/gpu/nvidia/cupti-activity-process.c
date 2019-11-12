#include "cupti-activity-process.h"
#include "cupti-activity-process.h"

#include <cupti.h>
#include <hpcrun/gpu/gpu-activity.h>

static void
cupti_unknown_process
(
 CUpti_Activity *activity
)
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}


static void
__attribute__ ((unused))
cupti_device_process
(
 CUpti_ActivityDevice2 *device
)
{
  PRINT("Device id %d\n", device->id);
}


static void
cupti_sample_process
(
 void *record
)
{
#if CUPTI_API_VERSION >= 10
  CUpti_ActivityPCSampling3 *sample = (CUpti_ActivityPCSampling3 *)record;
#else
  CUpti_ActivityPCSampling2 *sample = (CUpti_ActivityPCSampling2 *)record;
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

  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(sample->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry = 
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      PRINT("external_id %lu\n", external_id);
      // Delete a host op node if all the samples with the node are processed
      if (!gpu_host_correlation_map_samples_increase(external_id, sample->samples)) {
        cupti_correlation_id_map_delete(sample->correlationId);
      }
      // Function node has the start pc of the function
      cct_node_t *kernel_node = 
        gpu_host_correlation_map_entry_kernel_node_get(host_op_entry);
      cct_node_t *trace_node = 
        gpu_host_correlation_map_entry_trace_node_get(host_op_entry);
      // XXX(keren): for now, suppose a runtime api only launches a single kernel
      cct_node_t *func_node = hpcrun_cct_children(trace_node);
      if (func_node != NULL) {
        cct_addr_t *func_addr = hpcrun_cct_addr(func_node);
        ip_normalized_t ip_norm = {
          .lm_id = func_addr->ip_norm.lm_id,
          .lm_ip = func_addr->ip_norm.lm_ip + sample->pcOffset
        };
        cct_addr_t frm = { .ip_norm = ip_norm };
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(kernel_node, &frm)) != NULL) {
          cupti_activity_channel_t *channel = 
            gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
          cupti_activity_channel_produce(channel, (CUpti_Activity *)sample, cct_child);
        }
      } else {
        PRINT("host_op_map_entry %lu, func node not found\n", external_id);
      }
    } else {
      PRINT("host_op_map_entry %lu not found\n", external_id);
    }
  }
}


static void
__attribute__ ((unused))
cupti_source_locator_process
(
 CUpti_ActivitySourceLocator *asl
)
{
  PRINT("Source Locator Id %d, File %s Line %d\n",
    asl->id, asl->fileName,
    asl->lineNumber);
}


static void
__attribute__ ((unused))
cupti_function_process
(
 CUpti_ActivityFunction *af
)
{
  PRINT("Function Id %u, ctx %u, moduleId %u, functionIndex %u, name %s\n",
    af->id,
    af->contextId,
    af->moduleId,
    af->functionIndex,
    af->name);
  ip_normalized_t ip_norm = cubin_id_transform(af->moduleId, af->functionIndex, 0);
  cupti_function_id_map_insert(af->id, ip_norm);
}


static void
cupti_sampling_record_info_process
(
 CUpti_ActivityPCSamplingRecordInfo *sri
)
{
  PRINT("corr %u, totalSamples %llu, droppedSamples %llu\n",
   sri->correlationId,
   (unsigned long long)sri->totalSamples,
   (unsigned long long)sri->droppedSamples);
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(sri->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cupti_activity_channel_t *channel =
        gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cct_node_t *kernel_node = 
        gpu_host_correlation_map_entry_kernel_node_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)sri, kernel_node);
    }
    // sample record info is the last record for a given correlation id
    if (!gpu_host_correlation_map_total_samples_update
      (external_id, sri->totalSamples - sri->droppedSamples)) {
      cupti_correlation_id_map_delete(sri->correlationId);
    }
  }
  hpcrun_stats_acc_samples_add(sri->totalSamples);
  hpcrun_stats_acc_samples_dropped_add(sri->droppedSamples);
}


static void
cupti_correlation_process
(
 CUpti_ActivityExternalCorrelation *ec
)
{
  uint32_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->externalId;
  if (cupti_correlation_id_map_lookup(correlation_id) == NULL) {
    cupti_correlation_id_map_insert(correlation_id, external_id);
  } else {
    PRINT("External CorrelationId Replace %lu)\n", external_id);
    cupti_correlation_id_map_external_id_replace(correlation_id, external_id);
  }
  PRINT("External CorrelationId %lu\n", external_id);
  PRINT("CorrelationId %u\n", correlation_id);
}


static void
cupti_memcpy_process
(
 CUpti_ActivityMemcpy *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = NULL;
      if (activity->copyKind == CUPTI_ACTIVITY_MEMCPY_KIND_HTOD) {
        host_op_node = gpu_host_correlation_map_entry_copyin_node_get(host_op_entry);
      } else if (activity->copyKind == CUPTI_ACTIVITY_MEMCPY_KIND_DTOH) {
        host_op_node = gpu_host_correlation_map_entry_copyout_node_get(host_op_entry);
      }
      if (host_op_node == NULL) { // If we cannot find a perfect match for the operation
        host_op_node = gpu_host_correlation_map_entry_copy_node_get(host_op_entry);
      }
      if (hpcrun_trace_isactive()) {
        cupti_entry_trace_t entry_trace = {
          .start = activity->start,
          .end = activity->end,
          .node = host_op_node
        };
        cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
          cupti_trace_append, &entry_trace);
      }

      cupti_activity_channel_t *channel =
        gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity,
        host_op_node);
      //FIXME(keren): How to clear ops
      //gpu_host_correlation_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  } else {
    PRINT("Memcpy copy CorrelationId %u cannot be found\n",
      activity->correlationId);
  }
  PRINT("Memcpy copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy copy kind %u\n", activity->copyKind);
  PRINT("Memcpy copy bytes %lu\n", activity->bytes);
}


static void
cupti_memcpy2_process
(
 CUpti_ActivityMemcpy2 *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_copy_node_get(host_op_entry);
      if (hpcrun_trace_isactive()) {
        cupti_entry_trace_t entry_trace = {
          .start = activity->start,
          .end = activity->end,
          .node = host_op_node
        };
        cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
          cupti_trace_append, &entry_trace);
      }

      cupti_activity_channel_t *channel =
        gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity,
        host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Memcpy2 copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy2 copy kind %u\n", activity->copyKind);
  PRINT("Memcpy2 copy bytes %lu\n", activity->bytes);
}


static void
cupti_memset_process
(
 CUpti_ActivityMemset *activity
)
{
// TODO(Keren): support it later
#if 0
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry = gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = host_op_node
      };
      cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
        cupti_trace_append, &entry_trace);

      cupti_activity_channel_t *channel =
        gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
#endif
  PRINT("Memset CorrelationId %u\n", activity->correlationId);
  PRINT("Memset kind %u\n", activity->memoryKind);
  PRINT("Memset bytes %lu\n", activity->bytes);
}


static void
cupti_memctr_process
(
 CUpti_ActivityUnifiedMemoryCounter *activity
)
{
  // FIXME(keren): no correlationId field
  //cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  //if (cupti_entry != NULL) {
  //  uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
  //  cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
  //  cupti_attribute_activity(activity, node);
  //}
  //PRINT("Unified memory copy\n");
}


static void
cupti_activityAPI_process
(
 CUpti_ActivityAPI *activity
)
{
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_DRIVER:
    {
      break;
    }
    case CUPTI_ACTIVITY_KIND_RUNTIME:
    {
      break;
    }
    default:
      break;
  }
}


static void
cupti_kernel_process
(
 CUpti_ActivityKernel4 *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    if (cuda_device_map_lookup(activity->deviceId) == NULL) {
      cuda_device_map_insert(activity->deviceId);
    }
    cupti_correlation_id_map_kernel_update(activity->correlationId,
      activity->deviceId, activity->start, activity->end);
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = 
        gpu_host_correlation_map_entry_trace_node_get(host_op_entry);
      cct_node_t *func_node = hpcrun_cct_children(host_op_node);
      if (func_node != NULL) {
        if (hpcrun_trace_isactive()) {
          // do not delete it because it shares external_id with activity samples
          cupti_entry_trace_t entry_trace = {
            .start = activity->start,
            .end = activity->end,
            .node = func_node
          };
          cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
            cupti_trace_append, &entry_trace);
        }

        cupti_activity_channel_t *channel = 
          gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
        cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, host_op_node);
      } else {
        PRINT("host_op_entry %lu, func node not found\n", external_id);
      }
    }
  }

  PRINT("Kernel execution deviceId %u\n", activity->deviceId);
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


static void
cupti_synchronization_process
(
 CUpti_ActivitySynchronization *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *sync_node =
        gpu_host_correlation_map_entry_sync_node_get(host_op_entry);
      cupti_activity_channel_t *channel =
        gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
      if (hpcrun_trace_isactive()) {
        cupti_entry_trace_t entry_trace = {
          .start = activity->start,
          .end = activity->end,
          .node = sync_node
        };
        if (activity->type == CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_STREAM_SYNCHRONIZE) {
          // Insert a event for a specific stream
          PRINT("Add context %u stream %u sync\n", activity->contextId, activity->streamId);
          cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
            cupti_trace_append, &entry_trace); 
        } else if (activity->type == CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_CONTEXT_SYNCHRONIZE) {
          // Insert events for all current active streams
          // TODO(Keren): What if the stream is created
          PRINT("Add context %u sync\n", activity->contextId);
          cupti_context_id_map_context_process(activity->contextId, cupti_trace_append, &entry_trace);
        }
      }
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, sync_node);
      // TODO(Keren): handle event synchronization
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    } else {
      PRINT("host_op_map external id %lu not found\n", external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Synchronization CorrelationId %u\n", activity->correlationId);
}


static void
cupti_cdpkernel_process
(
 CUpti_ActivityCdpKernel *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_trace_node_get(host_op_entry);
      cct_node_t *func_node = hpcrun_cct_children(host_op_node);
      if (func_node != NULL) {
        if (hpcrun_trace_isactive()) {
          cupti_entry_trace_t entry_trace = {
            .start = activity->start,
            .end = activity->end,
            .node = func_node
          };
          cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
            cupti_trace_append, &entry_trace);
        }
      } else {
        PRINT("host_op_entry %lu, func_node not found\n", external_id);
      }
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Cdp Kernel CorrelationId %u\n", activity->correlationId);
}


static void
cupti_memory_process
(
 CUpti_ActivityMemory *activity
)
{
  PRINT("Memory process not implemented\n");
}


static void
cupti_instruction_process
(
 CUpti_Activity *activity
)
{
  uint32_t correlation_id = 0;
  uint32_t pc_offset = 0;
  if (activity->kind == CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS) {
    CUpti_ActivityGlobalAccess3 *global_access =
      (CUpti_ActivityGlobalAccess3 *)activity;
    correlation_id = global_access->correlationId;
    pc_offset = global_access->pcOffset;
  } else if (activity->kind == CUPTI_ACTIVITY_KIND_SHARED_ACCESS) {
    CUpti_ActivitySharedAccess *shared_access =
      (CUpti_ActivitySharedAccess *)activity;
    correlation_id = shared_access->correlationId;
    pc_offset = shared_access->pcOffset;
  } else if (activity->kind == CUPTI_ACTIVITY_KIND_BRANCH) {
    CUpti_ActivityBranch2 *branch = (CUpti_ActivityBranch2 *)activity;
    correlation_id = branch->correlationId;
    pc_offset = branch->pcOffset;
    PRINT("Branch instruction\n");
  }
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(correlation_id);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    PRINT("external_id %lu\n", external_id);
    gpu_host_correlation_map_entry_t *host_op_entry = 
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      // Function node has the start pc of the function
      cct_node_t *kernel_node = 
        gpu_host_correlation_map_entry_kernel_node_get(host_op_entry);
      cct_node_t *trace_node = 
        gpu_host_correlation_map_entry_trace_node_get(host_op_entry);
      cct_node_t *func_node = hpcrun_cct_children(trace_node);
      if (func_node != NULL) {
        cct_addr_t *func_addr = hpcrun_cct_addr(func_node);
        ip_normalized_t ip_norm = {
          .lm_id = func_addr->ip_norm.lm_id,
          .lm_ip = func_addr->ip_norm.lm_ip + pc_offset
        };
        cct_addr_t frm = { .ip_norm = ip_norm };
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(kernel_node, &frm)) != NULL) {
          cupti_activity_channel_t *channel = 
            gpu_host_correlation_map_entry_activity_channel_get(host_op_entry);
          cupti_activity_channel_produce(channel, activity, cct_child);
        }
      } else {
        PRINT("host_op_entry %lu, func node not found\n", external_id);
      }
    }
  }
  PRINT("Instruction correlationId %u\n", correlation_id);
}


//******************************************************************************
// activity processing
//******************************************************************************

void
cupti_activity_process
(
 CUpti_Activity *activity
)
{
  switch (activity->kind) {

  // unused functions
  case CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR:
  case CUPTI_ACTIVITY_KIND_FUNCTION:
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    cupti_sample_process(activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    cupti_sampling_record_info_process(
      (CUpti_ActivityPCSamplingRecordInfo *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION: 
    cupti_correlation_process((CUpti_ActivityExternalCorrelation *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY:
    cupti_memcpy_process((CUpti_ActivityMemcpy *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY2:
    cupti_memcpy2_process((CUpti_ActivityMemcpy2 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMSET:
    cupti_memset_process((CUpti_ActivityMemset *) activity);

  case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    cupti_memctr_process((CUpti_ActivityUnifiedMemoryCounter *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_DRIVER:
  case CUPTI_ACTIVITY_KIND_RUNTIME:
    cupti_activityAPI_process((CUpti_ActivityAPI *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_KERNEL:
    cupti_kernel_process((CUpti_ActivityKernel4 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    cupti_synchronization_process((CUpti_ActivitySynchronization *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMORY:
    cupti_memory_process((CUpti_ActivityMemory *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
  case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
  case CUPTI_ACTIVITY_KIND_BRANCH:
    cupti_instruction_process(activity);
    break;
  case CUPTI_ACTIVITY_KIND_CDP_KERNEL:
     cupti_cdpkernel_process((CUpti_ActivityCdpKernel*)activity);
     break;
  default:
    cupti_unknown_process(activity);
    break;
  }
}

#if 0
void
cupti_activity_handle(cupti_entry_activity_t *entry)
{
  cupti_activity_attribute(&(entry->activity), entry->cct_node);
}


void
cupti_correlation_handle(cupti_entry_correlation_t *entry)
{
  gpu_host_correlation_map_insert(entry->host_op_id,
    entry->activity_channel, entry->copy_node, entry->copyin_node,
    entry->copyout_node, entry->alloc_node, entry->delete_node,
    entry->sync_node, entry->kernel_node, entry->trace_node);
}
#endif
