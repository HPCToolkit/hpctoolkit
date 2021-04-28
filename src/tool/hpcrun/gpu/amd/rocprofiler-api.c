// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//******************************************************************************
// local includes
//******************************************************************************

#include "rocprofiler-api.h"
// #include "rocm-debug-api.h"
#include "rocm-binary-processing.h"

#include <roctracer_hip.h>
#include <rocprofiler.h>

#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/libdl.h>

#include <hpcrun/utilities/hpcrun-nanotime.h>

// #include <lib/prof-lean/stdatomic.h>
#include <pthread.h>

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"
//******************************************************************************
// macros
//******************************************************************************


#define PUBLIC_API __attribute__((visibility("default")))

#if 0
Returned API status:
- hsa_status_t - HSA status codes are used from hsa.h header

Loading and Configuring, loadable plugin on-load/unload methods:
- rocprofiler_settings_t – global properties
- OnLoadTool 
- OnLoadToolProp
- OnUnloadTool

Info API:
- rocprofiler_info_kind_t - profiling info kind
- rocprofiler_info_query_t - profiling info query
- rocprofiler_info_data_t - profiling info data
- rocprofiler_get_info - return the info for a given info kind
- rocprofiler_iterote_inf_ - iterate over the info for a given info kind 
- rocprofiler_query_info - iterate over the info for a given info query

Context API:
- rocprofiler_t - profiling context handle
- rocprofiler_feature_kind_t - profiling feature kind
- rocprofiler_feature_parameter_t - profiling feature parameter
- rocprofiler_data_kind_t - profiling data kind
- rocprofiler_data_t - profiling data
- rocprofiler_feature_t - profiling feature
- rocprofiler_mode_t - profiling modes
- rocprofiler_properties_t - profiler properties
- rocprofiler_open - open new profiling context
- rocprofiler_close - close profiling context and release all allocated resources
- rocprofiler_group_count - return profiling groups count
- rocprofiler_get_group - return profiling group for a given index
- rocprofiler_get_metrics - method for calculating the metrics data
- rocprofiler_iterate_trace_data - method for iterating output trace data instances
- rocprofiler_time_id_t - supported time value ID enumeration
- rocprofiler_get_time – return time for a given time ID and profiling timestamp value

Sampling API:
- rocprofiler_start - start profiling
- rocprofiler_stop - stop profiling
- rocprofiler_read - read profiling data to the profiling features objects
- rocprofiler_get_data - wait for profiling data
  Group versions of start/stop/read/get_data methods:
  o rocprofiler_group_start
  o rocprofiler_group_stop
  o rocprofiler_group_read
  o rocprofiler_group_get_data

Intercepting API:
- rocprofiler_callback_t - profiling callback type
- rocprofiler_callback_data_t - profiling callback data type
- rocprofiler_dispatch_record_t – dispatch record
- rocprofiler_queue_callbacks_t – queue callbacks, dispatch/destroy
- rocprofiler_set_queue_callbacks - set queue kernel dispatch and queue destroy callbacks
- rocprofiler_remove_queue_callbacks - remove queue callbacks

Context pool API:
- rocprofiler_pool_t – context pool handle
- rocprofiler_pool_entry_t – context pool entry
- rocprofiler_pool_properties_t – context pool properties
- rocprofiler_pool_handler_t – context pool completion handler
- rocprofiler_pool_open - context pool open
- rocprofiler_pool_close - context pool close
- rocprofiler_pool_fetch – fetch and empty context entry to pool
- rocprofiler_pool_release – release a context entry
- rocprofiler_pool_iterate – iterated fetched context entries
- rocprofiler_pool_flush – flush completed context entries
#endif


#define FORALL_ROCPROFILER_ROUTINES(macro)			\
  macro(rocprofiler_open)   \
  macro(rocprofiler_close)   \
  macro(rocprofiler_get_metrics) \
  macro(rocprofiler_set_queue_callbacks) \
  macro(rocprofiler_start_queue_callbacks) \
  macro(rocprofiler_stop_queue_callbacks) \
  macro(rocprofiler_remove_queue_callbacks) 
  


