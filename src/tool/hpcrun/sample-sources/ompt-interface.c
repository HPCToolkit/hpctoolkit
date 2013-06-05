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

#include "idle.h"
#include "lockwait.h"

#include "utilities/defer-cntxt.h"
#include "utilities/task-cntxt.h"



/******************************************************************************
 * private operations 
 *****************************************************************************/

static void 
init_threads()
{
  ompt_set_callback(ompt_event_thread_create, 
		    (ompt_callback_t)start_fn);

  ompt_set_callback(ompt_event_thread_exit, 
		    (ompt_callback_t)end_fn);
}


static void 
init_parallel_regions()
{
  ompt_set_callback(ompt_event_parallel_create, 
		    (ompt_callback_t)start_team_fn);

  ompt_set_callback(ompt_event_parallel_exit, 
		    (ompt_callback_t)end_team_fn);
}


static void 
init_tasks() 
{
  ompt_set_callback(ompt_event_task_create, 
		    (ompt_callback_t)start_task_fn);
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
  ompt_set_callback(ompt_event_release_lock, 
		    (ompt_callback_t)(unlock_fn1));

  ompt_set_callback(ompt_event_release_nest_lock_last, 
		    (ompt_callback_t)(unlock_fn1));

  ompt_set_callback(ompt_event_release_critical, 
		    (ompt_callback_t)(unlock_fn1));

  ompt_set_callback(ompt_event_release_atomic, 
		    (ompt_callback_t)(unlock_fn1));

  ompt_set_callback(ompt_event_release_ordered, 
		    (ompt_callback_t)(unlock_fn1));
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
  ompt_set_callback(ompt_event_idle_begin, 
		    (ompt_callback_t)idle_fn);

  ompt_set_callback(ompt_event_idle_end, 
		    (ompt_callback_t)work_fn);

  ompt_set_callback(ompt_event_wait_barrier_begin, 
		    (ompt_callback_t)bar_wait_begin);

  ompt_set_callback(ompt_event_wait_barrier_end, 
		    (ompt_callback_t)bar_wait_end);
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void 
ompt_initialize()
{
  init_threads();
  init_parallel_regions();
  init_tasks();
  init_blame_shift_undirected();
  init_blame_shift_directed();
}


int 
ompt_state_is_overhead()
{
  ompt_wait_id_t wait_id;
  ompt_get_state(&wait_id);

  switch (wait_id) {

  case ompt_state_overhead:
  case ompt_state_wait_critical:
  case ompt_state_wait_lock:
  case ompt_state_wait_nest_lock:
  case ompt_state_wait_atomic:
  case ompt_state_wait_ordered:
    return 1;

  default: break;

  }

  return 0;
}

