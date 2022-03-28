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
// Copyright ((c)) 2002-2022, Rice University
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

#include <monitor.h>

#include "ompt-callstack.h"
#include "ompt-defer.h"
#include "ompt-interface.h"
#include "ompt-queues.h"
#include "ompt-region.h"
#include "ompt-placeholders.h"
#include "ompt-task.h"
#include "ompt-thread.h"
#include "ompt-device.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define ompt_event_may_occur(r) \
  ((r ==  ompt_set_sometimes) | (r ==  ompt_set_always))

#define OMPT_DEBUG_STARTUP 0
#define OMPT_DEBUG_TASK 0



//*****************************************************************************
// static variables
//*****************************************************************************

// struct that contains pointers for initializing and finalizing
static ompt_start_tool_result_t init;
static const char* ompt_runtime_version;

volatile int ompt_debug_wait = 1;

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

//-----------------------------------------
// declare ompt interface function pointers
//-----------------------------------------

#define ompt_interface_fn(f) \
  static f ## _t f ## _fn;

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
  omp_idle_blame_info.participates = ompt_thread_computes;
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
  ompt_set_callback(ompt_callback_thread_begin, ompt_thread_begin);

  ompt_set_callback(ompt_callback_thread_end, ompt_thread_end);
}


static void 
init_parallel_regions
(
 void
)
{
  ompt_parallel_region_register_callbacks(ompt_set_callback_internal);
  ompt_regions_init();
}


static void 
init_tasks
(
 void
)
{
  ompt_task_register_callbacks(ompt_set_callback_internal);
}


static void
__attribute__ ((unused))
init_mutex_blame_shift
(
 const char *version
)
{
  int mutex_blame_shift_avail = 0;
  int retval = 0;

  ompt_mutex_blame_shift_request();

  if (!ompt_mutex_blame_requested) return;

  retval = ompt_set_callback(ompt_callback_mutex_released, 
                             ompt_mutex_blame_accept);
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
__attribute__ ((unused))
init_idle_blame_shift
(
 const char *version
)
{
  int idle_blame_shift_avail = 0;
  int retval = 0;

  if (!ompt_idle_blame_requested) return;

#if 0
  ompt_idle_blame_shift_request();

  retval = ompt_set_callback(ompt_callback_idle, ompt_idle);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);
#endif

  retval = ompt_set_callback(ompt_callback_sync_region_wait, ompt_sync);
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
  ompt_init_placeholders();

  init_threads();
  init_parallel_regions();

#if 0
  // johnmc: disable blame shifting for OpenMP 5 until we have 
  // an appropriate plan
  init_mutex_blame_shift(ompt_runtime_version);
  init_idle_blame_shift(ompt_runtime_version);
#endif

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
  // force hpctoolkit initialization
  monitor_initialize();
  // post-condition: hpctoolkit is initialized

  if (getenv("OMPT_DEBUG_WAIT")) {
    while (ompt_debug_wait);
  }
 
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
    return ompt_get_parallel_info_fn(ancestor_level, parallel_data, team_size);
  }
  return 0;
}

uint64_t hpcrun_ompt_get_unique_id()
{
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

    ompt_get_task_info_fn(level, &task_type_flags, &task_data,
			  &task_frame, &parallel_data, &thread_num);
    //printf("Task frame pointer = %p\n", task_frame);
    return thread_num;
  }
  return -1;
}


ompt_set_result_t 
ompt_set_callback_internal
(
  ompt_callbacks_t event,
  ompt_callback_t callback
)
{
  return ompt_set_callback_fn(event, callback);
}
