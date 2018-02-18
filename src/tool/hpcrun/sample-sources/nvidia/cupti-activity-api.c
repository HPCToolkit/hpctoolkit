#include <stdio.h>

#include <hpcrun/cct2metrics.h>
#include "nvidia.h"
#include "cubin-id-map.h"
#include "cupti-activity-api.h"
#include "cupti-activity-strings.h"
#include "cupti-correlation-id-map.h"
#include "cupti-activity-queue.h"
#include "cupti-function-id-map.h"
#include "cupti-host-op-map.h"
#include "cupti-context-map.h"

//******************************************************************************
// macros
//******************************************************************************

#define HPCRUN_OMPT_ENABLE 1

#if HPCRUN_OMPT_ENABLE
#include <hpcrun/ompt/ompt-interface.h>
#endif

#define HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE (64 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT (8)
#define HPCRUN_CUPTI_CALL(fn, args) \
{      \
    CUptiResult status = fn args; \
    if (status != CUPTI_SUCCESS) { \
      cupti_error_report(status, #fn); \
    }\
}

#define DISPATCH_CALLBACK(fn, args) if (fn) fn args

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

static __thread uint64_t cupti_correlation_id = 1;

//-------------------------------------------------------------
// general functions
//-------------------------------------------------------------

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
 void *record,
 void *state
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
#if HPCRUN_OMPT_ENABLE
      cct_node_t *cct_node = hpcrun_ompt_op_id_map_lookup(external_id);
#endif
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
  cupti_function_id_map_insert(af->id, af->functionIndex, af->moduleId);
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
cupti_process_memcpy
(
 CUpti_ActivityMemcpy *activity,
 void *state
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
cupti_process_memcpy2
(
 CUpti_ActivityMemcpy2 *activity, 
 void *state
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
cupti_process_kernel
(
 CUpti_ActivityKernel4 *activity, 
 void *state
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
    cupti_process_sample(activity, state);
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
  case CUPTI_ACTIVITY_KIND_RUNTIME:
    cupti_process_activityAPI((CUpti_ActivityAPI *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_KERNEL:
    cupti_process_kernel((CUpti_ActivityKernel4 *) activity, state);
    break;

  case CUPTI_ACTIVITY_KIND_EVENT:
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


void 
cupti_buffer_completion_callback
(
 CUcontext ctx,
 uint32_t streamId,
 uint8_t *buffer,
 size_t size,
 size_t validSize
)
{
  // signal advance to return pointer to first record
  CUptiResult status;
  CUpti_Activity *record = NULL;

  if (validSize > 0) {
    do {
      status = cuptiActivityGetNextRecord(buffer, validSize, &record);
      if (status == CUPTI_SUCCESS) {
        cupti_activity_handle(record);
      }   
      else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
        break;
      else {
        exit(-1);
      }   
    } while (true);

    // report any records dropped from the queue
    size_t dropped;
    cupti_get_num_dropped_records(ctx, streamId, &dropped);
    if (dropped != 0) {
      printf("Dropped %u activity records\n", (unsigned int) dropped);
    }   
  }
}



bool
cupti_activity_flush
(
)
{
  bool result = (cuptiActivityFlushAll(CUPTI_ACTIVITY_FLAG_FLUSH_FORCED) == CUPTI_SUCCESS);
  return result;
}

//******************************************************************************
// cupti functions for cuda
//******************************************************************************

//******************************************************************************
// types
//******************************************************************************


typedef void (*cupti_error_callback_t) 
(
 const char *type, 
 const char *fn, 
 const char *error_string
);


typedef void (*cupti_dropped_callback_t) 
(
 size_t dropped
);


typedef CUptiResult (*cupti_activity_enable_t)
(
 CUpti_ActivityKind activity
);


typedef CUptiResult (*cupti_activity_enable_disable_t) 
(
 CUcontext context,
 CUpti_ActivityKind activity
);


typedef struct {
  CUpti_BuffersCallbackRequestFunc buffer_request; 
  CUpti_BuffersCallbackCompleteFunc buffer_complete;
} cupti_activity_buffer_state_t;



//******************************************************************************
// forward declarations 
//******************************************************************************

static void
cupti_error_callback_dummy
(
 const char *type, 
 const char *fn, 
 const char *error_string
);


static void 
cupti_correlation_callback_dummy
(
 uint64_t *id
);





//******************************************************************************
// static data
//******************************************************************************
//
static bool cupti_enabled_correlation = false;

static cupti_correlation_callback_t cupti_correlation_callback = 
  cupti_correlation_callback_dummy;

static cupti_error_callback_t cupti_error_callback = 
  cupti_error_callback_dummy;

static cupti_activity_buffer_state_t cupti_activity_enabled = { 0, 0 };
static cupti_activity_buffer_state_t cupti_activity_disabled = { 0, 0 };

static cupti_activity_buffer_state_t *cupti_activity_state = 
  &cupti_activity_disabled;

static cupti_load_callback_t cupti_load_callback = 0;

static cupti_load_callback_t cupti_unload_callback = 0;

static CUpti_SubscriberHandle cupti_subscriber;


//******************************************************************************
// private operations
//******************************************************************************

static void
cupti_error_callback_dummy // __attribute__((unused))
(
 const char *type, 
 const char *fn, 
 const char *error_string
)
{
  PRINT("%s: function %s failed with error %s\n", type, fn, error_string);
  exit(-1);
} 


static void
cupti_error_report
(
 CUptiResult error, 
 const char *fn
)
{
  const char *error_string;
  cuptiGetResultString(error, &error_string);
  cupti_error_callback("CUPTI result error", fn, error_string);
} 


//******************************************************************************
// internal functions
//******************************************************************************

static void
cupti_subscriber_callback
(
 void *userdata,
 CUpti_CallbackDomain domain,
 CUpti_CallbackId cb_id,
 const CUpti_CallbackData *cb_info
)
{
  PRINT("enter cupti_subscriber_callback\n");

  if (domain == CUPTI_CB_DOMAIN_RESOURCE) {
    const CUpti_ResourceData *rd = (const CUpti_ResourceData *) cb_info;
    if (cb_id == CUPTI_CBID_RESOURCE_MODULE_LOADED) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *) rd->resourceDescriptor;
      PRINT("loaded module id %d, cubin size %ld, cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      //DISPATCH_CALLBACK(cupti_load_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    }
    if (cb_id == CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *) rd->resourceDescriptor;
      PRINT("unloaded module id %d, cubin size %ld, cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      //DISPATCH_CALLBACK(cupti_unload_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    }
  } else if (domain == CUPTI_CB_DOMAIN_DRIVER_API) {
    if ((cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH) ||                
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer_ptds) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuLaunch) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid) ||
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync) || 
      (cb_id == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel)) {

      if (cb_info->callbackSite == CUPTI_API_ENTER) {
        uint64_t correlation_id = __sync_fetch_and_add(&cupti_correlation_id, 1);
        HPCRUN_CUPTI_CALL(cuptiActivityPushExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, correlation_id));
        PRINT("Driver push externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      }
      if (cb_info->callbackSite == CUPTI_API_EXIT) {
        uint64_t correlation_id;
        HPCRUN_CUPTI_CALL(cuptiActivityPopExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, &correlation_id));
        PRINT("Driver pop externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      }
    }
  } else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API) { 
    switch (cb_id) {
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_ptsz_v7000:
      #if CUPTI_API_VERSION >= 10
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_ptsz_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernelMultiDevice_v9000:  
      #endif
      {
        if (cb_info->callbackSite == CUPTI_API_ENTER) {
          uint64_t correlation_id = __sync_fetch_and_add(&cupti_correlation_id, 1);
          PRINT("Runtime push externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
          HPCRUN_CUPTI_CALL(cuptiActivityPushExternalCorrelationId, (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, correlation_id));
        }
        if (cb_info->callbackSite == CUPTI_API_EXIT) {
          uint64_t correlation_id;
          HPCRUN_CUPTI_CALL(cuptiActivityPopExternalCorrelationId, (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, &correlation_id));
          PRINT("Runtime pop externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
        }
        break;
      }
      default:
        break;
    }
  }

  PRINT("exit cupti_subscriber_callback\n");
}


static void 
cupti_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t *id
)
{
  *id = 0;
}



//******************************************************************************
// interface  operations
//******************************************************************************


void
cupti_device_get_timestamp
(
 CUcontext context,
 uint64_t *time
)
{
  HPCRUN_CUPTI_CALL(cuptiDeviceGetTimestamp, (context, time));
}


void 
cupti_buffer_alloc 
(
 uint8_t **buffer, 
 size_t *buffer_size, 
 size_t *maxNumRecords
)
{
  int retval = posix_memalign((void **) buffer,
    (size_t) HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT,
    (size_t) HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE); 
  
  if (retval != 0) {
    cupti_error_callback("CUPTI", "cupti_buffer_alloc", "out of memory");
  }
  
  *buffer_size = HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE;

  *maxNumRecords = 0;
}

//-------------------------------------------------------------
// event specification
//-------------------------------------------------------------

cupti_set_status_t
cupti_set_monitoring
(
 const CUpti_ActivityKind activity_kinds[],
 bool enable
)
{
  PRINT("enter cupti_set_monitoring\n");
  int failed = 0;
  int succeeded = 0;
  cupti_activity_enable_t action =
    (enable ? cuptiActivityEnable: cuptiActivityDisable);
  int i = 0;
  for (;;) {
    CUpti_ActivityKind activity_kind = activity_kinds[i++];
    if (activity_kind == CUPTI_ACTIVITY_KIND_INVALID) break;
    bool succ = action(activity_kind) == CUPTI_SUCCESS;
    if (succ) {
      if (enable) {
        PRINT("activity %d enabled\n", activity_kind);
      } else {
        PRINT("activity %d disabled\n", activity_kind);
      }
      succeeded++;
    }
    else failed++;
  }
  if (succeeded > 0) {
    if (failed == 0) return cupti_set_all;
    else return cupti_set_some;
  }
  PRINT("leave cupti_set_monitoring\n");
  return cupti_set_none;
}


//-------------------------------------------------------------
// tracing control 
//-------------------------------------------------------------

void 
cupti_trace_init
(
)
{
  cupti_activity_enabled.buffer_request = cupti_buffer_alloc;
  cupti_activity_enabled.buffer_complete = cupti_buffer_completion_callback;
}


void
cupti_trace_flush
(
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityFlushAll, (CUPTI_ACTIVITY_FLAG_FLUSH_FORCED));
}


void 
cupti_trace_start
(
 CUcontext context
)
{
  *cupti_activity_state = cupti_activity_enabled;
  HPCRUN_CUPTI_CALL(cuptiActivityRegisterCallbacks,
    (cupti_activity_state->buffer_request, cupti_activity_state->buffer_complete));
}


void 
cupti_trace_pause
(
 CUcontext context,
 bool begin_pause
)
{
  cupti_activity_enable_disable_t action =
    (begin_pause ? cuptiActivityDisableContext : cuptiActivityEnableContext);
  size_t activity = 0;
  size_t activity_num = cupti_context_map_activity_num();
  cupti_context_map_entry_t *entry = cupti_context_map_lookup(context);
  if (entry == NULL) {
    return;
  }
  for (activity = 0; activity < activity_num; ++activity) {
    if (cupti_context_map_entry_activity_get(entry, activity)) {
      bool status = cupti_context_map_entry_activity_status_get(entry, activity);
      if (begin_pause == status) {
        bool activity_succ = action(context, activity) == CUPTI_SUCCESS;
        if (activity_succ) {
          if (begin_pause == true) {
            cupti_context_map_disable(context, activity);
          } else {
            cupti_context_map_enable(context, activity);
          }
        }
      }
    }
  }
}


void 
cupti_trace_finalize
(
)
{
  HPCRUN_CUPTI_CALL(cuptiFinalize, ());
}


//-------------------------------------------------------------
// correlation callback control 
//-------------------------------------------------------------

void
cupti_subscribe_callbacks
(
)
{
  HPCRUN_CUPTI_CALL(cuptiSubscribe, (&cupti_subscriber,
    (CUpti_CallbackFunc) cupti_subscriber_callback,
    (void *) NULL));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_DRIVER_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_unsubscribe_callbacks
(
)
{
  HPCRUN_CUPTI_CALL(cuptiUnsubscribe, (cupti_subscriber));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (0, cupti_subscriber, CUPTI_CB_DOMAIN_DRIVER_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (0, cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (0, cupti_subscriber, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_correlation_enable
(
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityEnable, (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));
}


void
cupti_correlation_disable
(
 CUcontext context
)
{
  if (cupti_enabled_correlation) {
    HPCRUN_CUPTI_CALL(cuptiActivityDisable, (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));
    cupti_enabled_correlation = false;
    PRINT("stop correlation\n");
  }
}


//-------------------------------------------------------------
// cursor support
//-------------------------------------------------------------
  
bool
cupti_buffer_cursor_advance
(
 uint8_t *buffer,
 size_t size,
 CUpti_Activity **activity
)
{
  return cuptiActivityGetNextRecord(buffer, size, activity) == CUPTI_SUCCESS;
}


bool
cupti_buffer_cursor_isvalid
(
 uint8_t *buffer,
 size_t size,
 CUpti_Activity *activity
)
{
  CUpti_Activity *cursor = activity;
  return cupti_buffer_cursor_advance(buffer, size, &cursor);
}


void
cupti_get_num_dropped_records
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped 
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityGetNumDroppedRecords, (context, streamId, dropped));
}


void
cupti_pc_sampling_config
(
  CUcontext context,
  CUpti_ActivityPCSamplingPeriod period
)
{
  CUpti_ActivityPCSamplingConfig config;
  config.samplingPeriod = period;
  HPCRUN_CUPTI_CALL(cuptiActivityConfigurePCSampling, (context, &config));
}
