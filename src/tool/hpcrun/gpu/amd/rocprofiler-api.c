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
// Copyright ((c)) 2002-2023, Rice University
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

#include <rocprofiler/rocprofiler.h>
#include <rocprofiler/activity.h>

#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/thread_data.h>
#include <hpcrun/sample-sources/libdl.h>

#include <hpcrun/utilities/hpcrun-nanotime.h>

#include <lib/prof-lean/spinlock.h>
#include <pthread.h>

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"
//******************************************************************************
// macros
//******************************************************************************


#define PUBLIC_API __attribute__((visibility("default")))

#define FORALL_ROCPROFILER_ROUTINES(macro)			\
  macro(rocprofiler_open)   \
  macro(rocprofiler_close)   \
  macro(rocprofiler_get_metrics) \
  macro(rocprofiler_set_queue_callbacks) \
  macro(rocprofiler_start_queue_callbacks) \
  macro(rocprofiler_stop_queue_callbacks) \
  macro(rocprofiler_remove_queue_callbacks) \
  macro(rocprofiler_iterate_info) \
  macro(rocprofiler_group_get_data) \
  macro(rocprofiler_get_group)



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

// Vladimir: TODO: We might not need this data structure anymore?
typedef struct {
  bool valid;
  hsa_agent_t agent;
  rocprofiler_group_t group;
  rocprofiler_callback_data_t data;
} hpcrun_amd_counter_data_t;


// Dispatch callback data type, that store information about requested counters.
typedef struct callbacks_data_s {
  rocprofiler_feature_t* features;
  unsigned feature_count;
  uint32_t *set;         // std::vector<uint32_t>* set;
  unsigned group_index;
  FILE* file_handle;
  int filter_on;
  uint32_t *gpu_index;   // std::vector<uint32_t>* gpu_index;
  char **kernel_string;  // std::vector<std::string>* kernel_string;
  uint32_t *range;       // std::vector<uint32_t>* range;
} callbacks_data_t;


// The following data structures are used within the context_entry_t
typedef struct kernel_properties_s {
  uint32_t grid_size;
  uint32_t workgroup_size;
  uint32_t lds_size;
  uint32_t scratch_size;
  uint32_t arch_vgpr_count;
  uint32_t accum_vgpr_count;
  uint32_t sgpr_count;
  uint32_t wave_size;
  hsa_signal_t signal;
  uint64_t object;
} kernel_properties_t;


#if 0
// Vladimir: C++ism of the official rocprofiler example

typedef struct symbols_map_data_s {
    const char* name;
    uint64_t refs_count;
} symbols_map_data_t;

typedef std::map<uint64_t, symbols_map_data_t> symbols_map_t;

typedef symbols_map_t::iterator symbols_map_it_t;

#else

// Vladimir: Since the size of the `symbols_map_t::iterator` is 8-bytes,
// I added this workaround.
typedef void * symbols_map_it_t;


#endif

// Data structure that contains the information about all requested counters,
// both basic and derived.
typedef struct context_entry_s {
  bool valid;
  bool active;
  uint32_t index;
  hsa_agent_t agent;
  rocprofiler_group_t group;
  rocprofiler_feature_t* features;
  unsigned feature_count;
  rocprofiler_callback_data_t data;
  kernel_properties_t kernel_properties;
  symbols_map_it_t kernel_name_it;
  FILE* file_handle;
} context_entry_t;


//******************************************************************************
// local variables
//******************************************************************************

// Currently we serialize kernel execution when collecting counters.
// So we have one global correlation id, counter data storage,
// and one variable indicating whether the processing is finished or not
static hpcrun_amd_counter_data_t counter_data;
static uint64_t rocprofiler_correlation_id;
static volatile int context_callback_finish;

static bool rocprofiler_initialized = false;

// total number of counters supported by rocprofiler,
// an array of their string names, and an array of their description
static int total_counters = 0;
static const char** counter_name = NULL;
static const char** counter_description = NULL;

// the list of counters specified at the command line
static int *is_specified_by_user = NULL;
static int total_requested = 0;
static rocprofiler_feature_t* rocprofiler_input = NULL;
static const char** requested_counter_name = NULL;
static const char** requested_counter_description = NULL;

