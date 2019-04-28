// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$ 
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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

/******************************************************************************
 * global include files
 *****************************************************************************/

#include <sys/param.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct-node-vector.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/hpcrun-initializers.h>
#include <hpcrun/main.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/blame-shift/blame-shift.h>
#include <hpcrun/sample-sources/blame-shift/directed.h>
#include <hpcrun/sample-sources/blame-shift/undirected.h>
#include <hpcrun/sample-sources/sample-filters.h>
#include <hpcrun/thread_data.h>

#include "ompt-callstack.h"
#include "ompt-defer.h"
#include "ompt-interface.h"
#include "ompt-queues.h"
#include "ompt-region.h"
#include "ompt-state-placeholders.h"
#include "ompt-task.h"
#include "ompt-thread.h"
#include "ompt-device-map.h"


#define HAVE_CUDA_H 1

#if HAVE_CUDA_H
#include "sample-sources/nvidia/nvidia.h"
#include "sample-sources/nvidia/cubin-id-map.h"
#include "sample-sources/nvidia/cubin-symbols.h"
#include "sample-sources/nvidia/cupti-api.h"
#include "sample-sources/nvidia/cupti-record.h"
#endif


//*****************************************************************************
// macros
//*****************************************************************************

#define ompt_event_may_occur(r) \
  ((r ==  ompt_set_sometimes) | (r ==  ompt_set_always))

#define OMPT_DEBUG_STARTUP 0
#define OMPT_DEBUG_TASK 0
#define OMPT_ACTIVITY_DEBUG 0

#if OMPT_ACTIVITY_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



//*****************************************************************************
// static variables
//*****************************************************************************

// struct that contains pointers for initializing and finalizing
static ompt_start_tool_result_t init;
static const char* ompt_runtime_version;

static int ompt_elide = 0;
static int ompt_initialized = 0;

static int ompt_task_full_context = 0;

static int ompt_mutex_blame_requested = 0;
static int ompt_idle_blame_requested = 0;

static int ompt_idle_blame_enabled = 0;

static bs_fn_entry_t mutex_bs_entry;
static bs_fn_entry_t idle_bs_entry;
static sf_fn_entry_t serial_only_sf_entry;

// state for directed blame shifting away from spinning on a mutex
static directed_blame_info_t omp_mutex_blame_info;

// state for undirected blame shifting away from spinning waiting for work
static undirected_blame_info_t omp_idle_blame_info;

static device_finalizer_fn_entry_t device_finalizer;

// target kind metric
static int ompt_target_metric_id = -1;
static int ompt_task_metric_id = -1;

// pc sampling
static bool ompt_pc_sampling_enabled = false;

//-----------------------------------------
// declare ompt interface function pointers
//-----------------------------------------
#define ompt_interface_fn(f) f ## _t f ## _fn;

FOREACH_OMPT_INQUIRY_FN(ompt_interface_fn)

#undef ompt_interface_fn



/******************************************************************************
 * thread-local variables
 *****************************************************************************/

//-----------------------------------------
// variable ompt_idle_count:
//    this variable holds a count of how 
//    many times the current thread has
//    been marked as idle. a count is used 
//    rather than a flag to support
//    nested marking.
//-----------------------------------------
static __thread int ompt_idle_count;
static __thread cct_node_t *target_node = NULL;


/******************************************************************************
 * private operations 
 *****************************************************************************/

//----------------------------------------------------------------------------
// support for directed blame shifting for mutex objects
//----------------------------------------------------------------------------

//-------------------------------------------------
// return a mutex that should be blamed for 
// current waiting (if any)
//-------------------------------------------------

static uint64_t
ompt_mutex_blame_target
(
 void
)
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    switch (state) {
    case ompt_state_wait_critical:
    case ompt_state_wait_lock:
    case ompt_state_wait_atomic:
    case ompt_state_wait_ordered:
      return wait_id;
    default: break;
    }
  }
  return 0;
}


static int
ompt_serial_only
(
 void *arg
)
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    ompt_thread_t ttype = ompt_thread_type_get();
    if (ttype != ompt_thread_initial) return 1;

    if (state == ompt_state_work_serial) return 0;
    return 1;
  }
  return 0;
}


static int *
ompt_get_idle_count_ptr
(
 void
)
{
  return &ompt_idle_count;
}


