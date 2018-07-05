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
#include <hpcrun/ompt/ompt-region-map.h>

#include <ompt-callstack.h>

#include "ompt-interface.h"
#include "ompt-region.h"


//----------------------------------------------------------------------------
// note the creation context for an OpenMP task
//----------------------------------------------------------------------------


static void 
ompt_task_begin_internal(
  ompt_data_t* task_data
)
{
  uint64_t zero_metric_incr = 0LL;

  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead ++;

  ucontext_t uc; 
  getcontext(&uc);

  hpcrun_safe_enter();

  // record the task creation context into task structure (in omp runtime)
  cct_node_t *cct_node = NULL;
  if(ompt_task_full_context){
    // FIXME vi3: Change according to new signature of hpcrun_sample_callpath
    // vi3 old version
    // cct_node = hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1).sample_node;
    // vi3 new version
    hpcrun_metricVal_t zero_metric_incr_metricVal;
    zero_metric_incr_metricVal.i = 0;
    cct_node = hpcrun_sample_callpath(&uc, 0, zero_metric_incr_metricVal, 1, 1, NULL).sample_node;
  }
  else{

//    thread_data_t *td = hpcrun_get_thread_data();


    ompt_data_t *parallel_info = NULL;
    int team_size = 0;
    hpcrun_ompt_get_parallel_info(0, &parallel_info, &team_size);
    ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_info->ptr;
    cct_node_t* prefix = region_data->call_path;

#if 0
    cct_node = hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1).sample_node;

    if(!TD_GET(master)){
      prefix = hpcrun_cct_insert_path_return_leaf(
        td->core_profile_trace_data.epoch->csdata.tree_root,
        prefix); 
    }


    cct_node = hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1).sample_node;
    while(cct_node->parent != prefix && cct_node->parent != NULL) cct_node = cct_node->parent;

    if(!cct_node->parent) assert(0);
#else
    cct_node = prefix;
#endif

//
//    cct_node = prefix;
  }

  hpcrun_safe_exit();


  task_data->ptr = cct_node;

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
ompt_task_create(
    ompt_data_t *parent_task_data,    /* data of parent task          */
    const ompt_frame_t *parent_frame, /* frame data for parent task   */
    ompt_data_t *new_task_data,       /* data of created task         */
    ompt_task_type_t type,
    int has_dependences,
    const void *codeptr_ra
)
{
  new_task_data->ptr = NULL;

  if(type == ompt_task_initial) return;
  ompt_task_begin_internal(new_task_data);
}

#endif

void
ompt_task_register_callbacks(
    ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_callback_task_create,
                                (ompt_callback_t)ompt_task_create);
  assert(ompt_event_may_occur(retval));
}