#define ROCPROFILER_FN_NAME(f) DYN_FN_NAME(f)

#define ROCPROFILER_FN(fn, args) \
  static hsa_status_t (*ROCPROFILER_FN_NAME(fn)) args

#define HPCRUN_ROCPROFILER_CALL(fn, args) \
{      \
  hsa_status_t status = ROCPROFILER_FN_NAME(fn) args;	\
  if (status != HSA_STATUS_SUCCESS) {		\    
    const char* error_string = NULL; \
    rocprofiler_error_string(&error_string); \
    fprintf(stderr, "ERROR: %s\n", error_string); \
    abort(); \    
  }						\
}


typedef const char* (*hip_kernel_name_fnt)(const hipFunction_t f);
typedef const char* (*hip_kernel_name_ref_fnt)(const void* hostFunction, hipStream_t stream);


// Context stored entry type
typedef struct {
  bool valid;
  hsa_agent_t agent;
  rocprofiler_group_t group;
  rocprofiler_callback_data_t data;
}context_entry_t;

// Context callback arg
typedef struct {
  rocprofiler_pool_t** pools;
}callbacks_arg_t;

// Handler callback arg
typedef struct {
  rocprofiler_feature_t* features;
  unsigned feature_count;
}handler_arg_t;



//******************************************************************************
// local variables
//******************************************************************************

static hip_kernel_name_fnt hip_kernel_name_fn;
static hip_kernel_name_ref_fnt hip_kernel_name_ref_fn;
pthread_mutex_t mutex_context;

//----------------------------------------------------------
// rocprofiler function pointers for late binding
//----------------------------------------------------------

ROCPROFILER_FN
(
 rocprofiler_open,
 (
    hsa_agent_t agent,			// GPU handle
    rocprofiler_feature_t* features,	// [in/out] profiling feature array
    uint32_t feature_count,			// profiling feature count
    rocprofiler_t** context,		// [out] profiling context handle
    uint32_t mode,				// profiling mode mask
    rocprofiler_properties_t* properties	// profiler properties
 )
);

ROCPROFILER_FN
(
  rocprofiler_close,
  (
	  rocprofiler_t* context		// [in] profiling context  
  )
);

ROCPROFILER_FN
(
  rocprofiler_get_metrics,
	(
    rocprofiler_t* context		// [in/out] profiling context
  )
);

ROCPROFILER_FN
(
  rocprofiler_set_queue_callbacks,
  (
    rocprofiler_queue_callbacks_t callbacks,           // callbacks
    void* data
  )
);

ROCPROFILER_FN
(
  rocprofiler_start_queue_callbacks,
  (
    void
  )
);

ROCPROFILER_FN
(
  rocprofiler_stop_queue_callbacks,
  (
    void
  )
);

ROCPROFILER_FN
(
  rocprofiler_remove_queue_callbacks,
  (
    void
  )
);


//******************************************************************************
// private operations
//******************************************************************************

static const char *
rocprofiler_path
(
 void
)
{
  const char *path = "librocprofiler64.so";

  return path;
}


unsigned metrics_input(rocprofiler_feature_t** ret) {
  // Profiling feature objects
  const unsigned feature_count = 6;
  rocprofiler_feature_t* features = (rocprofiler_feature_t*) malloc(sizeof(rocprofiler_feature_t) * feature_count);
  memset(features, 0, feature_count * sizeof(rocprofiler_feature_t));

  // PMC events
  features[0].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[0].name = "GRBM_COUNT";
  features[1].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[1].name = "GRBM_GUI_ACTIVE";
  features[2].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[2].name = "GPUBusy";
  features[3].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[3].name = "SQ_WAVES";
  features[4].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[4].name = "SQ_INSTS_VALU";
  features[5].kind = ROCPROFILER_FEATURE_KIND_METRIC;
  features[5].name = "VALUInsts";
//  features[6].kind = ROCPROFILER_FEATURE_KIND_METRIC;
//  features[6].name = "TCC_HIT_sum";
//  features[7].kind = ROCPROFILER_FEATURE_KIND_METRIC;
//  features[7].name = "TCC_MISS_sum";
//  features[8].kind = ROCPROFILER_FEATURE_KIND_METRIC;
//  features[8].name = "WRITE_SIZE";

  *ret = features;
  return feature_count;
}