//----------------------------------------------------------------------------
// identify whether a thread is an OpenMP thread or not
//----------------------------------------------------------------------------

static _Bool
ompt_thread_participates
(
 void
)
{
  switch(ompt_thread_type_get()) {
  case ompt_thread_initial:
  case ompt_thread_worker:
    return true;
  case ompt_thread_other:
  case ompt_thread_unknown:
  default:
    break;
  }
  return false;
}


static _Bool
ompt_thread_needs_blame
(
 void
)
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);
    switch(state) {
      case ompt_state_idle:
      case ompt_state_wait_barrier:
      case ompt_state_wait_barrier_implicit:
      case ompt_state_wait_barrier_explicit:
      case ompt_state_wait_taskwait:
      case ompt_state_wait_taskgroup:
         return false;
      default:
        return true;
    }
  }
  return true;
}


//----------------------------------------------------------------------------
// interface function to register support for directed blame shifting for 
// OpenMP operations on mutexes if event OMP_MUTEX is present
//----------------------------------------------------------------------------

static void
ompt_mutex_blame_shift_register
(
 void
)
{
  mutex_bs_entry.fn = directed_blame_sample;
  mutex_bs_entry.next = NULL;
  mutex_bs_entry.arg = &omp_mutex_blame_info;

  omp_mutex_blame_info.get_blame_target = ompt_mutex_blame_target;

  omp_mutex_blame_info.blame_table = blame_map_new();
  omp_mutex_blame_info.levels_to_skip = 1;

  blame_shift_register(&mutex_bs_entry);
}


