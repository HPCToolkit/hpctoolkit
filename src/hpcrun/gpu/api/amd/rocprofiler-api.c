// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "rocprofiler-api.h"

#include <rocprofiler/rocprofiler.h>
#include <rocprofiler/activity.h>

#include "../../activity/gpu-activity.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../activity/gpu-activity-process.h"
#include "../../activity/correlation/gpu-correlation-channel.h"
#include "../../gpu-metrics.h"
#include "../../gpu-monitoring-thread-api.h"
#include "../../gpu-application-thread-api.h"
#include "../../activity/gpu-op-placeholders.h"
#include "../../activity/correlation/gpu-host-correlation-map.h"

#include "../../../audit/audit-api.h"
#include "../../../thread_data.h"
#include "../../../sample-sources/libdl.h"

#include "../../../utilities/hpcrun-nanotime.h"

#include "../../../../common/lean/spinlock.h"
#include <pthread.h>

#define DEBUG 1

#include "../../common/gpu-print.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG_UNIQUE_COUNTERS 0

#if DEBUG_UNIQUE_COUNTERS
#define DUMP_COUNTER_INFO(array, n, label) dump_counter_info(array, n, label)
#else
#define DUMP_COUNTER_INFO(array, n, label)
#endif


#define FORALL_ROCPROFILER_ROUTINES(macro)                      \
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
  hsa_status_t status = ROCPROFILER_FN_NAME(fn) args;   \
  if (status != HSA_STATUS_SUCCESS) {           \
    const char* error_string = NULL; \
    rocprofiler_error_string(&error_string); \
    fprintf(stderr, "ERROR: %s\n", error_string); \
    hpcrun_terminate(); \
  }                                             \
}



//******************************************************************************
// type declarations
//******************************************************************************

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


// Vladimir: Since the size of the `symbols_map_t::iterator` is 8-bytes,
// I added this workaround.
typedef void * symbols_map_it_t;


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

typedef struct {
  const char *name;
  const char *desc;
} counter_info_t;



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
static int total_counters_all = 0;
static int total_counters_useful = 0;
static int total_counters_unique = 0;

static counter_info_t *counter_info_useful = NULL;
static counter_info_t *counter_info_unique = NULL;

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
    hsa_agent_t agent,                  // GPU handle
    rocprofiler_feature_t* features,    // [in/out] profiling feature array
    uint32_t feature_count,                     // profiling feature count
    rocprofiler_t** context,            // [out] profiling context handle
    uint32_t mode,                              // profiling mode mask
    rocprofiler_properties_t* properties        // profiler properties
 )
);

ROCPROFILER_FN
(
  rocprofiler_close,
  (
    rocprofiler_t* context              // [in] profiling context
  )
);

ROCPROFILER_FN
(
  rocprofiler_get_metrics,
  (
    rocprofiler_t* context              // [in/out] profiling context
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
    const hsa_agent_t* agent,                   // [in] GPU handle, NULL for all
                                  // GPU agents
    rocprofiler_info_kind_t kind,                       // kind of iterated info
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
    rocprofiler_t* context,                       // [in/out] profiling context,
                                  //  will be returned as
                                  //  a part of the group structure
    uint32_t index,                                     // [in] group index
    rocprofiler_group_t* group          // [out] profiling group
  )
);



//******************************************************************************
// private operations
//******************************************************************************

static void
dump_counter_info
(
  counter_info_t *ctrs,
  int n,
  const char *label
)
{
  printf("--------------------------------------------------\n");
  printf("%s\n",label);
  printf("--------------------------------------------------\n");
  for(int i=0; i<n; i++) {
    printf("%-29s %s\n", ctrs[i].name, ctrs[i].desc);
  }
  printf("\n");
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
    assert(false && "Unknown rocprofiler_data_kind_t");
    hpcrun_terminate();
  }
}


