// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-omp/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 3691 2012-03-05 21:21:08Z xl10 $
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
// PAPI sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "blame-shift.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/cct2metrics.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <omp.h>
#include "/home/xl10/support/gcc-4.6.2/libgomp/libgomp_g.h"
#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>
#include <utilities/defer-cntxt.h>
#include <hpcrun/unresolved.h>

/******************************************************************************
 * macros
 *****************************************************************************/

/******************************************************************************
 * forward declarations 
 *****************************************************************************/
static void lock_fn(void *lock);
static void unlock_fn(void *lock);

static void process_lockwait_blame_for_sample(cct_node_t *node, int metric_value);

/******************************************************************************
 * local variables
 *****************************************************************************/
static int lockwait_metric_id = -1;

static bs_fn_entry_t bs_entry;

static int period = 0;

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

// Return true if LOCKWAIT recognizes the name
static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, "LOCKWAIT") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
        bs_entry.fn = process_lockwait_blame_for_sample;
        bs_entry.next = 0;

	GOMP_lock_fn_register(lock_fn, unlock_fn);

	blame_shift_register(&bs_entry);

	lockwait_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(lockwait_metric_id, "LOCKWAIT",
			MetricFlags_ValFmt_Int, 1);
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available idle preset events\n");
  printf("===========================================================================\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name lockwait
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"

/******************************************************************************
 * blame samples
 *****************************************************************************/

void process_lockwait_blame_for_sample(cct_node_t *node, int metric_value)
{
  if(period == 0) period = metric_value;
  thread_data_t *td = hpcrun_get_thread_data();
  if(td->lockwait && td->lockid && (td->idle == 0)) {
    int32_t *lock = (int32_t *)td->lockid;
    if (!((*lock)>>31)) {
      atomic_add_i32(lock, 2);
    }
  }
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

void lock_fn(void *lock)
{
  int32_t rd;
  thread_data_t *td = hpcrun_get_thread_data();
  td->lockwait = 1;
  td->lockid = lock;
  // lock is 32 bit int
  volatile int32_t *l = (volatile int32_t *)lock;
  if(!__sync_bool_compare_and_swap(l, 0, 1)) {
    for (;;) {
      while ((rd = (*l)) & 1);
      // post-condition: low order bit of rd == 0 (lock available)
      if (__sync_bool_compare_and_swap(l, rd, rd | 1)) break;
    }
#if 0
    rd = ((*l)>>1)<<1;
    while (!__sync_bool_compare_and_swap(l, rd, rd|1)) {
       rd = ((*l)>>1)<<1;
     }
#endif
  }
  td->lockwait = 0;
  td->lockid = NULL;
}

#define OVERFLOW(val) ((val) < 0)

void unlock_fn(void *lock)
{
  int32_t *l = (int32_t *)lock;

  // __sync_lock_test_and_set is used in libgomp
  // we also find it is faster than __sync_fetch_and_and
//  int val = __sync_fetch_and_and(l, 0);
  int val = __sync_lock_test_and_set(l, 0);

  if(OVERFLOW(val)) {
    val = INT_MAX; 
  } else {
    val = val >> 1;
  }
  if(val > 0) {
    if(period > 0)
      val = val * period;
    ucontext_t uc;
    getcontext(&uc);
    if(need_defer_cntxt()) {
      omp_arg_t omp_arg;
      omp_arg.tbd = false;
      omp_arg.context = NULL;
      if(TD_GET(region_id) > 0) {
 	omp_arg.tbd = true;
 	omp_arg.region_id = TD_GET(region_id);
      }
      hpcrun_async_block();
      hpcrun_sample_callpath(&uc, lockwait_metric_id, val, 0, 1,(void *) &omp_arg);
      hpcrun_async_unblock();
    }
    else {
      hpcrun_async_block();
      hpcrun_sample_callpath(&uc, lockwait_metric_id, val, 0, 1, NULL);
      hpcrun_async_unblock();
    }
  }
}

