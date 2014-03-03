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
#include <stdbool.h>

//***** DBG
#include <stdio.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "pthread-blame.h"
#include <sample-sources/blame-shift/blame-shift.h>
#include <sample-sources/blame-shift/blame-map.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <messages/messages.h>

// typedefs
//
//

typedef enum {
  Running,
  Spinning,
  Blocked,
} state_t;

typedef struct {
  uint64_t target;
  state_t state;
} blame_t;

// *****************************************************************************
//    static local variables
// *****************************************************************************

static int blame_metric_id = -1;
static int blockwait_metric_id = -1;
static int spinwait_metric_id = -1;

static bs_fn_entry_t bs_entry;

static bool lockwait_enabled = false;

static blame_entry_t* pthread_blame_table = NULL;

static bool metric_id_set = false;

// *** DBG
typedef struct dbg_t {
  struct timeval tv;
  char l[4]; // "add" or "get"
  uint32_t amt;
  uint64_t obj;
} dbg_t;

typedef struct dbg_tr_t {
  unsigned n_elts;
  dbg_t trace[3000];
} dbg_tr_t;

//
// thread-local variables
//

static __thread blame_t pthread_blame = {0, Running};
static __thread dbg_tr_t p_dbg = {.n_elts = 0};

static inline
uint64_t
get_blame_target(void)
{
  return pthread_blame.target;
}

/***************************************************************************
 * private operations
 ***************************************************************************/
static inline
char*
state2str(state_t s)
{
  if (s == Running) return "Running";
  if (s == Spinning) return "Spinning";
  if (s == Blocked) return "Blocked";
  return "????";
}

static inline
int
get_blame_metric_id(void)
{
  return (metric_id_set) ? blame_metric_id : -1;
}

/*--------------------------------------------------------------------------
 | transferp directed blame as appropritate for a sample
 --------------------------------------------------------------------------*/

static inline
void
add_blame(uint64_t obj, uint32_t value)
{
  if (! pthread_blame_table) {
    EMSG("Attempted to add pthread blame before initialization");
    return;
  }
  blame_map_add_blame(pthread_blame_table,
		      obj, value);
}

static inline
uint64_t
get_blame(uint64_t obj)
{
  if (! pthread_blame_table) {
    EMSG("Attempted to fetch pthread blame before initialization");
    return 0;
  }
  return blame_map_get_blame(pthread_blame_table, obj);
}

// *** DBG **** temporary declaration, remove when debuggind done
extern void hpcrun_metric_std_inc1(int metric_id, metric_set_t* set,
				   hpcrun_metricVal_t incr);
static void 
process_directed_blame_for_sample(void* arg, int metric_id, cct_node_t* node, int metric_incr)
{
  TMSG(LOCKWAIT, "Processing directed blame");
  metric_desc_t* metric_desc = hpcrun_id2metric(metric_id);
 
#ifdef LOCKWAIT_FIX
  // Only blame shift idleness for time and cycle metrics. 
  if ( ! (metric_desc->properties.time | metric_desc->properties.cycles) ) 
    return;
#endif // LOCKWAIT_FIX
  
  uint32_t metric_value = (uint32_t) (metric_desc->period * metric_incr);

  uint64_t obj_to_blame = get_blame_target();
  if(obj_to_blame) {
    TMSG(LOCKWAIT, "about to add %d to blame object %d", metric_incr, obj_to_blame);
    add_blame(obj_to_blame, metric_incr);
    // update appropriate wait metric as well
    int wait_metric = (pthread_blame.state == Blocked) ? blockwait_metric_id : spinwait_metric_id;
    TMSG(LOCKWAIT, "about to add %d to %s-waiting in node %d",
	 metric_incr, state2str(pthread_blame.state),
	 hpcrun_cct_persistent_id(node));
    metric_set_t* metrics = hpcrun_reify_metric_set(node);
    hpcrun_metric_std_inc1(wait_metric,
			   metrics,
			   (cct_metric_data_t) {.i = metric_incr});
  }
}

// ******************************************************************************
//  public interface to local variables
// ******************************************************************************