static void
ompt_register_mutex_metrics
(
 void
)
{
  kind_info_t *mut_kind = hpcrun_metrics_new_kind();
  omp_mutex_blame_info.wait_metric_id = 
    hpcrun_set_new_metric_info_and_period(mut_kind, "OMP_MUTEX_WAIT", 
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  omp_mutex_blame_info.blame_metric_id = 
    hpcrun_set_new_metric_info_and_period(mut_kind, "OMP_MUTEX_BLAME",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  hpcrun_close_kind(mut_kind);
}


static void
__attribute__ ((unused))
ompt_register_idle_metrics
(
 void
)
{
  kind_info_t *idl_kind = hpcrun_metrics_new_kind();
  omp_idle_blame_info.idle_metric_id = 
    hpcrun_set_new_metric_info_and_period(idl_kind, "OMP_IDLE",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  omp_idle_blame_info.work_metric_id = 
    hpcrun_set_new_metric_info_and_period(idl_kind, "OMP_WORK",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  hpcrun_close_kind(idl_kind);
}


static void
ompt_idle_blame_shift_register
(
 void
)
{
  ompt_idle_blame_enabled = 1;

  idle_bs_entry.fn = undirected_blame_sample;
  idle_bs_entry.next = NULL;
  idle_bs_entry.arg = &omp_idle_blame_info;

  omp_idle_blame_info.get_idle_count_ptr = ompt_get_idle_count_ptr;
  omp_idle_blame_info.participates = ompt_thread_participates;
  omp_idle_blame_info.working = ompt_thread_needs_blame;

  blame_shift_register(&idle_bs_entry);
}


//----------------------------------------------------------------------------
// initialize pointers to callback functions supported by the OMPT interface
//----------------------------------------------------------------------------

static void 
ompt_init_inquiry_fn_ptrs
(
 ompt_function_lookup_t ompt_fn_lookup
)
{
#define ompt_interface_fn(f) \
  f ## _fn = (f ## _t) ompt_fn_lookup(#f);

FOREACH_OMPT_INQUIRY_FN(ompt_interface_fn)

#undef ompt_interface_fn
}


// johnmc merge
#if 0
//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------

static
void ompt_task_begin(ompt_task_id_t parent_task_id, 
		   ompt_frame_t *parent_task_frame, 
		   ompt_task_id_t new_task_id,
		   void *task_function)
{
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};

  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead++;

  ucontext_t uc;
  getcontext(&uc);

  hpcrun_safe_enter();

  // record the task creation context into task structure (in omp runtime)
  hpcrun_sample_callpath(&uc, ompt_task_metric_id, zero_metric_incr, 1, 1, NULL);

  hpcrun_safe_exit();

  task_map_insert(new_task_id, cct_node);  

  td->overhead--;
}
#endif


//----------------------------------------------------------------------------
// support for undirected blame shifting to attribute idleness
//----------------------------------------------------------------------------

//-------------------------------------------------
// note birth and death of threads to support 
// undirected blame shifting using the IDLE metric
//-------------------------------------------------


static void
ompt_thread_begin
(
 ompt_thread_t thread_type,
 ompt_data_t *thread_data
)
{
  //printf("Thread begin... %d\n", thread_data->value);
  ompt_thread_type_set(thread_type);
  undirected_blame_thread_start(&omp_idle_blame_info);

  wfq_init(&threads_queue);

  registered_regions = NULL;
  unresolved_cnt = 0;
//  printf("Tree root begin: %p\n", td->core_profile_trace_data.epoch->csdata.tree_root);
}


static void
ompt_thread_end
(
 ompt_data_t *thread_data
)
{
  // TODO(keren): test if it is called
  undirected_blame_thread_end(&omp_idle_blame_info);
//  printf("DEBUG: number of unresolved regions: %d\n", unresolved_cnt);
}


//-------------------------------------------------
// note the beginning and end of idleness to 
// support undirected blame shifting
//-------------------------------------------------

static void
ompt_idle_begin
(
 void
)
{
  undirected_blame_idle_begin(&omp_idle_blame_info);
  if (!ompt_eager_context_p()) {
    while(try_resolve_one_region_context());
  }
}


static void
ompt_idle_end
(
 void
)
{
  undirected_blame_idle_end(&omp_idle_blame_info);
}


static void 
__attribute__ ((unused))
ompt_idle
(
 ompt_scope_endpoint_t endpoint
)
{
  if (endpoint == ompt_scope_begin) ompt_idle_begin();
  else if (endpoint == ompt_scope_end) ompt_idle_end();
  else assert(0);

  //printf("Thread id = %d, \tIdle %s\n", omp_get_thread_num(), endpoint==1?"begin":"end");
}

static void
ompt_sync
(
 ompt_sync_region_t kind,
 ompt_scope_endpoint_t endpoint,
 ompt_data_t *parallel_data,
 ompt_data_t *task_data,
 const void *codeptr_ra
)
{
  if (kind == ompt_sync_region_barrier) {
    if (endpoint == ompt_scope_begin) ompt_idle_begin();
    else if (endpoint == ompt_scope_end) ompt_idle_end();
    else assert(0);

    //printf("Thread id = %d, \tBarrier %s\n", omp_get_thread_num(), endpoint==1?"begin":"end");
  }
}


//-------------------------------------------------
// accept any blame accumulated for mutex while 
// this thread held it
//-------------------------------------------------

static void 
ompt_mutex_blame_accept
(
 uint64_t mutex
)
{
  //printf("Lock has been realesed. \n");
  directed_blame_accept(&omp_mutex_blame_info, mutex);
}


//----------------------------------------------------------------------------
// initialization of OMPT interface by setting up callbacks
//----------------------------------------------------------------------------

static void 
init_threads
(
 void
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_callback_thread_begin,
		    (ompt_callback_t)ompt_thread_begin);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_callback_thread_end,
		    (ompt_callback_t)ompt_thread_end);
  assert(ompt_event_may_occur(retval));
}


static void 
init_parallel_regions
(
 void
)
{
  ompt_parallel_region_register_callbacks(ompt_set_callback_fn);
  ompt_regions_init();
}


static void 
init_tasks
(
 void
)
{
  ompt_task_register_callbacks(ompt_set_callback_fn);
}


static void
init_mutex_blame_shift
(
 const char *version
)
{
  int mutex_blame_shift_avail = 0;
  int retval = 0;

  ompt_mutex_blame_shift_request();

  if (!ompt_mutex_blame_requested) return;

  retval = ompt_set_callback_fn(ompt_callback_mutex_released,
                                (ompt_callback_t) ompt_mutex_blame_accept);
  mutex_blame_shift_avail |= ompt_event_may_occur(retval);


  if (mutex_blame_shift_avail) {
    ompt_mutex_blame_shift_register();
  } else {
    printf("hpcrun warning: OMP_MUTEX event requested, however the\n"
               "OpenMP runtime present (%s) doesn't support the\n"
               "events needed for MUTEX blame shifting. As a result\n"
               "OMP_MUTEX blame will not be monitored or reported.\n", version);
  }
}


//-------------------------------------------------
// register callbacks to support undirected blame
// shifting, namely attributing thread idleness to
// the work happening on other threads when the
// idleness occurs.
//-------------------------------------------------
static void
init_idle_blame_shift
(
 const char *version
)
{
  int idle_blame_shift_avail = 0;
  int retval = 0;

  if (!ompt_idle_blame_requested) return;

  ompt_idle_blame_shift_request();

#if 0
  retval = ompt_set_callback_fn(ompt_callback_idle,
                                (ompt_callback_t)ompt_idle);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);
#endif

  retval = ompt_set_callback_fn(ompt_callback_sync_region_wait,
                                (ompt_callback_t)ompt_sync);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);


  if (idle_blame_shift_avail) {
    ompt_idle_blame_shift_register();
  } else {
    printf("hpcrun warning: OMP_IDLE event requested, however the\n"
             "OpenMP runtime present (%s) doesn't support the\n"
             "events needed for blame shifting idleness. As a result\n"
             "OMP_IDLE blame will not be monitored or reported.\n", version);
  }
}


