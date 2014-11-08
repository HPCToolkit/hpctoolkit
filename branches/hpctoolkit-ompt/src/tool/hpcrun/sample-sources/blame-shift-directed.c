// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

//
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <ucontext.h>
#include <pthread.h>
#include <string.h>
#include <dlfcn.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "blame-shift/blame-shift.h"
#include "idle.h"
#include "unresolved.h"

#include <hpcrun/cct/cct.h>
#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample-sources/blame-shift/blame-map.h>
#include <hpcrun/thread_data.h>



/******************************************************************************
 * macros
 *****************************************************************************/

#define DIRECTED_BLAME_NAME "OMP_MUTEX"



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_directed_blame_for_sample(void *arg, int metric_id, 
					      cct_node_t *node, int metric_incr);



/******************************************************************************
 * global variables
 *****************************************************************************/

static int directed_blame_self_metric_id = -1;
static int directed_blame_other_metric_id = -1;
static bs_fn_entry_t bs_entry;
static blame_entry_t* ompt_blame_table = NULL;

static int doblame = 0;


/***************************************************************************
 * private operations
 ***************************************************************************/


static inline void 
set_blame_target(thread_data_t *td, uint64_t obj)
{
  td->blame_target = obj;
}


static inline uint64_t  
get_blame_target(thread_data_t *td)
{
  return td->blame_target;
}

static void 
process_directed_blame_for_sample(void *arg, int metric_id, cct_node_t *node, 
				  int metric_incr)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);
 
  // Only blame shift idleness for time and cycle metrics. 
  if (!(metric_desc->properties.time | metric_desc->properties.cycles)) 
    return;

  uint64_t obj_to_blame = hpcrun_ompt_get_blame_target();
  if (obj_to_blame) {
    uint32_t metric_period = metric_desc->period;
    uint32_t metric_value = (uint32_t) (metric_period * metric_incr);
    blame_map_add_blame(ompt_blame_table, obj_to_blame, metric_value); 
    cct_metric_data_increment(directed_blame_self_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}


/*--------------------------------------------------------------------------
 | sample source methods
 --------------------------------------------------------------------------*/

static void
METHOD_FN(init)
{
  self->state = INIT;
  // blame_shift_target_allow();
  if (! ompt_blame_table) ompt_blame_table = blame_map_new();
}


static void
METHOD_FN(thread_init)
{
}


static void
METHOD_FN(thread_init_action)
{
}


static void
METHOD_FN(start)
{
   doblame = 1;
}


static void
METHOD_FN(thread_fini_action)
{
}


static void
METHOD_FN(stop)
{
}


static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, DIRECTED_BLAME_NAME) != NULL);
}

 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  bs_entry.fn = process_directed_blame_for_sample;
  bs_entry.next = NULL;
  bs_entry.arg = NULL;

  blame_shift_register(&bs_entry);

  directed_blame_self_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(directed_blame_self_metric_id, "OMP_MUTEX_WAIT", 
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  directed_blame_other_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(directed_blame_other_metric_id, "OMP_MUTEX_BLAME",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available OpenMP directed blame shifting preset events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tWhen waiting for an OpenMP mutex (i.e., a lock or critical\n"
         "\t\tsection), shift blame for waiting to code holding the mutex.\n"
         "\t\tOnly suitable for OpenMP programs.\n",
	 DIRECTED_BLAME_NAME);
  printf("\n");
}



/*--------------------------------------------------------------------------
 | sample source object
 --------------------------------------------------------------------------*/

#define ss_name ompt_directed_blame
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


static cct_node_t *
ompt_attribute_blame(ucontext_t *uc, int metric_id, int metric_incr, int skipcnt)
{
  cct_node_t *node = 
    hpcrun_sample_callpath(uc, metric_id, metric_incr, skipcnt, 1).sample_node;
  return node;
}



/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

#if 0
void
ompt_directed_blame_shift_start(uint64_t obj)
{
  thread_data_t *td = hpcrun_get_thread_data();
  set_blame_target(td, obj);
}


void
ompt_directed_blame_shift_end()
{
  thread_data_t *td = hpcrun_get_thread_data();
  set_blame_target(td, 0);
}
#endif


void
ompt_directed_blame_accept(uint64_t obj)
{
  uint64_t blame = blame_map_get_blame(ompt_blame_table, obj);
  if (blame != 0 && hpctoolkit_sampling_is_active()) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_safe_enter();
    ompt_attribute_blame(&uc, directed_blame_other_metric_id, blame, 1);
    hpcrun_safe_exit();
  }
}
