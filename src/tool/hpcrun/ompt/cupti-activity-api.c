#include <stdio.h>

#include <hpcrun/cct2metrics.h>

#include "cupti-activity-api.h"
#include "cupti-activity-strings.h"
#include "ompt-function-id-map.h"
#include "ompt-interface.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

static __thread int external_id = -1;

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
	 "samples %u, stallreason %s\n",
	 sample->sourceLocatorId,
	 sample->functionId,
	 sample->pcOffset,
	 sample->correlationId,
	 sample->samples,
	 cupti_stall_reason_string(sample->stallReason));
  if (external_id != -1) {
    ompt_function_id_map_entry_t *entry = ompt_function_id_map_lookup(sample->functionId);
    if (entry != NULL) {
      uint64_t function_index = ompt_function_id_map_entry_function_index_get(entry);
      uint64_t cubin_id = ompt_function_id_map_entry_cubin_id_get(entry);
      ip_normalized_t ip = hpcrun_cubin_id_transform(cubin_id, function_index, sample->pcOffset);
      cct_addr_t frm = { .ip_norm = ip };
      cct_node_t *cct_node = hpcrun_op_id_map_lookup(external_id);
      if (cct_node != NULL) {
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(cct_node, &frm)) != NULL) {
          metric_set_t* metrics = hpcrun_reify_metric_set(cct_child);
          hpcrun_metric_std_inc(0, metrics, (cct_metric_data_t){.i = 1});
        }
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
  external_id = ec->externalId;
  PRINT("External CorrelationId %lu\n", ec->externalId);
  PRINT("Activity Kind %u\n", ec->kind);
}


static void
cupti_process_memcpy
(
 CUpti_ActivityMemcpy *activity,
 void *state
)
{
}


static void
cupti_process_memcpy2
(
 CUpti_ActivityMemcpy2 *activity, 
 void *state
)
{
  if (external_id != -1) {
    cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
    external_id = -1;
  }
}


static void
cupti_process_memctr
(
 CUpti_ActivityUnifiedMemoryCounter *activity, 
 void *state
)
{
}


static void
cupti_process_activityAPI
(
 CUpti_ActivityAPI *activity,
 void *state
)
{
  PRINT("driver and kernel");
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_DRIVER:
    {
      if (external_id != -1) {
        cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
        external_id = -1;
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      if (external_id != -1) {
        cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
        external_id = -1;
      }
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
