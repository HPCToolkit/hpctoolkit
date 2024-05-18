// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <stdio.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../safe-sampling.h"
#include "../sample_event.h"
#include "../thread_data.h"

#include "omp-tools.h"

#include "ompt-callback.h"
#include "ompt-interface.h"
#include "ompt-region.h"
#include "ompt-task.h"



//*****************************************************************************
// private operations
//*****************************************************************************

//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------
static void
ompt_task_begin_internal
(
 ompt_data_t* task_data
)
{
  bool unwind_here = false;
  thread_data_t *td = hpcrun_get_thread_data();

  td->overhead ++;

  // record the task creation context into task structure (in omp runtime)
  cct_node_t *cct_node = NULL;
  if (ompt_task_full_context_p()) {
    unwind_here = true;
  } else {
    ompt_data_t *parallel_info = NULL;
    int team_size = 0;
    hpcrun_ompt_get_parallel_info(0, &parallel_info, &team_size);
    ompt_region_data_t* region_data = (ompt_region_data_t*) parallel_info->ptr;

    if (region_data) {
      // has enclosing parallel region with initialized parallel_data:
      // the task context is the region callpath
      cct_node = region_data->call_path;
    } else {
      // has enclosing parallel region with uninitialized parallel_data:
      // this happens when a task is created for a target nowait when only
      // enclosed by the implicit parallel region for the implicit task, for
      // which the value of parallel_data is NULL (specifically, it has a
      // parallel data pointer, but there was no parallel begin so the data
      // is uninitialized
      unwind_here = true;
    }
  }

  if (unwind_here) {
    ucontext_t uc;
    getcontext(&uc);

    hpcrun_metricVal_t zero_metric_incr_metricVal;
    zero_metric_incr_metricVal.i = 0;
    cct_node = hpcrun_sample_callpath
      (&uc, 0, zero_metric_incr_metricVal, 1, 1, NULL).sample_node;
  }

  task_data->ptr = cct_node;

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
 ompt_task_flag_t type,
 int has_dependences,
 const void *codeptr_ra
)
{
  hpcrun_safe_enter();

  new_task_data->ptr = NULL;

  if (type != ompt_task_initial) {
    ompt_task_begin_internal(new_task_data);
  }

  hpcrun_safe_exit();
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void
ompt_task_register_callbacks
(
 ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_callback_task_create,
                                (ompt_callback_t)ompt_task_create);
  if (!ompt_event_may_occur(retval))
    hpcrun_terminate();  // Insufficient OMPT support
}
