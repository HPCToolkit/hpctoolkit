//-------------------------------------------------------------
// General functions
//-------------------------------------------------------------

static void
gpu_unknown_process
(
 gpu_activity_t *activity
)
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}

static void
gpu_sample_process
(
 gpu_activity_t* sample
)
{
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(sample->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cupti_entry);
      // TODO: how to refactor this function id map
    cupti_function_id_map_entry_t *entry =
      cupti_function_id_map_lookup(sample->details.pc_sampling.functionId);
    if (entry != NULL) {
      uint32_t function_index =
        cupti_function_id_map_entry_function_index_get(entry);
      uint32_t cubin_id = cupti_function_id_map_entry_cubin_id_get(entry);
      ip_normalized_t ip =
        cubin_id_transform(cubin_id, function_index, sample->details.pc_sampling.pcOffset);
      cct_addr_t frm = { .ip_norm = ip };
      gpu_host_correlation_map_entry_t *host_op_entry =
        gpu_host_correlation_map_lookup(external_id);
      if (host_op_entry != NULL) {
        PRINT("external_id %d\n", external_id);
        if (!gpu_host_correlation_map_samples_increase(external_id, sample->details.pc_sampling.samples)) {
          gpu_correlation_id_map_delete(sample->correlationId);
        }
        cct_node_t *host_op_node =
          gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
          PRINT("frm %d\n", ip);
          gpu_record_t *record =
            gpu_host_correlation_map_entry_record_get(host_op_entry);
            cupti_activity_produce(sample, cct_child,
            record);
        }
      } else {
        PRINT("host_map_entry %d not found\n", external_id);
      }
    } else {
      PRINT("host_correlation_map_entry %lu not found\n", external_id);
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
gpu_sampling_record_info_process
(
 gpu_activity_t *sri
)
{
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(sri->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      gpu_record_t *record =
        gpu_host_correlation_map_entry_record_get(host_op_entry);
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);
        cupti_activity_produce(sri, host_op_node, record);
    }
    // sample record info is the last record for a given correlation id
    if (!gpu_host_correlation_map_total_samples_update
      (external_id, sri->details.pc_sampling_record_info.totalSamples - sri->details.pc_sampling_record_info.droppedSamples)) {
      gpu_correlation_id_map_delete(sri->correlationId);
    }
  }
  hpcrun_stats_acc_samples_add(sri->details.pc_sampling_record_info.totalSamples);
  hpcrun_stats_acc_samples_dropped_add(sri->details.pc_sampling_record_info.droppedSamples);
}


static void
gpu_correlation_process
(
 gpu_activity_t *ec
)
{
  uint32_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->details.correlation.externalId;
  if (gpu_correlation_id_map_lookup(correlation_id) == NULL) {
    gpu_correlation_id_map_insert(correlation_id, external_id);
  } else {
    PRINT("External CorrelationId Replace %lu)\n", external_id);
    gpu_correlation_id_map_external_id_replace(correlation_id, external_id);
  }
}


static void
gpu_memcpy_process
(
 gpu_activity_process *activity
)
{
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        cupti_host_correlation_map_entry_host_op_node_get(host_op_entry);
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = host_op_node
      };
      cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
        cupti_trace_append, &entry_trace);

      cupti_activity_channel_t *channel =
        cupti_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity,
        host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  } else {
    PRINT("Memcpy copy CorrelationId %u cannot be found\n",
      activity->correlationId);
  }
}

static void
gpu_memset_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cupti_entry = gpu_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = gpu_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry = gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        cupti_host_correlation_map_entry_host_op_node_get(host_op_entry);
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = host_op_node
      };
      cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
        cupti_trace_append, &entry_trace);

      cupti_activity_channel_t *channel =
        cupti_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Memset CorrelationId %u\n", activity->correlationId);
  PRINT("Memset kind %u\n", activity->memoryKind);
  PRINT("Memset bytes %lu\n", activity->bytes);
}


static void
gpu_memctr_process
(
 gpu_activity_t *activity
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
gpu_activityAPI_process
(
 gpu_activity_t *activity
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
gpu_kernel_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  // TODO: how to refactor this device map
  if (cupti_entry != NULL) {
    if (cuda_device_map_lookup(activity->deviceId) == NULL) {
      cuda_device_map_insert(activity->deviceId);
    }
    gpu_correlation_id_map_kernel_update(activity->correlationId,
      activity->deviceId, activity->start, activity->end);
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *func_node = 
        cupti_host_correlation_map_entry_func_node_get(host_op_entry);
      // do not delete it because it shares external_id with activity samples
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = func_node
      };
      cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
        cupti_trace_append, &entry_trace);

      cupti_activity_channel_t *channel = 
        cupti_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, func_node);
    }
  }

  PRINT("Kernel execution deviceId %u\n", activity->deviceId);
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