bool
pthread_blame_lockwait_enabled(void)
{
  return lockwait_enabled;
}

void
inc_p_dbg(const char* tag, uint64_t obj, uint32_t val)
{
  unsigned idx = p_dbg.n_elts++;
  p_dbg.trace[idx] = (dbg_t) {.amt = val, .obj = obj};
  strncpy(&(p_dbg.trace[idx].l[0]), tag, 4);
  gettimeofday(&(p_dbg.trace[idx].tv), NULL);
}

//
// ***** Write out dbg info to file
//
void
pthread_write_debug(void)
{
  int id = TD_GET(core_profile_trace_data.id);
  char fn[100] = {'\0'};
  snprintf(fn, sizeof(fn) - 1, "thread-%02d.dbg", id);
  FILE* f = fopen(fn, "w"); // "wb" mode ?
  // fprintf(f, "%d ops\n", get_p_dbg());   // fwrite() binary data ?
  fprintf(f, "%d ops\n", p_dbg.n_elts);
  for(unsigned i = 0; i < p_dbg.n_elts; i++) {
    fprintf(f, "%ld,%ld %s %08lx %d\n",
	    p_dbg.trace[i].tv.tv_sec, p_dbg.trace[i].tv.tv_usec,
	    p_dbg.trace[i].l, p_dbg.trace[i].obj, p_dbg.trace[i].amt);
  }
  fclose(f);
}

//
// public blame manipulation functions
//
void
pthread_directed_blame_shift_blocked_start(void* obj)
{
  TMSG(LOCKWAIT, "Start directed blaming using blame structure %x, for obj %d",
       &pthread_blame, (uintptr_t) obj);
  pthread_blame = (blame_t) {.target = (uint64_t)(uintptr_t)obj,
                             .state   = Blocked};
}

void
pthread_directed_blame_shift_spin_start(void* obj)
{
  TMSG(LOCKWAIT, "Start directed blaming using blame structure %x, for obj %d",
       &pthread_blame, (uintptr_t) obj);
  pthread_blame = (blame_t) {.target = (uint64_t)(uintptr_t)obj,
                             .state   = Spinning};
}

void
pthread_directed_blame_shift_end(void)
{
  pthread_blame = (blame_t) {.target = 0, .state = Running};
  TMSG(LOCKWAIT, "End directed blaming for blame structure %x",
       &pthread_blame);
}

void
pthread_directed_blame_accept(void* obj)
{
  uint64_t blame = get_blame((uint64_t) (uintptr_t) obj);
  TMSG(LOCKWAIT, "Blame obj %d accepting %d units of blame", obj, blame);
  if (blame && hpctoolkit_sampling_is_active()) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_safe_enter();
    hpcrun_sample_callpath(&uc, get_blame_metric_id(),
                           blame, 0, 1);
    hpcrun_safe_exit();
  }
}

/*--------------------------------------------------------------------------
 | sample source methods
 --------------------------------------------------------------------------*/

static void
METHOD_FN(init)
{
  self->state = INIT;
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
  lockwait_enabled = true;
  TMSG(LOCKWAIT, "pthread blame ss STARTED, blame table = %x", pthread_blame_table);
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
  lockwait_enabled = false;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, PTHREAD_EVENT_NAME) != NULL);
}

 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  bs_entry.fn = process_directed_blame_for_sample;
  bs_entry.arg = NULL;
  bs_entry.next = NULL;

  blame_shift_register(&bs_entry);

  blame_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(blame_metric_id, PTHREAD_BLAME_METRIC,
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  blockwait_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(blockwait_metric_id, PTHREAD_BLOCKWAIT_METRIC,
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  spinwait_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(spinwait_metric_id, PTHREAD_SPINWAIT_METRIC,
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  metric_id_set = true;

  // create & initialize blame table (once per process)
  if (! pthread_blame_table) pthread_blame_table = blame_map_new();
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
	 PTHREAD_EVENT_NAME);
  printf("\n");
}



/*--------------------------------------------------------------------------
 | sample source object
 --------------------------------------------------------------------------*/

#include <stdio.h>

#define ss_name directed_blame
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"
