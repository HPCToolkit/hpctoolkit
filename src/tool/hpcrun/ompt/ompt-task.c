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
// Copyright ((c)) 2002-2020, Rice University
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


//*****************************************************************************
// system includes 
//*****************************************************************************

#include <stdio.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include "omp-tools.h"

#include "ompt-callback.h"
#include "ompt-interface.h"
#include "ompt-region.h"
#include "ompt-task.h"


struct ompt_task_data_t {
  ompt_task_flag_t task_type;
  cct_node_t *callpath;
  struct ompt_task_data_t *next;
}; 

static __thread ompt_task_data_t *task_free_list = 0;


ompt_task_flag_t
ompt_task_type
(
 ompt_data_t *task_data
);


//*****************************************************************************
// private operations
//*****************************************************************************

static void 
ompt_task_init
(
 ompt_task_data_t *t,
 cct_node_t *callpath,
 ompt_task_flag_t task_type
)
{
  t->task_type = task_type;
  t->callpath = callpath;
  t->next = 0;
}

static ompt_task_data_t* 
ompt_task_alloc
(
 void
)
{
  ompt_task_data_t* t = (ompt_task_data_t*) hpcrun_malloc(sizeof(ompt_task_data_t));
  return t;
}


static ompt_task_data_t* 
ompt_task_freelist_get
(
 void
)
{
  ompt_task_data_t* t = 0; 
  if (task_free_list) {
    t = task_free_list;
    task_free_list = task_free_list->next;
  }
  return t;
}


static void
ompt_task_freelist_put
(
 ompt_task_data_t *t 
)
{
  if (t) {
    t->next = task_free_list;
    task_free_list = t;
  }
}


ompt_task_data_t*
ompt_task_acquire
(
 cct_node_t *callpath,
 ompt_task_flag_t task_type
)
{
  ompt_task_data_t* t = ompt_task_freelist_get();
  if (t == 0) {
    t = ompt_task_alloc();
  }
  ompt_task_init(t,callpath, task_type);
  return t;
}


void
ompt_task_release
(
 ompt_data_t *t
)
{
  ompt_task_freelist_put((ompt_task_data_t *) t->ptr);
  t->ptr = 0;
}


//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------
static void 
ompt_task_begin_internal
(
 ompt_data_t* task_data,
 int flags
)
{
  thread_data_t *td = hpcrun_get_thread_data();

  td->overhead ++;

  // record the task creation context into task structure (in omp runtime)
  cct_node_t *cct_node = NULL;
  if (ompt_task_full_context_p()){
    ucontext_t uc; 
    getcontext(&uc);

    hpcrun_metricVal_t zero_metric_incr_metricVal;
    zero_metric_incr_metricVal.i = 0;
    cct_node = hpcrun_sample_callpath(&uc, 0, zero_metric_incr_metricVal, 1, 1, NULL).sample_node;
  } else {
    ompt_data_t *parallel_info = NULL;
    int team_size = 0;
    hpcrun_ompt_get_parallel_info(0, &parallel_info, &team_size);
    ompt_region_data_t* region_data = (ompt_region_data_t*) parallel_info->ptr;
    cct_node = region_data->call_path;
  }

  task_data->ptr = ompt_task_acquire(cct_node, ompt_task_explicit);

  if (task_data->ptr == NULL) {
    // this says that we have explicit task but not it's path
    // we use this in elider (clip frames above the frame of the explicit task) and ompt_cursor_finalize
    // only possible when ompt_eager_context == false,
    // because we don't collect call path for the innermost region eagerly
    task_data->ptr = task_data_invalid;
  }

  td->overhead --;
}


static void
ompt_task_create
(
 ompt_data_t *parent_task_data,    // data of parent task
 const ompt_frame_t *parent_frame, // frame data for parent task
 ompt_data_t *new_task_data,       // data of created task
 int flags,
 int has_dependences,
 const void *codeptr_ra
)
{
  hpcrun_safe_enter();

  new_task_data->ptr = NULL;

  if (flags != ompt_task_initial) {
    ompt_task_begin_internal(new_task_data, flags);
  }

  hpcrun_safe_exit();
}


static void
ompt_task_schedule
(
 ompt_data_t *prior_task_data,    
 ompt_task_status_t prior_task_status, 
 ompt_data_t *next_task_data    
)
{
  hpcrun_safe_enter();

  if ((ompt_task_type(prior_task_data) & ompt_task_implicit) &&
      (ompt_task_type(next_task_data) & ompt_task_explicit)) {
    ompt_idle_end();
  }

  if ((ompt_task_type(prior_task_data) & ompt_task_explicit) &&
      (ompt_task_type(next_task_data) & ompt_task_implicit)) {
    ompt_idle_begin();
  }


  if (prior_task_status == ompt_task_complete) {
    ompt_task_release(prior_task_data);
  }
  hpcrun_safe_exit();
}



//*****************************************************************************
// interface operations
//*****************************************************************************

cct_node_t *
ompt_task_callpath
(
 ompt_data_t *task_data
)
{
  ompt_task_data_t *t = (ompt_task_data_t *) task_data->ptr;
  return t ? t->callpath : 0;
}


ompt_task_flag_t
ompt_task_type
(
 ompt_data_t *task_data
)
{
  ompt_task_data_t *t = (ompt_task_data_t *) task_data->ptr;
  return t ? t->task_type : 0;
}


void
ompt_task_register_callbacks
(
 ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_callback_task_create,
                                (ompt_callback_t)ompt_task_create);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_callback_task_schedule,
                                (ompt_callback_t)ompt_task_schedule);
  assert(ompt_event_may_occur(retval));
}