static void
gpu_synchronization_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cupti_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        cupti_host_correlation_map_entry_host_op_node_get(host_op_entry);
      cupti_activity_channel_t *channel =
        cupti_host_correlation_map_entry_activity_channel_get(host_op_entry);
      cupti_activity_channel_produce(channel, (CUpti_Activity *)activity, host_op_node);
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = host_op_node
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
      // TODO(Keren): handle event synchronization
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Synchronization CorrelationId %u\n", activity->correlationId);
}


static void
gpu_cdpkernel_process
(
 gpu_activity_t *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry =
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_correlation_map_entry_t *host_op_entry =
      cupti_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        cupti_host_correlation_map_entry_host_op_node_get(host_op_entry);
      cupti_entry_trace_t entry_trace = {
        .start = activity->start,
        .end = activity->end,
        .node = host_op_node
      };
      cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
        cupti_trace_append, &entry_trace);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Cdp Kernel CorrelationId %u\n", activity->correlationId);
}


static void
gpu_memory_process
(
 gpu_activity_t *activity
)
{
  PRINT("Memory process not implemented\n");
}


static void
gpu_instruction_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->correlationId;
  uint32_t pc_offset = activity->details.instruction.pcOffset;    
  gpu_correlation_id_map_entry_t *cupti_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cupti_entry != NULL) {
    uint64_t external_id =
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    PRINT("external_id %lu\n", external_id);
    cupti_host_correlation_map_entry_t *host_op_entry = 
      cupti_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      // Function node has the start pc of the function
      cct_node_t *func_node = 
        cupti_host_correlation_map_entry_func_node_get(host_op_entry);
      cct_addr_t *func_addr = hpcrun_cct_addr(func_node);
      ip_normalized_t ip_norm = {
        .lm_id = func_addr->ip_norm.lm_id,
        .lm_ip = func_addr->ip_norm.lm_ip + pc_offset
      };
      cct_addr_t frm = { .ip_norm = ip_norm };
      cct_node_t *cct_child = NULL;
      if ((cct_child = hpcrun_cct_insert_addr(func_node, &frm)) != NULL) {
        cupti_activity_channel_t *channel = 
          cupti_host_correlation_map_entry_activity_channel_get(host_op_entry);
        cupti_activity_channel_produce(channel, activity, cct_child);
      }
    }
  }
  PRINT("Instruction correlationId %u\n", correlation_id);
}


//******************************************************************************
// activity processing
//******************************************************************************

void
gpu_activity_process
(
 gpu_activity_t *ga
)
{
  switch (ga->kind) {

  // unused records
  case GPU_ACTIVITY_KIND_SOURCE_LOCATOR:
  case GPU_ACTIVITY_KIND_FUNCTION:
    break;

  case GPU_ACTIVITY_KIND_PC_SAMPLING:
    gpu_sample_process(ga);
    break;

  case GPU_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    gpu_sampling_record_info_process(ga);
    break;

  case GPU_ACTIVITY_KIND_EXTERNAL_CORRELATION: 
    gpu_correlation_process(ga);
    break;

  case GPU_ACTIVITY_KIND_MEMCPY:
    gpu_memcpy_process(ga);
    break;
/*
  case GPU_ACTIVITY_KIND_MEMCPY2:
    gpu_memcpy2_process(ga);
    break;
*/
  case GPU_ACTIVITY_KIND_MEMSET:
    gpu_memset_process(ga);

  case GPU_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    gpu_memctr_process(ga);
    break;

  case GPU_ACTIVITY_KIND_DRIVER:
  //case CUPTI_ACTIVITY_KIND_RUNTIME:
    gpu_activityAPI_process(ga);
    break;

  case GPU_ACTIVITY_KIND_KERNEL:
    gpu_kernel_process(ga);
    break;

  case GPU_ACTIVITY_KIND_SYNCHRONIZATION:
    gpu_synchronization_process(ga);
    break;

  case GPU_ACTIVITY_KIND_MEMORY:
    gpu_memory_process(ga);
    break;

  case GPU_ACTIVITY_KIND_SHARED_ACCESS:
  case GPU_ACTIVITY_KIND_GLOBAL_ACCESS:
  case GPU_ACTIVITY_KIND_BRANCH:
    gpu_instruction_process(ga);
    break;

  case GPU_ACTIVITY_KIND_CDP_KERNEL:
     gpu_cdpkernel_process(ga);
     break;

  default:
    gpu_unknown_process(ga);
    break;
  }
}