//*****************************************************************************
// interface operations
//*****************************************************************************


//-------------------------------------------------
// ompt_initialize will be called by an OpenMP
// runtime system immediately after it initializes
// itself.
//-------------------------------------------------


// forward declaration
void prepare_device();

void hpcrun_ompt_device_finalizer(void *args);

static int
ompt_initialize
(
 ompt_function_lookup_t lookup,
 int initial_device_num,
 ompt_data_t *tool_data
)
{
  hpcrun_safe_enter();

#if OMPT_DEBUG_STARTUP
  printf("Initializing OMPT interface\n");
#endif
  ompt_initialized = 1;

  ompt_init_inquiry_fn_ptrs(lookup);
  ompt_init_placeholders(lookup);

  init_threads();
  init_parallel_regions();

  init_mutex_blame_shift(ompt_runtime_version);
  init_idle_blame_shift(ompt_runtime_version);

  char* ompt_task_full_ctxt_str = getenv("OMPT_TASK_FULL_CTXT");
  if (ompt_task_full_ctxt_str) {
    ompt_task_full_context = 
      strcmp("ENABLED", getenv("OMPT_TASK_FULL_CTXT")) == 0;
  } else{
    ompt_task_full_context = 0;
  }

  prepare_device();
  init_tasks();

#if DEBUG_TASK
  printf("Task full context: %s\n", ompt_task_full_context ? "yes" : "no");
#endif

  if (!ENABLED(OMPT_KEEP_ALL_FRAMES)) {
    ompt_elide = 1;
    ompt_callstack_init();
  }
  if (getenv("HPCRUN_OMP_SERIAL_ONLY")) {
    serial_only_sf_entry.fn = ompt_serial_only;
    serial_only_sf_entry.arg = 0;
    sample_filters_register(&serial_only_sf_entry);
  }

  hpcrun_safe_exit();

  return 1;
}


void
ompt_finalize
(
 ompt_data_t *tool_data
)
{
  hpcrun_safe_enter();

#if OMPT_DEBUG_STARTUP
  printf("Finalizing OMPT interface\n");
#endif

  hpcrun_safe_exit();
}


ompt_start_tool_result_t *
ompt_start_tool
(
 unsigned int omp_version,
 const char *runtime_version
)
{
#if OMPT_DEBUG_STARTUP
  printf("Starting tool...\n");
#endif
  ompt_runtime_version = runtime_version;

  init.initialize = &ompt_initialize;
  init.finalize = &ompt_finalize;

  return &init;
}


int 
hpcrun_omp_state_is_overhead
(
 void
)
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    switch (state) {
    case ompt_state_overhead:
    case ompt_state_wait_critical:
    case ompt_state_wait_lock:
    case ompt_state_wait_atomic:
    case ompt_state_wait_ordered:
      return 1;

    default: 
      break;
    }
  }
  return 0;
}


//-------------------------------------------------
// returns true if OpenMP runtime frames should
// be elided out of thread callstacks
//-------------------------------------------------

