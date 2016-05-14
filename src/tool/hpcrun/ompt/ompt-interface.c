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
 * ompt
 *****************************************************************************/

#include <ompt.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/ompt/ompt-region.h>
#include <hpcrun/ompt/ompt-task.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/hpcrun-initializers.h>

#include "ompt-interface.h"
#include "ompt-callstack.h"
#include "ompt-state-placeholders.h"
#include "ompt-thread.h"

#include "sample-sources/sample-filters.h"
#include "sample-sources/blame-shift/directed.h"
#include "sample-sources/blame-shift/undirected.h"
#include "sample-sources/blame-shift/blame-shift.h"
#include "sample-sources/blame-shift/blame-map.h"

#include "sample-sources/idle.h"


/******************************************************************************
 * macros
 *****************************************************************************/

#define ompt_event_may_occur(r) \
  ((r ==  ompt_has_event_may_callback) | (r ==  ompt_has_event_must_callback))



/******************************************************************************
 * static variables
 *****************************************************************************/

static int ompt_elide = 0;
static int ompt_initialized = 0;

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
ompt_mutex_blame_target()
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    switch (state) {
    case ompt_state_wait_critical:
    case ompt_state_wait_lock:
    case ompt_state_wait_nest_lock:
    case ompt_state_wait_atomic:
    case ompt_state_wait_ordered:
      return wait_id;
    default: break;
    }
  }
  return 0;
}


static int
ompt_serial_only(void *arg)
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    ompt_thread_type_t ttype = ompt_thread_type_get();
    if (ttype != ompt_thread_initial) return 1;

    if (state == ompt_state_work_serial) return 0;
    return 1;
  }
  return 0;
}


static int *
ompt_get_idle_count_ptr()
{
  return &ompt_idle_count;
}


//----------------------------------------------------------------------------
// identify whether a thread is an OpenMP thread or not
//----------------------------------------------------------------------------

static _Bool
ompt_thread_participates(void)
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
ompt_thread_needs_blame(void)
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
ompt_mutex_blame_shift_register(void)
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
ompt_register_mutex_metrics(void)
{
  int wait_id = omp_mutex_blame_info.wait_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(wait_id, "OMP_MUTEX_WAIT", 
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  int blame_id = omp_mutex_blame_info.blame_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(blame_id, "OMP_MUTEX_BLAME",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
}


static void
ompt_register_idle_metrics(void)
{
  int idle_metric_id = omp_idle_blame_info.idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(idle_metric_id, "OMP_IDLE",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  int work_metric_id = omp_idle_blame_info.work_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(work_metric_id, "OMP_WORK",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
}

static void
ompt_idle_blame_shift_register(void)
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
ompt_init_inquiry_fn_ptrs(ompt_function_lookup_t ompt_fn_lookup)
{
#define ompt_interface_fn(f) \
  f ## _fn = (f ## _t) ompt_fn_lookup(#f); 

FOREACH_OMPT_INQUIRY_FN(ompt_interface_fn)

#undef ompt_interface_fn
}


//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------

static
void ompt_task_begin(ompt_task_id_t parent_task_id, 
		   ompt_frame_t *parent_task_frame, 
		   ompt_task_id_t new_task_id,
		   void *task_function)
{
  uint64_t zero_metric_incr = 0LL;

  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead ++;

  ucontext_t uc;
  getcontext(&uc);

  hpcrun_safe_enter();

  // record the task creation context into task structure (in omp runtime)
  cct_node_t *cct_node =
    hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1).sample_node;

  hpcrun_safe_exit();

  task_map_insert(new_task_id, cct_node);  

  td->overhead --;
}


//----------------------------------------------------------------------------
// support for undirected blame shifting to attribute idleness
//----------------------------------------------------------------------------

//-------------------------------------------------
// note birth and death of threads to support 
// undirected blame shifting using the IDLE metric
//-------------------------------------------------

static void
ompt_thread_begin(ompt_thread_type_t ttype)
{
  ompt_thread_type_set(ttype);
  undirected_blame_thread_start(&omp_idle_blame_info);
}


static void
ompt_thread_end()
{
  undirected_blame_thread_end(&omp_idle_blame_info);
}


//-------------------------------------------------
// note the beginning and end of idleness to 
// support undirected blame shifting
//-------------------------------------------------

static void
ompt_idle_begin()
{
  undirected_blame_idle_begin(&omp_idle_blame_info);
}


static void
ompt_idle_end()
{
  undirected_blame_idle_end(&omp_idle_blame_info);
}


//-------------------------------------------------
// accept any blame accumulated for mutex while 
// this thread held it
//-------------------------------------------------

static void 
ompt_mutex_blame_accept(uint64_t mutex)
{
  directed_blame_accept(&omp_mutex_blame_info, mutex);
}


//----------------------------------------------------------------------------
// initialization of OMPT interface by setting up callbacks
//----------------------------------------------------------------------------

static void 
init_threads()
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_thread_begin, 
		    (ompt_callback_t)ompt_thread_begin);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_event_thread_end, 
		    (ompt_callback_t)ompt_thread_end);
  assert(ompt_event_may_occur(retval));
}


static void 
init_parallel_regions()
{
  ompt_parallel_region_register_callbacks(ompt_set_callback_fn);
}


static void 
init_tasks() 
{
  ompt_task_register_callbacks(ompt_set_callback_fn);
}


