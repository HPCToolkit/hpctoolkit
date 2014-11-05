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
 * system includes
 *****************************************************************************/

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
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>

#include "ompt-interface.h"

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
static blame_entry_t* ompt_blame_table = NULL;


//-----------------------------------------
// declare ompt interface function pointers
//-----------------------------------------
#define ompt_interface_fn(f) f ## _t f ## _fn;
FOREACH_OMPT_FN(ompt_interface_fn)
#undef ompt_interface_fn


/*--------------------------------------------------------------------------*
 | transfer directed blame as appropriate for a sample			    |
 *--------------------------------------------------------------------------*/

static inline
void
add_blame(uint64_t obj, uint32_t value)
{
  if (!ompt_blame_table) {
    EMSG("Attempted to add pthread blame before initialization");
    return;
  }
  blame_map_add_blame(ompt_blame_table, obj, value);
}


static inline
uint64_t
get_blame(uint64_t obj)
{
  if (!ompt_blame_table) {
    EMSG("Attempted to fetch pthread blame before initialization");
    return 0;
  }
  return blame_map_get_blame(ompt_blame_table, obj);
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
ompt_thread_participates(void)
{
  uint64_t wait_id;
  return hpcrun_ompt_get_state(&wait_id) != ompt_state_undefined;
}


static
void start_task_fn(ompt_task_id_t parent_task_id, 
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


#if 0
static uint64_t
ompt_get_blame_target()
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
#endif

  
static void
ompt_thread_create()
{
  idle_metric_thread_start();
}


static void
ompt_thread_exit()
{
  idle_metric_thread_end();
}


static void 
init_threads()
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_thread_begin, 
		    (ompt_callback_t)ompt_thread_create);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_event_thread_end, 
		    (ompt_callback_t)ompt_thread_exit);
  assert(ompt_event_may_occur(retval));
}


static void 
init_parallel_regions()
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_parallel_begin, 
		    (ompt_callback_t)start_team_fn);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_event_parallel_end, 
		    (ompt_callback_t)end_team_fn);
  assert(ompt_event_may_occur(retval));
}


static void 
init_tasks() 
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_task_begin, 
		    (ompt_callback_t)start_task_fn);
  assert(ompt_event_may_occur(retval));
}


#if 0
//------------------------------------------------------------------------------
// function:
//   init_blame_shift_directed()
//
// description:
//   register functions that will employ directed blame shifting 
//   to attribute idleness caused while awaiting mutual exclusion 
//------------------------------------------------------------------------------
static void 
init_blame_shift_directed()
{
  static bs_tfn_entry_t entry;

  int retval = 0;

  retval |= ompt_set_callback_fn(ompt_event_release_lock, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback_fn(ompt_event_release_nest_lock_last, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback_fn(ompt_event_release_critical, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback_fn(ompt_event_release_atomic, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback_fn(ompt_event_release_ordered, 
		    (ompt_callback_t) directed_blame_accept);

  if (retval) {
    entry.fn = ompt_get_blame_target;
    entry.next = NULL;
    blame_shift_target_register(&entry);
  }
}
#endif
  

static void
ompt_idle_begin()
{
  idle_metric_blame_shift_idle();
}


static void
ompt_idle_end()
{
  idle_metric_blame_shift_work();
}


static void 
ompt_wait_callback(ompt_wait_id_t wait_id)
{
  uint64_t blame = get_blame((uint64_t) wait_id );
  
}


//------------------------------------------------------------------------------
// function:
//   init_blame_shift_directed()
//
// description:
//   register functions that will employ directed blame shifting 
//   to attribute idleness caused while awaiting mutual exclusion 
//------------------------------------------------------------------------------
static void 
init_blame_shift_directed()
{
  int blame_shift_init = 0;
  int retval = 0;

  retval = ompt_set_callback_fn(ompt_event_release_lock, 
		    (ompt_callback_t) ompt_wait_callback);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_nest_lock_last, 
		    (ompt_callback_t) ompt_wait_callback);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_critical, 
		    (ompt_callback_t) ompt_wait_callback);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_atomic, 
		    (ompt_callback_t) ompt_wait_callback);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_release_ordered, 
		    (ompt_callback_t) ompt_wait_callback);
  blame_shift_init |= ompt_event_may_occur(retval);

  if (blame_shift_init) {
    // create & initialize blame table (once per process)
    if (!ompt_blame_table) ompt_blame_table = blame_map_new();
  }
}


//------------------------------------------------------------------------------
// function:
//   init_blame_shift_undirected()
//
// description:
//   register functions that will employ undirected blame shifting 
//   to attribute idleness 
//------------------------------------------------------------------------------
static void 
init_blame_shift_undirected()
{
  int blame_shift_init = 0;
  int retval = 0;
  retval = ompt_set_callback_fn(ompt_event_idle_begin, 
		    (ompt_callback_t)ompt_idle_begin);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_idle_end, 
				(ompt_callback_t)ompt_idle_end);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_wait_barrier_begin, 
				(ompt_callback_t)ompt_idle_begin);
  blame_shift_init |= ompt_event_may_occur(retval);

  retval = ompt_set_callback_fn(ompt_event_wait_barrier_end, 
				(ompt_callback_t)ompt_idle_end);
  blame_shift_init |= ompt_event_may_occur(retval);

  if (blame_shift_init) {
    idle_metric_register_blame_source(ompt_thread_participates);
  }
}


static void 
init_function_pointers(ompt_function_lookup_t ompt_fn_lookup)
{
#define ompt_interface_fn(f) \
  f ## _fn = (f ## _t) ompt_fn_lookup(#f); \
  assert(f ##_fn != 0);

FOREACH_OMPT_FN(ompt_interface_fn)

#undef ompt_interface_fn

}

//*****************************************************************************
// interface operations
//*****************************************************************************

int 
ompt_initialize(
  ompt_function_lookup_t ompt_fn_lookup,
  const char *runtime_version,
  int ompt_version
)
{
  ompt_initialized = 1;

  init_function_pointers(ompt_fn_lookup);
  init_threads();
  init_parallel_regions();
  init_blame_shift_undirected();
  init_blame_shift_directed();

  if(ENABLED(OMPT_TASK_FULL_CTXT)) {
    init_tasks();
  }

  if(!ENABLED(OMPT_KEEP_ALL_FRAMES)) {
    ompt_elide = 1;
  }
  return 1;
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


int
hpcrun_ompt_elide_frames()
{
  return ompt_elide; 
}


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
  if (ompt_initialized) return ompt_get_task_id_fn(level);
  return ompt_task_id_none;
}


void *
hpcrun_ompt_get_idle_frame()
{
  if (ompt_initialized) return ompt_get_idle_frame_fn();
  return NULL;
}


uint64_t
hpcrun_ompt_get_blame_target()
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
