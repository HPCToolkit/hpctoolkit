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

#include "sample-sources/idle.h"
#include "sample-sources/lockwait.h"
#include "sample-sources/blame-shift.h"
#include "sample-sources/blame-shift-directed.h"

#include "utilities/defer-cntxt.h"
#include "utilities/task-cntxt.h"

int ompt_elide = 0;
int ompt_initialized = 0;


/******************************************************************************
 * private operations 
 *****************************************************************************/

#if 0
void ompt_sync_sample()
{
  thread_data_t *td = hpcrun_get_thread_data();
  ompt_data_t arg;
  arg.tbd = true;
  arg.region_id = 
  record_synchronous_sample(td, n, );
}
#endif


static uint64_t
ompt_get_blame_target()
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = ompt_get_state(&wait_id);

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

  
static void
ompt_thread_create(ompt_data_t *data)
{
  idle_metric_thread_start();
}

static void
ompt_thread_exit(ompt_data_t *data)
{
  idle_metric_thread_end();
}


static void 
init_threads()
{
  int retval;
  retval = ompt_set_callback(ompt_event_thread_create, 
		    (ompt_callback_t)ompt_thread_create);
  assert(retval > 0);

  retval = ompt_set_callback(ompt_event_thread_exit, 
		    (ompt_callback_t)ompt_thread_exit);
  assert(retval > 0);
}


static void 
init_parallel_regions()
{
  int retval;
  retval = ompt_set_callback(ompt_event_parallel_create, 
		    (ompt_callback_t)start_team_fn);
  assert(retval > 0);

  retval = ompt_set_callback(ompt_event_parallel_exit, 
		    (ompt_callback_t)end_team_fn);
  assert(retval > 0);
}


static void 
init_tasks() 
{
  int retval;
  retval = ompt_set_callback(ompt_event_task_create, 
		    (ompt_callback_t)start_task_fn);
  assert(retval > 0);
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
  static bs_tfn_entry_t entry;

  int retval = 0;

  retval |= ompt_set_callback(ompt_event_release_lock, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback(ompt_event_release_nest_lock_last, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback(ompt_event_release_critical, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback(ompt_event_release_atomic, 
		    (ompt_callback_t) directed_blame_accept);

  retval |= ompt_set_callback(ompt_event_release_ordered, 
		    (ompt_callback_t) directed_blame_accept);

  if (retval) {
    entry.fn = ompt_get_blame_target;
    entry.next = NULL;
    blame_shift_target_register(&entry);
  }
}

  
static void
ompt_idle_begin(ompt_data_t *data)
{
  idle_metric_blame_shift_idle();
}


static void
ompt_idle_end(ompt_data_t *data)
{
  idle_metric_blame_shift_work();
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
  int retval = 0;
  retval |= ompt_set_callback(ompt_event_idle_begin, 
		    (ompt_callback_t)ompt_idle_begin);

  retval |= ompt_set_callback(ompt_event_idle_end, 
		    (ompt_callback_t)ompt_idle_end);

  retval |= ompt_set_callback(ompt_event_wait_barrier_begin, 
		    (ompt_callback_t)ompt_idle_begin);

  retval |= ompt_set_callback(ompt_event_wait_barrier_end, 
		    (ompt_callback_t)ompt_idle_end);

  if (retval) {
    idle_metric_register_blame_source();
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

int 
ompt_initialize(void)
{
  ompt_initialized = 1;

  init_threads();
  init_parallel_regions();
  init_blame_shift_undirected();
  init_blame_shift_directed();

  if(ENABLED(OMPT_TASK_FULL_CTXT)) {
    init_tasks();
    task_cntxt_full();
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
    ompt_state_t state = ompt_get_state(&wait_id);

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
      ompt_parallel_id_t next_id = ompt_get_parallel_id(i++);
      if (next_id == 0) break;
      outer_id = next_id;
    }
  }
  return outer_id;
}


ompt_parallel_id_t 
hpcrun_ompt_get_parallel_id(int level)
{
  if (ompt_initialized) return ompt_get_parallel_id(level);
  return 0;
}


ompt_state_t 
hpcrun_ompt_get_state(uint64_t *wait_id)
{
  if (ompt_initialized) return ompt_get_state(wait_id);
  return ompt_state_undefined;
}


ompt_frame_t *
hpcrun_ompt_get_task_frame(int level)
{
  if (ompt_initialized) return ompt_get_task_frame(level);
  return NULL;
}


ompt_data_t *
hpcrun_ompt_get_task_data(int level)
{
  if (ompt_initialized) return ompt_get_task_data(level);
  return NULL;
}

ompt_data_t *
hpcrun_ompt_get_thread_data()
{
  if (ompt_initialized) return ompt_get_thread_data();
  return NULL;
}


void *
hpcrun_ompt_get_thread_idle_frame()
{
  if (ompt_initialized) return ompt_get_thread_idle_frame();
  return NULL;
}


uint64_t
hpcrun_ompt_get_blame_target()
{
  if (ompt_initialized) {
    ompt_wait_id_t wait_id;
    ompt_state_t state = ompt_get_state(&wait_id);

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