//-------------------------------------------------
// register callbacks to support directed blame
// shifting, namely attributing waiting for a mutex
// to the mutex holder at the point of release.
//-------------------------------------------------
static void 
init_mutex_blame_shift(const char *version)
{
  int mutex_blame_shift_avail = 0;
  int retval = 0;

  if (!ompt_mutex_blame_requested) return;

  retval = ompt_set_callback_fn(ompt_event_release_lock, 
		    (ompt_callback_t) ompt_mutex_blame_accept);
  mutex_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_nest_lock_last, 
		    (ompt_callback_t) ompt_mutex_blame_accept);
  mutex_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_critical, 
		    (ompt_callback_t) ompt_mutex_blame_accept);
  mutex_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_atomic, 
		    (ompt_callback_t) ompt_mutex_blame_accept);
  mutex_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_ordered, 
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
init_idle_blame_shift(const char *version)
{
  int idle_blame_shift_avail = 0;
  int retval = 0;

  if (!ompt_idle_blame_requested) return;

  retval = ompt_set_callback_fn(ompt_event_idle_begin, 
		    (ompt_callback_t)ompt_idle_begin);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_idle_end, 
				(ompt_callback_t)ompt_idle_end);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_wait_barrier_begin, 
				(ompt_callback_t)ompt_idle_begin);
  idle_blame_shift_avail |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_wait_barrier_end, 
				(ompt_callback_t)ompt_idle_end);
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

void
ompt_initialize(ompt_function_lookup_t ompt_fn_lookup,
                const char*            runtime_version,
                unsigned int           ompt_version)
{
  ompt_initialized = 1;

#if 0
  fprintf(stderr, "ompt_fn_lookup = %p\n", ompt_fn_lookup);
  fflush(NULL);
#endif

  ompt_init_inquiry_fn_ptrs(ompt_fn_lookup);

  ompt_init_placeholders(ompt_fn_lookup);

  init_threads();
  init_parallel_regions();
  init_mutex_blame_shift(runtime_version);
  init_idle_blame_shift(runtime_version);

  if(ENABLED(OMPT_TASK_FULL_CTXT)) {
    init_tasks();
  }

  if(!ENABLED(OMPT_KEEP_ALL_FRAMES)) {
    ompt_elide = 1;
    ompt_callstack_init();
  }
  if (getenv("HPCRUN_OMP_SERIAL_ONLY")) {
     serial_only_sf_entry.fn = ompt_serial_only;
     serial_only_sf_entry.arg = 0;
     sample_filters_register(&serial_only_sf_entry);
  }
}


ompt_initialize_t 
ompt_tool(void)
{
  return ompt_initialize;
}


int 
hpcrun_ompt_state_is_overhead()
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = hpcrun_ompt_get_state(&wait_id);

    switch (state) {
  
    case ompt_state_overhead:
    case ompt_state_wait_critical:
    case ompt_state_wait_lock:
    case ompt_state_wait_nest_lock:
    case ompt_state_wait_atomic:
    case ompt_state_wait_ordered:
      return 1;
  
    default: break;
    }
  }
  return 0;
}


//-------------------------------------------------
// returns true if OpenMP runtime frames should
// be elided out of thread callstacks
//-------------------------------------------------

int
hpcrun_ompt_elide_frames()
{
  return ompt_elide; 
}


//----------------------------------------------------------------------------
// safe (wrapped) versions of API functions that can be called from the rest
// of hpcrun, even if no OMPT support is available
//----------------------------------------------------------------------------

ompt_parallel_id_t 
hpcrun_ompt_get_parallel_id(int level)
{
  if (ompt_initialized) return ompt_get_parallel_id_fn(level);
  return 0;
}


ompt_state_t 
hpcrun_ompt_get_state(uint64_t *wait_id)
{
  if (ompt_initialized) return ompt_get_state_fn(wait_id);
  return ompt_state_undefined;
}


ompt_frame_t *
hpcrun_ompt_get_task_frame(int level)
{
  if (ompt_initialized) return ompt_get_task_frame_fn(level);
  return NULL;
}


ompt_task_id_t 
hpcrun_ompt_get_task_id(int level)
{
#ifndef OMPT_V2013_07
  if (ompt_initialized) return ompt_get_task_id_fn(level);
#endif
  return ompt_task_id_none;
}


void *
hpcrun_ompt_get_idle_frame()
{
#ifndef OMPT_V2013_07
  if (ompt_initialized) return ompt_get_idle_frame_fn();
#endif
  return NULL;
}


//-------------------------------------------------
// a special safe function that determines the
// outermost parallel region enclosing the current
// context.
//-------------------------------------------------

ompt_parallel_id_t
hpcrun_ompt_outermost_parallel_id()
{ 
  ompt_parallel_id_t outer_id = 0; 
  if (ompt_initialized) { 
    int i = 0;
    for (;;) {
      ompt_parallel_id_t next_id = ompt_get_parallel_id_fn(i++);
      if (next_id == 0) break;
      outer_id = next_id;
    }
  }
  return outer_id;
}


void 
ompt_mutex_blame_shift_request()
{
  ompt_mutex_blame_requested = 1;
  ompt_register_mutex_metrics();
}


void 
ompt_idle_blame_shift_request()
{
  ompt_idle_blame_requested = 1;
  ompt_register_idle_metrics();
}