// A spin lock to serialize GPU kernels
static spinlock_t kernel_lock;

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

ROCPROFILER_FN
(
  rocprofiler_iterate_info,
  (
    const hsa_agent_t* agent,			// [in] GPU handle, NULL for all
                                  // GPU agents
    rocprofiler_info_kind_t kind,			// kind of iterated info
    hsa_status_t (*callback)(const rocprofiler_info_data_t info, void *data), // callback
    void *data
  )
);

ROCPROFILER_FN
(
  rocprofiler_group_get_data,
  (
    rocprofiler_group_t* group // [in/out] profiling group
  )
);

ROCPROFILER_FN
(
  rocprofiler_get_group,
  (
    rocprofiler_t* context,			  // [in/out] profiling context,
                                  //  will be returned as
                                  //  a part of the group structure
    uint32_t index,				        // [in] group index
    rocprofiler_group_t* group		// [out] profiling group
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

// Depending on the `kind`, access to the corresponding union field
// to extract the value of the counter.  Cast the value to the
// gpu_counter_value_t required by the gpu_activity.
// Are we losing any data by doing the cast?
// How about adding the explicit inline attribute?
static gpu_counter_value_t
decode_counter_value
(
  const rocprofiler_data_t* cnt_data
)
{
  switch (cnt_data->kind)
  {
  case ROCPROFILER_DATA_KIND_UNINIT:
    return (gpu_counter_value_t) 0;
  case ROCPROFILER_DATA_KIND_INT32:
    return (gpu_counter_value_t) cnt_data->result_int32;
  case ROCPROFILER_DATA_KIND_INT64:
    return (gpu_counter_value_t) cnt_data->result_int64;
  case ROCPROFILER_DATA_KIND_FLOAT:
    return (gpu_counter_value_t) cnt_data->result_float;
  case ROCPROFILER_DATA_KIND_DOUBLE:
    return (gpu_counter_value_t) cnt_data->result_double;
  default:
      // The `rocprofiler_data_kind_t` hold 6 possible values, but the union
      // present within the `rocprofiler_data_t` has 5 fields of different
      // type. It seems that ROCPROFILER_DATA_KIND_UNINIT (unsigned_int)
      // is missing from the union, so I am unsure what to do about it.
      // Furthermore, I do not know how to parse ROCPROFILER_DATA_KIND_BYTES,
      // In both cases, I decided to crash the program to see how frequent this is.
    assert(false);
    return (gpu_counter_value_t) 0;
  }
}

static void
translate_rocprofiler_output
(
  gpu_activity_t* ga,
  context_entry_t* entry
)
{
  rocprofiler_group_t *group = &entry->group;
  if (group->context != NULL && entry->feature_count > 0) {
    HPCRUN_ROCPROFILER_CALL(rocprofiler_group_get_data, (group));
    HPCRUN_ROCPROFILER_CALL(rocprofiler_get_metrics, (group->context));
  }

  // Translate counter results stored in rocprofiler_feature_t
  // to hpcrun's gpu_activity_t data structure
  rocprofiler_feature_t* features = entry->features;
  unsigned feature_count = entry->feature_count;

  ga->kind = GPU_ACTIVITY_COUNTER;
  ga->details.counters.correlation_id = rocprofiler_correlation_id;
  ga->details.counters.total_counters = feature_count;

  // This function should be called by rocprofiler thread,
  // which is not monitored. So, this function will not be called
  // inside a signal handler and we can call malloc.
  // The memory is freed when we attribute this gpu_activity_t.
  ga->details.counters.values =
    (gpu_counter_value_t *) malloc(sizeof(gpu_counter_value_t) * feature_count);

  // rocprofiler should pass metric results in the same order
  // that we pass metrics as input to rocprofiler
  for (unsigned i = 0; i < feature_count; ++i) {
    const rocprofiler_feature_t* p = &features[i];
    ga->details.counters.values[i] = decode_counter_value(&p->data);
  }
}

// Profiling completion handler
// Dump and delete the context entry
// Return true if the context was dumped successfully
static bool
rocprofiler_context_handler
(
  rocprofiler_group_t group,
  void* arg
)
{
  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

  // This wait-loop is taken from rocprofiler example.
  // It is strange that the rocprofiler thread will have to
  // wait for subscriber callback to finish.
  volatile bool valid = counter_data.valid;
  while (!valid) {
    sched_yield();
    valid = counter_data.valid;
  }

  // Vladimir: We might not need two while loops. The official rocprof example
  // waits on the `entry->valid`, so I added it just in case.
  // Although not declared as the atomic boolean,
  // the example considers it as the atomic_bool.
  // We should consider using the atomic_bool instea.
  context_entry_t *entry = (context_entry_t *) arg;
  while (entry->valid == false) sched_yield();

  if (counter_data.group.context == NULL) {
    EMSG("error: AMD group->context = NULL");
  }

  gpu_activity_t ga;
  memset(&ga, 0, sizeof(gpu_activity_t));
  cstack_ptr_set(&(ga.next), 0);

  // Copy information from the `entry` to the `ga.
  translate_rocprofiler_output(&ga, entry);
  free(entry);

  // Consume the correlation channel for rocprofiler
  gpu_monitoring_thread_activities_ready_with_idx(ROCPROFILER_CHANNEL_IDX);
  if (gpu_correlation_id_map_lookup(rocprofiler_correlation_id) == NULL) {
    gpu_correlation_id_map_insert(rocprofiler_correlation_id, rocprofiler_correlation_id);
  }
  gpu_activity_process(&ga);

  context_callback_finish = 1;
  return false;
}

static hsa_status_t
rocprofiler_dispatch_callback
(
  const rocprofiler_callback_data_t* callback_data,
  void* arg,
  rocprofiler_group_t* group
) {
  if (total_requested == 0) return HSA_STATUS_SUCCESS;

  // Initialize and set all fields to 0, so use calloc
  context_entry_t *entry = (context_entry_t *) calloc(1, sizeof(context_entry_t));

  // Passed tool data
  hsa_agent_t agent = callback_data->agent;

  // HSA status
  hsa_status_t status = HSA_STATUS_ERROR;

  rocprofiler_t* context = NULL;
  rocprofiler_properties_t properties = {};
  properties.handler = rocprofiler_context_handler;
  properties.handler_arg = (void *) entry;

  counter_data.valid = false;
  HPCRUN_ROCPROFILER_CALL(rocprofiler_open, (agent, rocprofiler_input, total_requested,
                            &context, 0 /*ROCPROFILER_MODE_SINGLEGROUP*/, &properties));


  // Get group[0]
  HPCRUN_ROCPROFILER_CALL(rocprofiler_get_group, (context, 0, group));

  // Fill profiling context entry
  counter_data.agent = agent;
  counter_data.group = *group;
  counter_data.data = *callback_data;
  counter_data.valid = true;


  // The rocprofiler example uses reinterpret_cast<callbacks_data_t*>(arg);
  callbacks_data_t* tool_data = (callbacks_data_t*) arg;
  rocprofiler_feature_t* features = tool_data->features;
  unsigned feature_count = tool_data->feature_count;

  entry->agent = callback_data->agent;
  entry->group = *group;
  entry->features = features;
  entry->feature_count = feature_count;
  entry->file_handle = tool_data->file_handle;
  entry->active = true;
  // The original example uses:
  // reinterpret_cast<std::atomic<bool>*>(&entry->valid)->store(true);
  // Should we use the atomic operation here?
  entry->valid = true;

  // FIXME: some error checking seems warranted
  status = HSA_STATUS_SUCCESS;

  return status;
}

static hsa_status_t
total_counter_accumulator
(
  const rocprofiler_info_data_t info,
  void *data
)
{
  total_counters += 1;
  return HSA_STATUS_SUCCESS;
}

static hsa_status_t
counter_info_accumulator
(
  const rocprofiler_info_data_t info,
  void *data
)
{
  if (getenv("HPCRUN_PRINT_ROCPROFILER_COUNTER_DETAILS")) {
    printf("Enter counter_info_accumulator\n");
    printf("\tname %s\n", info.metric.name);
    printf("\tinstances %d\n", info.metric.instances);
    printf("\texpr %s\n", info.metric.expr);
    printf("\tblock name %s\n", info.metric.block_name);
    printf("\tblock_counters %d\n", info.metric.block_counters);
  }
  counter_name[total_counters] = strdup(info.metric.name);
  counter_description[total_counters] = strdup(info.metric.description);
  total_counters += 1;
  return HSA_STATUS_SUCCESS;
}

static void
initialize_counter_information
(

)
{
  // First we iterate over all counters to get the total
  HPCRUN_ROCPROFILER_CALL(rocprofiler_iterate_info,
    (NULL, ROCPROFILER_INFO_KIND_METRIC, total_counter_accumulator, NULL));

  // Allocate information array
  counter_name = (const char**) malloc(total_counters * sizeof(const char*));
  counter_description = (const char**) malloc(total_counters * sizeof(const char*));

  // Fill in name and description string for each counter
  total_counters = 0;
  HPCRUN_ROCPROFILER_CALL(rocprofiler_iterate_info,
    (NULL, ROCPROFILER_INFO_KIND_METRIC, counter_info_accumulator, NULL));

  // Allocate an array to record whether a counter is asked by the user
  is_specified_by_user = (int*) malloc(total_counters * sizeof(int));
  memset(is_specified_by_user, 0, total_counters * sizeof(int));
}

// TODO: We no longer record binaries for ROCm, so no need for this callback
#if 0
// This function should be implemented in roctracer-api.c,
// but due to c++ism in AMD software, I can only include rocprofiler header
// filers in one .o
static void
roctracer_codeobj_callback
(
  uint32_t domain,
  uint32_t cid,
  const void* data,
  void* arg
)
{
  PRINT("codeobj_callback domain(%u) cid(%u): load_base(0x%lx) load_size(0x%lx) load_delta(0x%lx) uri(\"%s\")\n",
    domain,
    cid,
    evt_data->codeobj.load_base,
    evt_data->codeobj.load_size,
    evt_data->codeobj.load_delta,
    uri);
}
#endif

//******************************************************************************
// AMD hidden interface operations
//******************************************************************************

// This is necessary for rocprofiler callback to work
extern PUBLIC_API void OnLoadToolProp(rocprofiler_settings_t* settings){
  // Enable hsa interception for getting code object URIs
  settings->hsa_intercepting = 1;
}

extern PUBLIC_API void OnUnloadTool() {
  // Must be provided. Otherwise rocprofiler
  // will refuse to work
}

//******************************************************************************
// interface operations
//******************************************************************************


void
rocprofiler_start_kernel
(
  uint64_t correlation_id
)
{
  spinlock_lock(&kernel_lock);
  rocprofiler_correlation_id = correlation_id;
  // We will only allow the critical section
  // to finish after we get rocprofiler results
  context_callback_finish = 0;
  HPCRUN_ROCPROFILER_CALL(rocprofiler_start_queue_callbacks, ());
}


void rocprofiler_stop_kernel(){
  HPCRUN_ROCPROFILER_CALL(rocprofiler_stop_queue_callbacks, ());
  spinlock_unlock(&kernel_lock);
}


void
rocprofiler_init
(
 void
)
{
  if (rocprofiler_initialized) {
    return;
  }
  // Ensure librocprofiler64.so is loaded
  // and initialize all rocprofiler API function pointers
  rocprofiler_initialized = true;

  monitor_disable_new_threads();

#ifndef HPCRUN_STATIC_LINK
  // We usually bind GPU vendor library in finalize_event_list.
  // But here we must do early binding to query supported list of counters
  if (rocprofiler_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to AMD rocprofiler library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

  initialize_counter_information();

  monitor_enable_new_threads();

  // Initialize the spin lock used to serialize GPU kernel launches
  spinlock_init(&kernel_lock);
  return;
}


// Since this functions needs to know the exact number of counters
// the hpcrun is interested for, the function must execute after
// the `finalize_event_list`
void
rocprofiler_register_counter_callbacks
(
  void
)
{
  rocprofiler_queue_callbacks_t callbacks_ptrs = {};
  callbacks_ptrs.dispatch = rocprofiler_dispatch_callback;
  // Initialize the callbacks_data information to use the counters information
  // set within by `initialize_counter_information`.
  callbacks_data_t *callbacks_data = (callbacks_data_t *) malloc(sizeof(callbacks_data_t));
  callbacks_data->features = rocprofiler_input;
  callbacks_data->feature_count = total_requested;

  HPCRUN_ROCPROFILER_CALL(rocprofiler_set_queue_callbacks, (callbacks_ptrs, callbacks_data));
}

void
rocprofiler_fini
(
 void *args,
 int how
)
{
  HPCRUN_ROCPROFILER_CALL(rocprofiler_remove_queue_callbacks, ());
  return;
}



int
rocprofiler_bind
(
 void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(rocprofiler, rocprofiler_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCPROFILER_BIND(fn) \
  CHK_DLSYM(rocprofiler, fn);

  FORALL_ROCPROFILER_ROUTINES(ROCPROFILER_BIND);

#undef ROCPROFILER_BIND

  hpcrun_force_dlopen(true);
  CHK_DLOPEN(hsa, "libhsa-runtime64.so", RTLD_NOW | RTLD_GLOBAL);
  hsa_init();
  hpcrun_force_dlopen(false);

  return DYNAMIC_BINDING_STATUS_OK;
#else
  return DYNAMIC_BINDING_STATUS_ERROR;
#endif // ! HPCRUN_STATIC_LINK
}

void
rocprofiler_wait_context_callback
(
  void
)
{
  // The rocprofiler monitoring thread will set
  // context_callback_finish to 1 after it finishes processing
  // rocprofiler data
  while (context_callback_finish == 0);
}

int
rocprofiler_total_counters
(
  void
)
{
  return total_counters;
}

const char*
rocprofiler_counter_name
(
  int idx
)
{
  if (idx < 0 || idx >= total_counters || counter_name == NULL) return NULL;
  return counter_name[idx];
}

const char*
rocprofiler_counter_description
(
  int idx
)
{
  if (idx < 0 || idx >= total_counters || counter_description == NULL) return NULL;
  return counter_description[idx];
}

int
rocprofiler_match_event
(
  const char *ev_str
)
{
  for (int i = 0; i < total_counters; i++) {
    if (strcmp(ev_str, counter_name[i]) == 0) {
      is_specified_by_user[i] = 1;
      return 1;
    }
  }
  return 0;
}

void
rocprofiler_finalize_event_list
(
)
{
  for (int i = 0; i < total_counters; i++) {
    if (is_specified_by_user[i] == 1) {
      total_requested += 1;
    }
  }

  rocprofiler_input = (rocprofiler_feature_t*) malloc(sizeof(rocprofiler_feature_t) * total_requested);
  memset(rocprofiler_input, 0, total_requested * sizeof(rocprofiler_feature_t));

  requested_counter_name = (const char**) malloc(sizeof(const char*) * total_requested);
  requested_counter_description = (const char**) malloc(sizeof(const char*) * total_requested);

  int cur_id = 0;
  for (int i = 0; i < total_counters; i++) {
    if (is_specified_by_user[i] == 1) {
      rocprofiler_input[cur_id].kind = ROCPROFILER_FEATURE_KIND_METRIC;
      rocprofiler_input[cur_id].name = counter_name[i];
      requested_counter_name[cur_id] = counter_name[i];
      requested_counter_description[cur_id] = counter_description[i];
      cur_id += 1;
    }
  }

  gpu_metrics_GPU_CTR_enable(total_requested, requested_counter_name, requested_counter_description);
}

void
rocprofiler_uri_setup
(
  void
)
{
  // Ask roctracer to set up code object URI callbacks
  // TODO: this really should be implemented in roctracer-api.c,
  // however, due to an AMD header file that is not fully C compatible,
  // I can only include rocprofiler header file in one source file.

  // TODO: We are currently not saving ROCm binaries, so this callback is not
  // needed at the moment.
  // roctracer_enable_op_callback(
  //   ACTIVITY_DOMAIN_HSA_EVT, HSA_EVT_ID_CODEOBJ, roctracer_codeobj_callback, NULL
  // );
}
