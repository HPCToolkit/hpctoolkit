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

#include <stdio.h>
#include <ompt.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include <hpcrun/cct/cct.h>

#include <hpcrun/ompt/ompt-callback.h>
#include <hpcrun/ompt/ompt-task.h>
#include <hpcrun/ompt/ompt-task-map.h>



//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------

static void 
ompt_task_begin_internal(
  ompt_task_id_t task_id
)
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

  ompt_task_map_insert(task_id, cct_node);  

  td->overhead --;
}
#ifdef OMPT_V2013_07
static ompt_task_id_t 
get_task_id()
{
  static ompt_task_id_t next_task_id = 1; 
  return __sync_fetch_and_add(&next_task_id, 1);
}
#endif

#ifdef OMPT_V2013_07
void
ompt_task_begin(
  ompt_data_t  *parent_task_data,   /* tool data for parent task    */
  ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
  ompt_data_t  *new_task_data       /* tool data for created task   */
)
{
  ompt_task_id_t new_task_id = get_task_id();
  new_task_data->value = new_task_id;
  ompt_task_begin_internal(new_task_id);
}
#else
void
ompt_task_begin(
  ompt_task_id_t parent_task_id, 
  ompt_frame_t *parent_task_frame, 
  ompt_task_id_t new_task_id,
  void *task_function
)
{
  ompt_task_begin_internal(new_task_id);
}
#endif


void
ompt_task_register_callbacks(
  ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_task_begin, 
		    (ompt_callback_t)ompt_task_begin);
  assert(ompt_event_may_occur(retval));
}