int
hpcrun_ompt_elide_frames
(
 void
)
{
  return ompt_elide; 
}


//----------------------------------------------------------------------------
// safe (wrapped) versions of API functions that can be called from the rest
// of hpcrun, even if no OMPT support is available
//----------------------------------------------------------------------------


ompt_state_t
hpcrun_ompt_get_state
(
 uint64_t *wait_id
)
{
  if (ompt_initialized) return ompt_get_state_fn(wait_id);
  return ompt_state_undefined;
}

ompt_state_t
hpcrun_ompt_get_state_only
(
 void
)
{
  uint64_t wait_id;
  return hpcrun_ompt_get_state(&wait_id);
}


ompt_frame_t *
hpcrun_ompt_get_task_frame
(
 int level
)
{
  if (ompt_initialized) {
    int task_type_flags;
    ompt_data_t *task_data = NULL;
    ompt_data_t *parallel_data = NULL;
    ompt_frame_t *task_frame = NULL;
    int thread_num = 0;

    ompt_get_task_info_fn(level, &task_type_flags, &task_data, &task_frame, &parallel_data, &thread_num);
    //printf("Task frame pointer = %p\n", task_frame);
    return task_frame;
  }
  return NULL;
}


ompt_data_t*
hpcrun_ompt_get_task_data
(
 int level
)
{
  if (ompt_initialized){
    int task_type_flags;
    ompt_data_t *task_data = NULL;
    ompt_data_t *parallel_data = NULL;
    ompt_frame_t *task_frame = NULL;
    int thread_num = 0;

    ompt_get_task_info_fn(level, &task_type_flags, &task_data, &task_frame, &parallel_data, &thread_num);
    return task_data;
  }
  return (ompt_data_t*) ompt_data_none;
}


void *
hpcrun_ompt_get_idle_frame
(
 void
)
{
  return NULL;
}


// new version

int hpcrun_ompt_get_parallel_info
(
 int ancestor_level,
 ompt_data_t **parallel_data,
 int *team_size
)
{
  if (ompt_initialized) {
    // FIXME: changed at 2nd March 2018 16:43 CET
    return ompt_get_parallel_info_fn(ancestor_level, parallel_data, team_size);
//    return 2;
  }
  return 0;
}

uint64_t hpcrun_ompt_get_unique_id()
{
//  static uint64_t ID = 0x1000000000000000;
//  return __sync_fetch_and_add(&ID, 1);
//  static uint64_t thread = 1;
//  static __thread uint64_t ID = 0;
//  if (ompt_initialized) {
//    if (ID == 0) {
//      uint64_t new_thread = __sync_fetch_and_add(&thread, 1);
//      ID = new_thread << (sizeof(uint64_t) * 8 - 16);
//    }
//    ++ID;
//    return ID;
//  }else return 0;

  if (ompt_initialized) return ompt_get_unique_id_fn();
  return 0;
}


uint64_t 
hpcrun_ompt_get_parallel_info_id
(
 int ancestor_level
)
{
  ompt_data_t *parallel_info = NULL;
  int team_size = 0;
  hpcrun_ompt_get_parallel_info(ancestor_level, &parallel_info, &team_size);
  if (parallel_info == NULL) return 0;
  return parallel_info->value;
//  ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_info->ptr;
//  return region_data->region_id;
}


void 
hpcrun_ompt_get_parallel_info_id_pointer
(
 int ancestor_level, 
 uint64_t *region_id
)
{
  ompt_data_t *parallel_info = NULL;
  int team_size = 0;
  hpcrun_ompt_get_parallel_info(ancestor_level, &parallel_info, &team_size);
  if (parallel_info == NULL){
    *region_id = 0L;
  };
  *region_id =  parallel_info->value;
}


//-------------------------------------------------
// a special safe function that determines the
// outermost parallel region enclosing the current
// context.
//-------------------------------------------------

uint64_t
hpcrun_ompt_outermost_parallel_id
(
 void
)
{ 
  ompt_id_t outer_id = 0; 
  if (ompt_initialized) { 
    int i = 0;
    for (;;) {
      ompt_id_t next_id = hpcrun_ompt_get_parallel_info_id(i++);
      if (next_id == 0) break;
      outer_id = next_id;
    }
  }
  return outer_id;
}