// Dump stored context entry
static void dump_context_entry(context_entry_t* entry, rocprofiler_feature_t* features, unsigned feature_count) {
  volatile bool valid = entry->valid;
  while (valid == false) sched_yield();

  const char *kernel_name = entry->data.kernel_name;
  const rocprofiler_dispatch_record_t* record = entry->data.record;

  fflush(stdout);
  fprintf(stdout, "kernel symbol(0x%lx) name(\"%s\") tid(%u) queue-id(%u)) ", // gpu-id(%u) ",
    entry->data.kernel_object,
    kernel_name,
    entry->data.thread_id,
    entry->data.queue_id);
    // HsaRsrcFactory::Instance().GetAgentInfo(entry->agent)->dev_index);
  if (record) fprintf(stdout, "time(%lu,%lu,%lu,%lu)",
    record->dispatch,
    record->begin,
    record->end,
    record->complete);
  fprintf(stdout, "\n");
  fflush(stdout);

  rocprofiler_group_t *group = &entry->group;
  if (group->context == NULL) {
    EMSG("error: AMD group->context = NULL");    
  }
  if (feature_count > 0) {
    HPCRUN_ROCPROFILER_CALL(rocprofiler_group_get_data, (group));  
    HPCRUN_ROCPROFILER_CALL(rocprofiler_get_metrics, (group->context));    
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    const rocprofiler_feature_t* p = &features[i];
    fprintf(stdout, ">  %s ", p->name);
    switch (p->data.kind) {
      // Output metrics results
      case ROCPROFILER_DATA_KIND_INT64:
        fprintf(stdout, "= (%lu)\n", p->data.result_int64);
        break;
      default:
        fprintf(stderr, "Undefined data kind(%u)\n", p->data.kind);
        abort();
    }
  }
}


// Profiling completion handler
// Dump and delete the context entry
// Return true if the context was dumped successfully
static bool context_handler1(rocprofiler_group_t group, void* arg) {
  context_entry_t* ctx_entry = (context_entry_t*)arg;

  if (pthread_mutex_lock(&mutex_context) != 0) {
    perror("pthread_mutex_lock");
    abort();
  }

  rocprofiler_feature_t* features = ctx_entry->group.features[0];
  unsigned feature_count = ctx_entry->group.feature_count;
  dump_context_entry(ctx_entry, features, feature_count);

  if (pthread_mutex_unlock(&mutex_context) != 0) {
    perror("pthread_mutex_unlock");
    abort();
  }

  return false;
}


// Kernel disoatch callback
hsa_status_t dispatch_callback(const rocprofiler_callback_data_t* callback_data, void* arg,
                               rocprofiler_group_t* group) {

  printf("Rocprofiler dispatch_callback\n\n");
  // Passed tool data
  hsa_agent_t agent = callback_data->agent;
  // HSA status
  hsa_status_t status = HSA_STATUS_ERROR;

  // Open profiling context
  // context properties
  context_entry_t* entry = (context_entry_t*) malloc(sizeof(context_entry_t));
  rocprofiler_t* context = NULL;
  rocprofiler_properties_t properties = {};
  properties.handler = context_handler1;
  properties.handler_arg = (void*)entry;

  rocprofiler_feature_t *features;
  unsigned feature_count = metrics_input(&features);

  HPCRUN_ROCPROFILER_CALL(rocprofiler_open, (agent, features, feature_count,
                            &context, 0 /*ROCPROFILER_MODE_SINGLEGROUP*/, &properties));
  

  // Get group[0]
  HPCRUN_ROCPROFILER_CALL(rocprofiler_get_group, (context, 0, group));
  

  // Fill profiling context entry
  entry->agent = agent;
  entry->group = *group;
  entry->data = *callback_data;
  entry->data.kernel_name = strdup(callback_data->kernel_name);
  entry->valid = true;

  return HSA_STATUS_SUCCESS;
}