static void
translate_rocprofiler_output
(
  gpu_activity_t* ga,
  context_entry_t* entry,
  uint64_t *correlation_id
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

  *correlation_id = rocprofiler_correlation_id;

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
  uint64_t correlation_id;
  translate_rocprofiler_output(&ga, entry, &correlation_id);
  free(entry);

  // Consume the correlation channel for rocprofiler
  gpu_monitoring_thread_activities_ready_with_idx(ROCPROFILER_CHANNEL_IDX);

  // FIXME when rocprofiler supports external correlation IDs (check CUPTI code)
  if (correlation_id == 0) {
    context_callback_finish = 1;
    return false;
  }

  gpu_host_correlation_map_entry_t *host_op_entry =
    gpu_host_correlation_map_lookup(correlation_id);
  if (host_op_entry == NULL) {
    context_callback_finish = 1;
    return false;
  }

  gpu_activity_channel_t *channel =
    gpu_host_correlation_map_entry_channel_get(host_op_entry);
  if (channel == NULL) {
    context_callback_finish = 1;
    return false;
  }

  gpu_activity_channel_send(channel, &ga);

  context_callback_finish = 1;
  return false;
}


static hsa_status_t
rocprofiler_dispatch_callback
(
  const rocprofiler_callback_data_t* callback_data,
  void* arg,
  rocprofiler_group_t* group
)
{
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
  total_counters_all += 1;
  return HSA_STATUS_SUCCESS;
}


static bool
counter_is_useful
(
  const rocprofiler_info_data_t *info
)
{
  // for now, only handle single instance metrics
  // because we don't index metrics
  if (info->metric.instances != 1) return false;

  //  no expression is certainly OK
  if (!info->metric.expr) return true;

  // examine each divide in the expression, if any
  // only a divide by a constant is OK
  const char *rest = info->metric.expr;
  for(;;) {
    const char *divide = strchr(rest, '/');

    // if the rest of the expression has no divide, it is OK
    if (!divide) return true;

    // advance past the divide
    rest = ++divide;

    // if not constant divisor, unacceptable
    if (!isdigit(*rest)) return false;
  } while (rest);

  return true;
}


static hsa_status_t
collect_useful_counters
(
  const rocprofiler_info_data_t info,
  void *data
)
{
  char *action;
  if (counter_is_useful(&info)) {
    counter_info_useful[total_counters_useful].name = strdup(info.metric.name);
    counter_info_useful[total_counters_useful].desc = strdup(info.metric.description);
    total_counters_useful += 1;
    action = "add";
  } else {
    action = "skip";
  }

  if (getenv("HPCRUN_PRINT_ROCPROFILER_COUNTER_DETAILS")) {
    printf("%4s ", action);
    printf("block=%-4s ", info.metric.block_name);
    printf("ctrs=%-2d ", info.metric.block_counters);
    printf("instances=%-2d ", info.metric.instances);
    printf("name %-29s ", info.metric.name);
    printf("expr %s\n", info.metric.expr);
  }
  return HSA_STATUS_SUCCESS;
}


static int
counter_info_cmp
(
  const void *c1,
  const void *c2
)
{
#define ci_name(c) (((counter_info_t *)c)->name)
  return strcmp(ci_name(c1), ci_name(c2));
#undef ci_name
}


static counter_info_t *
last_unique_counter
(
  void
)
{
  return &counter_info_unique[total_counters_unique - 1];
}


static void
append_unique_counter
(
  counter_info_t *ctr
)
{
  counter_info_t *unique = last_unique_counter() + 1;
  unique->name = ctr->name;
  unique->desc = ctr->desc;
  total_counters_unique++;
}


static void
collect_unique_counters
(
  void
)
{
  if (total_counters_all > 0) {
    counter_info_useful = (counter_info_t *) malloc(total_counters_all * sizeof(counter_info_t));

    // collect information about useful counters
    HPCRUN_ROCPROFILER_CALL(rocprofiler_iterate_info,
      (NULL, ROCPROFILER_INFO_KIND_METRIC, collect_useful_counters, NULL));

    DUMP_COUNTER_INFO(counter_info_useful, total_counters_useful, "before sort");

    // sort the counters
    qsort(counter_info_useful, total_counters_useful, sizeof(counter_info_t), counter_info_cmp);

    DUMP_COUNTER_INFO(counter_info_useful, total_counters_useful, "after sort");

    //--------------------
    // remove duplicates
    //--------------------

    counter_info_unique = (counter_info_t *) malloc(total_counters_useful * sizeof(counter_info_t));

    // keep the first counter
    append_unique_counter(&counter_info_useful[0]);

    for (int i = 1; i < total_counters_useful; i++) {
      // keep this counter if not same as previous
      if (counter_info_cmp(last_unique_counter(), &counter_info_useful[i]) != 0) {
        append_unique_counter(&counter_info_useful[i]);
      }
    }
    DUMP_COUNTER_INFO(counter_info_unique, total_counters_unique, "unique counters");
  }
}


