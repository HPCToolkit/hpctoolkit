#include <stdio.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/sample-sources/nvidia.h>

#include "cupti-activity-api.h"
#include "cupti-activity-strings.h"
#include "cupti-correlation-id-map.h"
#include "cupti-activity-queue.h"
#include "ompt-function-id-map.h"
#include "ompt-host-op-map.h"
#include "ompt-interface.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

static void
cupti_process_unknown
(
 CUpti_Activity *activity,
 void *state
)    
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}


static void
cupti_process_sample
(
 CUpti_ActivityPCSampling2 *sample,
 void *state
)
{
  PRINT("source %u, functionId %u, pc 0x%x, corr %u, "
	 "samples %u, latencySamples %u, stallreason %s\n",
	 sample->sourceLocatorId,
	 sample->functionId,
	 sample->pcOffset,
	 sample->correlationId,
	 sample->samples,
	 sample->latencySamples,
	 cupti_stall_reason_string(sample->stallReason));
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(sample->correlationId);
  uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
  PRINT("external_id %d\n", external_id);
  ompt_function_id_map_entry_t *entry = ompt_function_id_map_lookup(sample->functionId);
  if (entry != NULL) {
    uint64_t function_index = ompt_function_id_map_entry_function_index_get(entry);
    uint64_t cubin_id = ompt_function_id_map_entry_cubin_id_get(entry);
    ip_normalized_t ip = hpcrun_cubin_id_transform(cubin_id, function_index, sample->pcOffset);
    cct_addr_t frm = { .ip_norm = ip };
    cct_node_t *cct_node = hpcrun_op_id_map_lookup(external_id);
    ompt_host_op_map_entry_t *host_op_entry = ompt_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = ompt_host_op_map_entry_activity_queue_get(host_op_entry);
    if (cct_node != NULL) {
      cct_node_t *cct_child = NULL;
      if ((cct_child = hpcrun_cct_insert_addr(cct_node, &frm)) != NULL) {
        cupti_activity_queue_push(queue, sample, cct_child);
      }
    }
  }
}


void
cupti_process_source_locator
(
 CUpti_ActivitySourceLocator *asl,
 void *state
)
{
  PRINT("Source Locator Id %d, File %s Line %d\n", 
	 asl->id, asl->fileName, 
	 asl->lineNumber);
}


static void
cupti_process_function
(
 CUpti_ActivityFunction *af,
 void *state
)
{
  PRINT("Function Id %u, ctx %u, moduleId %u, functionIndex %u, name %s\n",
	 af->id,
	 af->contextId,
	 af->moduleId,
	 af->functionIndex,
	 af->name);
  ompt_function_id_map_insert(af->id, af->functionIndex, af->moduleId);
}


static void
cupti_process_sampling_record_info
(
 CUpti_ActivityPCSamplingRecordInfo *sri,
 void *state
)
{
  PRINT("corr %u, totalSamples %llu, droppedSamples %llu\n",
	 sri->correlationId,
	 (unsigned long long)sri->totalSamples,
	 (unsigned long long)sri->droppedSamples);
}


static void
cupti_process_correlation
(
 CUpti_ActivityExternalCorrelation *ec,
 void *state
)
{
  uint64_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->externalId;
  if (hpcrun_op_id_map_lookup(external_id) != NULL) {
    if (cupti_correlation_id_map_lookup(correlation_id) != NULL) {
      cupti_correlation_id_map_external_id_replace(correlation_id, external_id);
    } else {
      cupti_correlation_id_map_insert(correlation_id, external_id);
    }
  }
  PRINT("External CorrelationId %lu\n", external_id);
  PRINT("CorrelationId %lu\n", correlation_id);
  PRINT("Activity Kind %u\n", ec->externalKind);
}


static void
cupti_process_memcpy
(
 CUpti_ActivityMemcpy *activity,
 void *state
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    ompt_host_op_map_entry_t *host_op_entry = ompt_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = ompt_host_op_map_entry_activity_queue_get(host_op_entry);
    cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, activity, node);
    }
  }
  PRINT("Memcpy copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy copy kind %u\n", activity->copyKind);
  PRINT("Memcpy copy bytes %u\n", activity->bytes);
}


static void
cupti_process_memcpy2
(
 CUpti_ActivityMemcpy2 *activity, 
 void *state
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    ompt_host_op_map_entry_t *host_op_entry = ompt_host_op_map_lookup(external_id);
    cupti_activity_queue_entry_t **queue = ompt_host_op_map_entry_activity_queue_get(host_op_entry);
    cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, activity, node);
    }
  }
  PRINT("Memcpy2 copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy2 copy kind %u\n", activity->copyKind);
  PRINT("Memcpy2 copy bytes %u\n", activity->bytes);
}


static void
cupti_process_memctr
(
 CUpti_ActivityUnifiedMemoryCounter *activity, 
 void *state
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
cupti_process_activityAPI
(
 CUpti_ActivityAPI *activity,
 void *state
)
{
  PRINT("driver and kernel\n");
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_DRIVER:
    {
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      break;
    }
    default:
      break;
  }
}


static void
cupti_process_runtime
(
 CUpti_ActivityEvent *activity, 
 void *state
)
{
}


static void
cupti_process_activity
(
 CUpti_Activity *activity,
 void *state
)
{
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR:
    cupti_process_source_locator((CUpti_ActivitySourceLocator *) activity, 
				 state);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    cupti_process_sample((CUpti_ActivityPCSampling2 *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    cupti_process_sampling_record_info
      ((CUpti_ActivityPCSamplingRecordInfo *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_FUNCTION:
    cupti_process_function((CUpti_ActivityFunction *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION: 
    cupti_process_correlation((CUpti_ActivityExternalCorrelation *) activity,
			      state);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY: 
    cupti_process_memcpy((CUpti_ActivityMemcpy *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY2: 
    cupti_process_memcpy2((CUpti_ActivityMemcpy2 *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    cupti_process_memctr((CUpti_ActivityUnifiedMemoryCounter *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_DRIVER:
  case CUPTI_ACTIVITY_KIND_KERNEL:
    cupti_process_activityAPI((CUpti_ActivityAPI *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_RUNTIME:
    cupti_process_runtime((CUpti_ActivityEvent *) activity, state);
    break;

  default:
    cupti_process_unknown(activity, state);
    break;
  }
}


void
cupti_activity_handle
(
 CUpti_Activity *activity
)
{
  void *state;
  cupti_process_activity(activity, state);
}


bool
cupti_advance_buffer_cursor
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
