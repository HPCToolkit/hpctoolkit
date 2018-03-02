#include <stdio.h>
#include <errno.h>   // errno
#include <fcntl.h>   // open
#include <limits.h>  // PATH_MAX
#include <stdio.h>   // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

#include <hpcrun/cct2metrics.h>
#include <lib/prof-lean/spinlock.h>
#include "nvidia.h"
#include "cubin-id-map.h"
#include "cubin-md5-map.h"
#include "cupti-activity-api.h"
#include "cupti-activity-strings.h"
#include "cupti-correlation-id-map.h"
#include "cupti-activity-queue.h"
#include "cupti-function-id-map.h"
#include "cupti-host-op-map.h"

//******************************************************************************
// macros
//******************************************************************************

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

static uint64_t cupti_correlation_id = 1;

static spinlock_t files_lock = SPINLOCK_UNLOCKED;


//******************************************************************************
// types
//******************************************************************************


typedef void (*cupti_error_callback_t) 
(
 const char *type, 
 const char *fn, 
 const char *error_string
);


typedef CUptiResult (*cupti_activity_enable_t)
(
 CUpti_ActivityKind activity
);


typedef void (*cupti_correlation_callback_t)
(
 uint64_t *id 
);


typedef void (*cupti_load_callback_t)
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
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
static bool cupti_correlation_enabled = false;

static cupti_correlation_callback_t cupti_correlation_callback = 
  cupti_correlation_callback_dummy;

static cupti_error_callback_t cupti_error_callback = 
  cupti_error_callback_dummy;

static cupti_activity_buffer_state_t cupti_activity_enabled = { 0, 0 };

static cupti_load_callback_t cupti_load_callback = 0;

static cupti_load_callback_t cupti_unload_callback = 0;

static CUpti_SubscriberHandle cupti_subscriber;


//******************************************************************************
// private operations
//******************************************************************************

static void 
cupti_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t *id
)
{
  *id = 0;
}


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

static bool
cupti_write_cubin
(
 const char *file_name,
 const void *cubin,
 size_t cubin_size
)
{
  int fd;
  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd >= 0) {
    // success
    // another process already wrote [vdso]
    if (errno == EEXIST) {
      close(fd);
      return true;
    }
    if (write(fd, cubin, cubin_size) != cubin_size) {
      close(fd);
      return false;   
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}


#define FILE_NAME_LEN 256

static void
cupti_load_callback_cuda
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute md5 for cubin and store it into a map
  cubin_md5_map_entry_t *entry = cubin_md5_map_lookup(module_id);
  unsigned char *md5;
  if (entry == NULL) {
    cubin_md5_map_insert(module_id, cubin, cubin_size);
  }
  md5 = cubin_md5_map_entry_md5_get(entry);

  // Write a file if does not exist
  bool file_flag;
  spinlock_lock(&files_lock);
  if (file_flag == false) {
    char file_name[PATH_MAX];
    mkdir("cubins", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
    sprintf(file_name, "cubins/%s", md5);
    file_flag = cupti_write_cubin(file_name, cubin, cubin_size);
  }
  spinlock_unlock(&files_lock);

  if (file_flag) {
    char device_file[PATH_MAX]; 
    sprintf(device_file, "%s@0x%lx", md5, (unsigned long)NULL);
    uint64_t hpctoolkit_module_id = hpcrun_loadModule_add(device_file);
    cubin_id_map_entry_t *entry = cubin_id_map_lookup(module_id);
    if (entry == NULL) {
      Elf_SymbolVector *vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_id_map_insert(module_id, hpctoolkit_module_id, vector);
    }
  }
}


static void
cupti_unload_callback_cuda
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  cubin_id_map_refcnt_update(module_id, 0);
}