void
ompt_mutex_blame_shift_request
(
  void
)
{
  ompt_mutex_blame_requested = 1;
  ompt_register_mutex_metrics();
}


int 
ompt_task_full_context_p
(
  void
)
{
  return ompt_task_full_context;
}


// added by vi3:

// FIXME: be sure that everything is connected properl ymaybe we need
// sometimes to call wfq_get_next just to wait eventually lately connections


// vi3: freelist helper functions
// rename freelist_remove_first
ompt_base_t*
freelist_remove_first(ompt_base_t **head){
  if (*head){
    ompt_base_t* first = *head;
    // note: this is not thread safe, because of that we introduce private_queue_remove_first
    *head = OMPT_BASE_T_GET_NEXT(first);
    return first;
  }
  return ompt_base_nil;
}


void 
freelist_add_first
(
 ompt_base_t *new, 
 ompt_base_t **head
)
{
  if (!new)
    return;
  OMPT_BASE_T_GET_NEXT(new) = *head;
  *head = new;
}
// allocating and free notifications
ompt_notification_t*
hpcrun_ompt_notification_alloc
(
 void
)
{
  // only the current thread uses notification_freelist_head
  ompt_notification_t* first = (ompt_notification_t*) freelist_remove_first(
          OMPT_BASE_T_STAR_STAR(notification_freelist_head));
  return first ? first : (ompt_notification_t*)hpcrun_malloc(sizeof(ompt_notification_t));
}


void
hpcrun_ompt_notification_free
(
 ompt_notification_t *notification
)
{
  freelist_add_first(OMPT_BASE_T_STAR(notification), OMPT_BASE_T_STAR_STAR(notification_freelist_head));
}


// allocate and free thread's region
ompt_trl_el_t*
hpcrun_ompt_trl_el_alloc
(
 void
)
{
  // only the thread that owns thread_region_freelist_head access to it
  ompt_trl_el_t* first = (ompt_trl_el_t*) freelist_remove_first(OMPT_BASE_T_STAR_STAR(thread_region_freelist_head));
  return first ? first : (ompt_trl_el_t*)hpcrun_malloc(sizeof(ompt_trl_el_t));
}


void
hpcrun_ompt_trl_el_free
(
 ompt_trl_el_t *thread_region
)
{
  freelist_add_first(OMPT_BASE_T_STAR(thread_region), OMPT_BASE_T_STAR_STAR(thread_region_freelist_head));
}


//FIXME: regex \(ompt_base_t\s+\*\)

// vi3: Helper function to get region_data
ompt_region_data_t*
hpcrun_ompt_get_region_data
(
 int ancestor_level
)
{
  ompt_data_t* parallel_data = NULL;
  int team_size;
  int ret_val = hpcrun_ompt_get_parallel_info(ancestor_level, &parallel_data, &team_size);
  // FIXME: potential problem if parallel info is unavailable and runtime returns 1
  if (ret_val < 2)
    return NULL;
  return parallel_data ? (ompt_region_data_t*)parallel_data->ptr : NULL;
}


ompt_region_data_t*
hpcrun_ompt_get_current_region_data
(
 void
)
{
  return hpcrun_ompt_get_region_data(0);
}


ompt_region_data_t*
hpcrun_ompt_get_parent_region_data
(
 void
)
{
  return hpcrun_ompt_get_region_data(1);
}

int
hpcrun_ompt_get_thread_num(int level)
{
    if (ompt_initialized) {
        int task_type_flags;
        ompt_data_t *task_data = NULL;
        ompt_data_t *parallel_data = NULL;
        ompt_frame_t *task_frame = NULL;
        int thread_num = 0;

        ompt_get_task_info_fn(level, &task_type_flags, &task_data, &task_frame, &parallel_data, &thread_num);
        //printf("Task frame pointer = %p\n", task_frame);
        return thread_num;
    }
    return -1;
}


static void
hpcrun_ompt_op_id_notify(ompt_id_t host_op_id, uint16_t ip)
{
  cct_node_t *cct_child = hpcrun_cct_insert_dummy(target_node, ip);
  cupti_worker_notification_apply(host_op_id, cct_child);
}


//*****************************************************************************
// device operations
//*****************************************************************************
#define OMPT_API_FNTYPE(fn) fn##_t