static void initialize() {
  // Getting profiling features
  rocprofiler_feature_t* features = NULL;
  unsigned feature_count = metrics_input(&features);

  // Handler arg
  handler_arg_t* handler_arg = (handler_arg_t*) malloc(sizeof(handler_arg_t));
  handler_arg->features = features;
  handler_arg->feature_count = feature_count;

  rocprofiler_queue_callbacks_t callbacks_ptrs = {};
  callbacks_ptrs.dispatch = dispatch_callback;
  rocprofiler_set_queue_callbacks(callbacks_ptrs, NULL);

  // Initialize recursive mutex_context
  pthread_mutexattr_t Attr;
  pthread_mutexattr_init(&Attr);
  pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex_context, &Attr);

}

// This is necessary for rocprofiler callback to work
extern PUBLIC_API void OnLoadToolProp(rocprofiler_settings_t* settings){
  printf("Rocprofiler OnLoadToolProp______________________\n");  
  initialize();
}


static void cleanup() {
  // Unregister dispatch callback
  rocprofiler_remove_queue_callbacks();
}



//******************************************************************************
// interface operations
//******************************************************************************


void rocprofiler_start_kernel(){
  HPCRUN_ROCPROFILER_CALL(rocprofiler_start_queue_callbacks, ());
}


void rocprofiler_stop_kernel(){
  HPCRUN_ROCPROFILER_CALL(rocprofiler_stop_queue_callbacks, ());
}


void
rocprofiler_init
(
 void
)
{
  printf("Rocprofiler INIT\n");  
  // initialize();
  return; 
}


void
rocprofiler_fini
(
 void *args,
 int how
)
{
  printf("Rocprofiler FINI\n");  
  cleanup();
  return;
}



int
rocprofiler_bind
(
 void
)
{
//   // This is a workaround for roctracer to not hang when taking timer interrupts
//   // More details: https://github.com/ROCm-Developer-Tools/roctracer/issues/22
//   setenv("HSA_ENABLE_INTERRUPT", "0", 1);

  // if (rocm_debug_api_bind() != DYNAMIC_BINDING_STATUS_OK) {
  //   return DYNAMIC_BINDING_STATUS_ERROR;
  // }

#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(rocprofiler, rocprofiler_path(), RTLD_NOW | RTLD_GLOBAL);
  // Somehow roctracter needs libkfdwrapper64.so, but does not really load it.
  // So, we load it before using any function in roctracter.
  CHK_DLOPEN(kfd, "libkfdwrapper64.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLOPEN(hip, "libamdhip64.so", RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCPROFILER_BIND(fn) \
  CHK_DLSYM(rocprofiler, fn);

  FORALL_ROCPROFILER_ROUTINES(ROCPROFILER_BIND);

#undef ROCPROFILER_BIND

  dlerror();
  hip_kernel_name_fn = (hip_kernel_name_fnt) dlsym(hip, "hipKernelNameRef");
  if (hip_kernel_name_fn == 0) {
    return DYNAMIC_BINDING_STATUS_ERROR;
  }

  dlerror();
  hip_kernel_name_ref_fn = (hip_kernel_name_ref_fnt) dlsym(hip, "hipKernelNameRefByPtr");
  if (hip_kernel_name_ref_fn == 0) {
    return DYNAMIC_BINDING_STATUS_ERROR;
  }

  return DYNAMIC_BINDING_STATUS_OK;
#else
  return DYNAMIC_BINDING_STATUS_ERROR;
#endif // ! HPCRUN_STATIC_LINK
}

