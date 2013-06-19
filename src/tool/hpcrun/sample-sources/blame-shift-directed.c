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
#include "blame-shift.h"
#include "idle.h"
#include "unresolved.h"

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>



/******************************************************************************
 * macros
 *****************************************************************************/

#define N (128*1024)
#define INDEX_MASK ((N)-1)

#define DIRECTED_BLAME_NAME "MUTEX"



/******************************************************************************
 * data type
 *****************************************************************************/

struct blame_parts {
  uint32_t obj_id;
  uint32_t blame;
};


typedef union {
  uint64_t all;
  struct blame_parts parts; //[0] is id, [1] is blame
} blame_entry;



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_directed_blame_for_sample(int metric_id, cct_node_t *node, 
	int metric_incr);



/******************************************************************************
 * global variables
 *****************************************************************************/

static int directed_blame_self_metric_id = -1;
static int directed_blame_other_metric_id = -1;
static bs_fn_entry_t bs_entry;
volatile blame_entry table[N];

static int doblame = 0;

static uint32_t metric_period = 0;

/***************************************************************************
 * private operations
 ***************************************************************************/


/*--------------------------------------------------------------------------
 | hash table for recording directed blame
 --------------------------------------------------------------------------*/

static void 
blameht_init()
{
  int i;
  for(i = 0; i < N; i++)
    table[i].all = 0ULL;
}


uint32_t 
blameht_obj_id(uint64_t obj)
{
  return ((uint32_t) ((uint64_t)obj) >> 2);
}


uint32_t 
blameht_hash(uint64_t obj) 
{
  return ((uint32_t)((blameht_obj_id(obj)) & INDEX_MASK));
}


uint64_t 
blameht_entry(uint64_t obj, uint32_t metric_value)
{
  blame_entry be;
  be.parts.obj_id = blameht_obj_id(obj);
  be.parts.blame = metric_value;
  return be.all;
}

#define LOSSLESS_BLAME 

void
blameht_add_blame(uint64_t obj, uint32_t metric_value)
{
  uint32_t obj_id = blameht_obj_id(obj);
  uint32_t index = blameht_hash(obj);

  assert(index >= 0 && index < N);

  for(;;) {
    blame_entry oldval = table[index];

    if(oldval.parts.obj_id == obj_id) {
#ifdef LOSSLESS_BLAME
      blame_entry newval = oldval;
      newval.parts.blame += metric_value;
      if (compare_and_swap_i64(&table[index].all, oldval.all, newval.all) 
	    == oldval.all) break;
#else
      oldval.parts.blame += metric_value;
      table[index].all = oldval.all;
#endif
      break;
    } else {
      if(oldval.parts.obj_id == 0) {
	uint64_t newval = blameht_entry(obj, metric_value);
#ifdef LOSSLESS_BLAME
	if (compare_and_swap_i64(&table[index].all, oldval.all, newval) 
	    == oldval.all) break;
	// otherwise, try again
#else
	table[index].all = newval;
	break;
#endif
      } else {
	EMSG("leaked blame %d\n", metric_value);
	// entry in use for another object's blame
	// in this case, since it isn't easy to shift 
	// our blame efficiently, we simply drop it.
	break;
      }
    }
  }
}


uint64_t 
blameht_get_blame(uint64_t obj)
{
  static uint64_t zero = 0;
  uint64_t val = 0;
  uint32_t obj_id = blameht_obj_id(obj);
  uint32_t index = blameht_hash(obj);

  assert(index >= 0 && index < N);

  for(;;) {
    blame_entry oldval = table[index];
    if(oldval.parts.obj_id == obj_id) {
#ifdef LOSSLESS_BLAME
      if (compare_and_swap_i64(&table[index].all, oldval.all, zero) 
	  != oldval.all) continue; // try again on failure
#else
      table[index].all = 0;
#endif
      val = (uint64_t)oldval.parts.blame;
      break;
    }
    break;
  }
  return val;
}


#if 0
/*--------------------------------------------------------------------------
 | transfer directed blame as appropritate for a sample
 --------------------------------------------------------------------------*/

set_blame_target()
{
  td->blame_target = get_blame_target();
}
#endif

static void 
process_directed_blame_for_sample(int metric_id, cct_node_t *node, int metric_incr)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);
 
  // Only blame shift idleness for time and cycle metrics. 
  if ( ! (metric_desc->properties.time | metric_desc->properties.cycles) ) 
    return;
  
  // FIXME: not good form to use a static here
  metric_period = metric_desc->period;

  uint32_t metric_value = (uint32_t) (metric_period * metric_incr);

  thread_data_t *td = hpcrun_get_thread_data();

  uint64_t obj_to_blame = td->blame_target;

  if(obj_to_blame) {
    blameht_add_blame(obj_to_blame, metric_incr); // save blame bits by not inflating with period
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
  blameht_init();
  blame_shift_target_allow();
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
  bs_entry.next = 0;

  blame_shift_register(&bs_entry, bs_type_directed);

  directed_blame_self_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(directed_blame_self_metric_id, "MUTEX_WAIT", 
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  directed_blame_other_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(directed_blame_other_metric_id, "MUTEX_BLAME",
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
  printf("Available directed blame shifting preset events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tShift the blame for waiting for a lock to the lock holder.\n"
	 "\t\tOnly suitable for threaded programs.\n",
	 DIRECTED_BLAME_NAME);
  printf("\n");
}



/*--------------------------------------------------------------------------
 | sample source object
 --------------------------------------------------------------------------*/

#define ss_name directed_blame
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// attribute cost

static cct_node_t *
attribute_blame(ucontext_t *uc, int metric_id, int metric_incr, int skipcnt)
{
  omp_arg_t omp_arg;
  void *info = NULL;
  cct_node_t *node;

  // for side thread or master thread in the nested region, unwind to the dummy root 
  // with outer most region attached to the tbd root
  //
  if (!TD_GET(master)) { // sub-master thread in nested regions
    omp_arg.tbd = false;
    omp_arg.context = NULL;
    if(TD_GET(region_id) > 0) {
      omp_arg.tbd = true;
      omp_arg.region_id = TD_GET(region_id);
    }
    info = &omp_arg;
  }
  node = hpcrun_sample_callpath(uc, metric_id, metric_incr, skipcnt, 1, info).sample_node;
  return node;
}



/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

void
directed_blame_shift_start(uint64_t obj)
{
  thread_data_t *td = hpcrun_get_thread_data();
  // if (td->blame_target == 0) idle_metric_blame_shift_idle();
  td->blame_target = obj;
}


void
directed_blame_shift_end()
{
  thread_data_t *td = hpcrun_get_thread_data();
  // if (td->blame_target) idle_metric_blame_shift_work();
  td->blame_target = 0;
}


void
directed_blame_accept(uint64_t obj)
{
  uint64_t blame = blameht_get_blame(obj);
  if (blame != 0 && hpctoolkit_sampling_is_active()) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_safe_enter();
    // hpcrun_sample_callpath(&uc, directed_blame_metric_id, blame, 0, 1);
    attribute_blame(&uc, directed_blame_other_metric_id, blame * metric_period, 1);
    hpcrun_safe_exit();
  }
}