#define OMPT_API_FUNCTION(return_type, fn, args)  \
    typedef return_type (*OMPT_API_FNTYPE(fn)) args

#define OMPT_TARGET_API_FUNCTION(return_type, fn, args)  \
    OMPT_API_FUNCTION(return_type, fn, args) 

OMPT_TARGET_API_FUNCTION(void, ompt_set_pc_sampling, (
    ompt_device_t *device,
    int enable,
    int pc_sampling_frequency
));

#define FOREACH_OMPT_TARGET_FN(macro) \
  macro(ompt_get_device_time) \
  macro(ompt_translate_time) \
  macro(ompt_set_trace_native) \
  macro(ompt_start_trace) \
  macro(ompt_pause_trace) \
  macro(ompt_stop_trace) \
  macro(ompt_get_record_type) \
  macro(ompt_get_record_native) \
  macro(ompt_get_record_abstract) \
  macro(ompt_advance_buffer_cursor) \
  macro(ompt_set_pc_sampling) 

#define ompt_decl_name(fn) \
  fn ## _t  fn;

  FOREACH_OMPT_TARGET_FN(ompt_decl_name)

#undef ompt_decl_name

void 
ompt_bind_names(ompt_function_lookup_t lookup)
{
#define ompt_bind_name(fn) \
  fn = (fn ## _t ) lookup(#fn);

  FOREACH_OMPT_TARGET_FN(ompt_bind_name)

#undef ompt_bind_name
}


#define BUFFER_SIZE (1024 * 1024 * 8)

void 
ompt_callback_buffer_request
(
 int device_id,
 ompt_buffer_t **buffer,
 size_t *bytes
)
{
  *bytes = BUFFER_SIZE;
  *buffer = (ompt_buffer_t *)malloc(*bytes);
  assert(buffer);
}


void 
ompt_callback_buffer_complete
(
 int device_id,
 ompt_buffer_t *buffer,
 size_t bytes,
 ompt_buffer_cursor_t begin,
 int buffer_owned
)
{
  // handle notifications
  cupti_cupti_notification_apply(cupti_notification_handle);
  // signal advance to return pointer to first record
  ompt_buffer_cursor_t next = begin;
  int status = 0;
  do {
    // TODO(keren): replace cupti_activity_handle with device_activity handle
    CUpti_Activity *activity = (CUpti_Activity *)next;
    cupti_activity_process(activity);
    status = cupti_buffer_cursor_advance(buffer, bytes, (CUpti_Activity **)&next);
  } while(status);
}


void
ompt_pc_sampling_enable()
{
  ompt_pc_sampling_enabled = true;
}


void
ompt_pc_sampling_disable()
{
  ompt_pc_sampling_enabled = false;
}


void
ompt_trace_configure(ompt_device_t *device)
{
  int flags = 0;

  // specify desired monitoring
  flags |= ompt_native_driver;

  flags |= ompt_native_runtime;

  flags |= ompt_native_kernel_invocation;

  flags |= ompt_native_kernel_execution;

  flags |= ompt_native_data_motion_explicit;

  // indicate desired monitoring
  ompt_set_trace_native(device, 1, flags);
  
  // set pc sampling after other traces
  if (ompt_pc_sampling_enabled) {
    ompt_set_pc_sampling(device, true, cupti_pc_sampling_frequency_get());
  }

  // turn on monitoring previously indicated
  ompt_start_trace(device, ompt_callback_buffer_request, ompt_callback_buffer_complete);
}


void
ompt_device_initialize(uint64_t device_num,
                       const char *type,
                       ompt_device_t *device,
                       ompt_function_lookup_t lookup,
                       const char *documentation)
{
  PRINT("ompt_device_initialize->%s, %d\n", type, device_num);

  ompt_bind_names(lookup);

  ompt_trace_configure(device);

  ompt_device_map_insert(device_num, device, type);
}


void 
ompt_device_finalize(uint64_t device_num)
{
}


void 
ompt_device_load(uint64_t device_num,
                 const char *filename,
                 int64_t file_offset,
                 const void *file_addr,
                 size_t bytes,
                 const void *host_addr,
                 const void *device_addr,
                 uint64_t module_id)
{
  PRINT("ompt_device_load->%s, %d\n", filename, device_num);
  cupti_load_callback_cuda(module_id, host_addr, bytes);
}


void 
ompt_device_unload(uint64_t device_num,
                   uint64_t module_id)
{
  //cubin_id_map_delete(module_id);
}


void 
ompt_target_callback
(
  ompt_target_t kind,
  ompt_scope_endpoint_t endpoint,
  uint64_t device_num,
  ompt_data_t *task_data,
  ompt_id_t target_id,
  const void *codeptr_ra
)
{
  PRINT("ompt_target_callback->target_id %d\n", target_id);
  // If a thread creates a target region, we init records
  // and it must be flushed in the finalizer
  cupti_stop_flag_set();
  cupti_record_init();

  // process cupti records
  cupti_worker_activity_apply(cupti_activity_handle);

  // sample a record
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  ucontext_t uc;
  getcontext(&uc);
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead++;
  // NOTE(keren): hpcrun_safe_enter prevent self interruption
  hpcrun_safe_enter();
  
  target_node = hpcrun_sample_callpath(&uc, ompt_target_metric_id, zero_metric_incr, 0, 1, NULL).sample_node; 

  hpcrun_safe_exit();
  td->overhead--;
}

#define FOREACH_OMPT_DATA_OP(macro)                                        \
  macro(op, ompt_target_data_alloc, ompt_op_alloc)                \
  macro(op, ompt_target_data_transfer_to_device, ompt_op_copy_in)    \
  macro(op, ompt_target_data_transfer_from_device, ompt_op_copy_out) \
  macro(op, ompt_target_data_delete, ompt_op_delete)

void
ompt_data_op_callback(ompt_id_t target_id,
                      ompt_id_t host_op_id,
                      ompt_target_data_op_t optype,
                      void *host_addr,
                      void *device_addr,
                      size_t bytes)
{
  uint16_t op = 0;
  switch (optype) {                       
#define ompt_op_macro(op, ompt_op_type, ompt_op_class) \
    case ompt_op_type:                                 \
      op = ompt_op_class;                              \
      break;
    
    FOREACH_OMPT_DATA_OP(ompt_op_macro);

#undef ompt_op_macro
    default:
      break;
  }
  hpcrun_ompt_op_id_notify(host_op_id, op);
}


void
ompt_submit_callback(ompt_id_t target_id,
                     ompt_id_t host_op_id)
{
  PRINT("ompt_submit_callback enter->target_id %d\n", target_id);
  hpcrun_ompt_op_id_notify(host_op_id, ompt_op_kernel_submit);
  PRINT("ompt_submit_callback exit->target_id %d\n", target_id);
}


void
ompt_map_callback(ompt_id_t target_id,
                  unsigned int nitems,
                  void **host_addr,
                  void **device_addr,
                  size_t *bytes,
                  unsigned int *mapping_flags)
{
}


#define ompt_set_callback(e, cb) ompt_set_callback_fn(e, (ompt_callback_t) cb)

void
prepare_device()
{
  PRINT("ompt_initialize->prepare_device enter\n");

  device_finalizer.fn = cupti_device_flush;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer);

  kind_info_t *ompt_target_kind = hpcrun_metrics_new_kind();
  ompt_target_metric_id = hpcrun_set_new_metric_info(ompt_target_kind, "OMPT_TARGET_KIND"); 
  hpcrun_close_kind(ompt_target_kind);

  kind_info_t *ompt_task_kind = hpcrun_metrics_new_kind();
  ompt_task_metric_id = hpcrun_set_new_metric_info(ompt_task_kind, "OMPT_TASK_KIND");
  hpcrun_close_kind(ompt_task_kind);

  ompt_set_callback(ompt_callback_device_initialize, ompt_device_initialize);
  ompt_set_callback(ompt_callback_device_finalize, ompt_device_finalize);
  ompt_set_callback(ompt_callback_device_load, ompt_device_load);
  ompt_set_callback(ompt_callback_device_unload, ompt_device_unload);
  ompt_set_callback(ompt_callback_target, ompt_target_callback);
  ompt_set_callback(ompt_callback_target_data_op, ompt_data_op_callback);
  ompt_set_callback(ompt_callback_target_submit, ompt_submit_callback);
  ompt_set_callback(ompt_callback_target_map, ompt_map_callback);

  PRINT("ompt_initialize->prepare_device exit\n");
}
