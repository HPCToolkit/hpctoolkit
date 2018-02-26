#include <stdio.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/ompt/ompt-interface.h>
#include "cupti-ompt-api.h"
#include "nvidia.h"
#include "cubin-id-map.h"
#include "cupti-activity-strings.h"
#include "cupti-correlation-id-map.h"
#include "cupti-activity-queue.h"
#include "cupti-function-id-map.h"
#include "cupti-host-op-map.h"

#define HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE (64 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT (8)
#define HPCRUN_CUPTI_CALL(fn, args) \
{      \
    CUptiResult status = fn args; \
    if (status != CUPTI_SUCCESS) { \
      cupti_error_report(status, #fn); \
    }\
}

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

//-------------------------------------------------------------
// General functions
//-------------------------------------------------------------

static void
cupti_unknown_process
(
 CUpti_Activity *activity
)    
{
  PRINT("Unknown activity kind %d\n", activity->kind);
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
  //PRINT("source %u, functionId %u, pc 0x%x, corr %u, "
	// "samples %u, latencySamples %u, stallreason %s\n",
	// sample->sourceLocatorId,
	// sample->functionId,
	// sample->pcOffset,
	// sample->correlationId,
	// sample->samples,
	// sample->latencySamples,
	// cupti_stall_reason_string(sample->stallReason));
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(sample->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    //PRINT("external_id %d\n", external_id);
    cupti_function_id_map_entry_t *entry = cupti_function_id_map_lookup(sample->functionId);
    if (entry != NULL) {
      uint64_t function_index = cupti_function_id_map_entry_function_index_get(entry);
      uint64_t cubin_id = cupti_function_id_map_entry_cubin_id_get(entry);
      ip_normalized_t ip = cubin_id_transform(cubin_id, function_index, sample->pcOffset);
      cct_addr_t frm = { .ip_norm = ip };
      // TODO(keren): directly link to target node
      cct_node_t *cct_node = hpcrun_ompt_op_id_map_lookup(external_id);
      cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
      cupti_activity_queue_entry_t **queue = cupti_host_op_map_entry_activity_queue_get(host_op_entry);
      if (cct_node != NULL) {
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(cct_node, &frm)) != NULL) {
          cupti_activity_queue_push(queue, CUPTI_ACTIVITY_KIND_PC_SAMPLING, (void *)sample, cct_child);
        }
      }
    }
  }
}


static void
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
  cupti_function_id_map_insert(af->id, af->functionIndex, af->moduleId);
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
}


static void
cupti_correlation_process
(
 CUpti_ActivityExternalCorrelation *ec
)
{
  uint64_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->externalId;
  if (hpcrun_ompt_op_id_map_lookup(external_id) != NULL) {
    if (cupti_correlation_id_map_lookup(correlation_id) != NULL) {
      cupti_correlation_id_map_external_id_replace(correlation_id, external_id);
    } else {
      cupti_correlation_id_map_insert(correlation_id, external_id);
    }
  } else {
    PRINT("External CorrelationId %lu cannot be found\n", external_id);
  }
  PRINT("External CorrelationId %lu\n", external_id);
  PRINT("CorrelationId %lu\n", correlation_id);
}


static void
cupti_memcpy_process
(
 CUpti_ActivityMemcpy *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = cupti_host_op_map_entry_activity_queue_get(host_op_entry);
    cct_node_t *node = hpcrun_ompt_op_id_map_lookup(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, CUPTI_ACTIVITY_KIND_MEMCPY, (void *)activity, node);
    }
  } else {
    PRINT("Memcpy copy CorrelationId %u cannot be found\n", activity->correlationId);
  }
  PRINT("Memcpy copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy copy kind %u\n", activity->copyKind);
  PRINT("Memcpy copy bytes %u\n", activity->bytes);
}


static void
cupti_memcpy2_process
(
 CUpti_ActivityMemcpy2 *activity 
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = cupti_host_op_map_entry_activity_queue_get(host_op_entry);
    cct_node_t *node = hpcrun_ompt_op_id_map_lookup(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, CUPTI_ACTIVITY_KIND_MEMCPY2, (void *)activity, node);
    }
  }
  PRINT("Memcpy2 copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy2 copy kind %u\n", activity->copyKind);
  PRINT("Memcpy2 copy bytes %u\n", activity->bytes);
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
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = cupti_host_op_map_entry_activity_queue_get(host_op_entry);
    cct_node_t *node = hpcrun_ompt_op_id_map_lookup(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, CUPTI_ACTIVITY_KIND_KERNEL, (void *)activity, node);
    }
  }
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


void
cupti_ompt_activity_process
(
 CUpti_Activity *activity
)
{
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR:
    cupti_source_locator_process((CUpti_ActivitySourceLocator *) activity); 
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    cupti_sample_process(activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    cupti_sampling_record_info_process
      ((CUpti_ActivityPCSamplingRecordInfo *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_FUNCTION:
    cupti_function_process((CUpti_ActivityFunction *) activity);
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

  case CUPTI_ACTIVITY_KIND_EVENT:
    break;

  default:
    cupti_unknown_process(activity);
    break;
  }
}


bool
cupti_ompt_buffer_cursor_advance
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity *current,
  CUpti_Activity **next
)
{
  CUpti_Activity *cursor = (CUpti_Activity *)current;
  bool result = (cuptiActivityGetNextRecord(buffer, size, &cursor) == CUPTI_SUCCESS);
  if (result) {
    *next = (CUpti_Activity *)cursor;
  }
  return result;
}


void
cupti_ompt_activity_flush
(
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityFlushAll, (CUPTI_ACTIVITY_FLAG_FLUSH_FORCED));
}