static void
initialize_counter_information
(
  void
)
{
  // First we iterate over all counters to get the total
  HPCRUN_ROCPROFILER_CALL(rocprofiler_iterate_info,
    (NULL, ROCPROFILER_INFO_KIND_METRIC, total_counter_accumulator, NULL));

  collect_unique_counters();

  // Allocate an array to record whether a counter is requested by the user
  is_specified_by_user = (int*) malloc(total_counters_unique * sizeof(int));
  memset(is_specified_by_user, 0, total_counters_unique * sizeof(int));
}

//******************************************************************************
// AMD hidden interface operations
//******************************************************************************

// This is necessary for rocprofiler callback to work
void foilbase_OnLoadToolProp(void* v_settings){
  rocprofiler_settings_t* settings = v_settings;

  // Enable hsa interception for getting code object URIs
  settings->hsa_intercepting = 1;
}

void foilbase_OnUnloadTool() {
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

  // We usually bind GPU vendor library in finalize_event_list.
  // But here we must do early binding to query supported list of counters
  if (rocprofiler_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to AMD rocprofiler library: %s\n", dlerror());
    EEMSG("hpcrun: see hpcrun --help message for instruction on how to provide a rocprofiler install");
    auditor_exports->exit(-1);
  }

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
  hpcrun_force_dlopen(true);
  const char* rocprofiler_path = getenv("HSA_TOOLS_LIB");
  if (rocprofiler_path == NULL) {
    return DYNAMIC_BINDING_STATUS_ERROR;
  }
  CHK_DLOPEN(rocprofiler, rocprofiler_path, RTLD_NOW | RTLD_GLOBAL);
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
  return total_counters_unique;
}


const char*
rocprofiler_counter_name
(
  int idx
)
{
  if (idx < 0 || idx >= total_counters_unique || counter_info_unique == NULL) return NULL;
  return counter_info_unique[idx].name;
}


const char*
rocprofiler_counter_description
(
  int idx
)
{
  if (idx < 0 || idx >= total_counters_unique || counter_info_unique == NULL) return NULL;
  return counter_info_unique[idx].desc;
}


int
rocprofiler_match_event
(
  const char *ev_str
)
{
  for (int i = 0; i < total_counters_unique; i++) {
    if (strcmp(ev_str, counter_info_unique[i].name) == 0) {
      is_specified_by_user[i] = 1;
      return 1;
    }
  }
  return 0;
}


void
rocprofiler_finalize_event_list
(
  void
)
{
  for (int i = 0; i < total_counters_unique; i++) {
    if (is_specified_by_user[i] == 1) {
      total_requested += 1;
    }
  }

  rocprofiler_input = (rocprofiler_feature_t*) malloc(sizeof(rocprofiler_feature_t) * total_requested);
  memset(rocprofiler_input, 0, total_requested * sizeof(rocprofiler_feature_t));

  requested_counter_name = (const char**) malloc(sizeof(const char*) * total_requested);
  requested_counter_description = (const char**) malloc(sizeof(const char*) * total_requested);

  int cur_id = 0;
  for (int i = 0; i < total_counters_unique; i++) {
    if (is_specified_by_user[i] == 1) {
      rocprofiler_input[cur_id].kind = ROCPROFILER_FEATURE_KIND_METRIC;
      rocprofiler_input[cur_id].name = counter_info_unique[i].name;
      requested_counter_name[cur_id] = counter_info_unique[i].name;
      requested_counter_description[cur_id] = counter_info_unique[i].desc;
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