static void
cupti_correlation_callback_cuda
(
 uint64_t *id 
)
{
  // TODO(keren): include atomic.h
  *id = __sync_fetch_and_add(&cupti_correlation_id, 1);
  kind_info_t *cupti_host_op_kind = hpcrun_metrics_new_kind();
  int cupti_host_op_metric_id = hpcrun_set_new_metric_info(cupti_host_op_kind, "CUPTI_HOST_OP_KIND"); 
  hpcrun_close_kind(cupti_host_op_kind);
  
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  ucontext_t uc;
  getcontext(&uc);
  thread_data_t *td = hpcrun_get_thread_data();
  // NOTE(keren): hpcrun_safe_enter prevent self interruption
  td->overhead++;
  hpcrun_safe_enter();
  cct_node_t *node = hpcrun_sample_callpath(&uc, cupti_host_op_metric_id, zero_metric_incr, 0, 1, NULL).sample_node; 
  hpcrun_safe_exit();
  td->overhead--;
  cupti_host_op_map_insert(*id, 0, NULL, node);
}


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
      DISPATCH_CALLBACK(cupti_load_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    }
    if (cb_id == CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *) rd->resourceDescriptor;
      PRINT("unloaded module id %d, cubin size %ld, cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      DISPATCH_CALLBACK(cupti_unload_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
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
        uint64_t correlation_id;
        cupti_correlation_callback_cuda(&correlation_id);
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
          uint64_t correlation_id;
          cupti_correlation_callback_cuda(&correlation_id);
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

//******************************************************************************
// interface  operations
//******************************************************************************

void
cupti_device_timestamp_get
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
        cupti_activity_process(record);
      }   
      else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
        break;
      else {
        exit(-1);
      }   
    } while (true);

    // report any records dropped from the queue
    size_t dropped;
    cupti_num_dropped_records_get(ctx, streamId, &dropped);
    if (dropped != 0) {
      printf("Dropped %u activity records\n", (unsigned int) dropped);
    }   
  }
}

//-------------------------------------------------------------
// event specification
//-------------------------------------------------------------

cupti_set_status_t
cupti_monitoring_set
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
  HPCRUN_CUPTI_CALL(cuptiActivityRegisterCallbacks,
    (cupti_activity_enabled.buffer_request, cupti_activity_enabled.buffer_complete));
}


void 
cupti_trace_finalize
(
)
{
  HPCRUN_CUPTI_CALL(cuptiFinalize, ());
}


void
cupti_num_dropped_records_get
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped 
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityGetNumDroppedRecords, (context, streamId, dropped));
}


//-------------------------------------------------------------
// correlation callback control 
//-------------------------------------------------------------

void
cupti_callbacks_subscribe
(
)
{
  cupti_load_callback = cupti_load_callback_cuda;
  cupti_unload_callback = cupti_unload_callback_cuda;
  cupti_correlation_callback = cupti_correlation_callback_cuda;
  HPCRUN_CUPTI_CALL(cuptiSubscribe, (&cupti_subscriber,
    (CUpti_CallbackFunc) cupti_subscriber_callback,
    (void *) NULL));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_DRIVER_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
  HPCRUN_CUPTI_CALL(cuptiEnableDomain, (1, cupti_subscriber, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_callbacks_unsubscribe
(
)
{
  cupti_load_callback = 0;
  cupti_unload_callback = 0;
  cupti_correlation_callback = 0;
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
  cupti_correlation_enabled = true;
  HPCRUN_CUPTI_CALL(cuptiActivityEnable, (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));
}


void
cupti_correlation_disable
(
 CUcontext context
)
{
  if (cupti_correlation_enabled) {
    HPCRUN_CUPTI_CALL(cuptiActivityDisable, (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));
    cupti_correlation_enabled = false;
  }
}


//******************************************************************************
// pc sampling configs
//******************************************************************************

void
cupti_pc_sampling_config
(
  CUcontext context,
  CUpti_ActivityPCSamplingPeriod period
)
{
  CUpti_ActivityPCSamplingConfig config;
  config.samplingPeriod = period;
  config.size = 0;
  config.samplingPeriod2 = 0;
  HPCRUN_CUPTI_CALL(cuptiActivityConfigurePCSampling, (context, &config));
}


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
      cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
      cupti_activity_queue_entry_t **queue = cupti_host_op_map_entry_activity_queue_get(host_op_entry);
      cct_node_t *host_op_node = cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      if (host_op_node != NULL) {
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
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
  if (cupti_host_op_map_lookup(external_id) != NULL) {
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
    cct_node_t *node = cupti_host_op_map_entry_host_op_node_get(external_id);
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
    cct_node_t *node = cupti_host_op_map_entry_host_op_node_get(external_id);
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
    cct_node_t *node = cupti_host_op_map_entry_host_op_node_get(external_id);
    if (node != NULL) {
      cupti_activity_queue_push(queue, CUPTI_ACTIVITY_KIND_KERNEL, (void *)activity, node);
    }
  }
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


void
cupti_activity_flush
(
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityFlushAll, (CUPTI_ACTIVITY_FLAG_FLUSH_FORCED));
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

  case CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR:
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
