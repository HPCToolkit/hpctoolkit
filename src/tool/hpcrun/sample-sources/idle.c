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

#include <utilities/defer-write.h>
#include <utilities/defer-cntxt.h>
/******************************************************************************
 * macros
 *****************************************************************************/

#define OMPstr "gomp"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/
static void idle_fn();
static void work_fn();
static void start_fn(int rank);
static void end_fn();

static void process_blame_for_sample(cct_node_t *node, int metric_value);
void scale_fn(void *);
void normalize_fn(cct_node_t *node, cct_op_arg_t arg, size_t level);

/******************************************************************************
 * local variables
 *****************************************************************************/
static uint64_t work = 1L;// by default work is 1 (1 thread)
static uint64_t thread_num = 1L;// by default thread is 1 (master thread)
// record the max thread number active concurrently
// use it to normalize the idleness at the end of measurement
static uint64_t max_thread_num = 1L;

static int idle_metric_id = -1;//idle for requested cores
static int core_idle_metric_id = -1;//idle for all hardware cores
static int thread_idle_metric_id = -1;//idle for all threads
static int work_metric_id = -1;
static int overhead_metric_id = -1;
static int count_metric_id = -1;

static int omp_lm_id = -1;

static bs_fn_entry_t bs_entry;

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
  //scale the requested core idleness here
  thread_data_t *td = hpcrun_get_thread_data();
  if(!td->scale_fn)
    td->scale_fn = scale_fn;
#if 0
  thread_data_t *td = hpcrun_get_thread_data();
  cct_node_t *root, *unresolved_root;
  root = td->epoch->csdata.top;
  unresolved_root = td->epoch->csdata.unresolved_root;
  hpcrun_cct_walk_node_1st(root, normalize_fn, NULL);
  hpcrun_cct_walk_node_1st(unresolved_root, normalize_fn, NULL);
#endif
}

static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}

// Return true if IDLE recognizes the name
static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, "IDLE") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
        bs_entry.fn = process_blame_for_sample;
        bs_entry.next = 0;

	GOMP_barrier_callback_register(idle_fn, work_fn);
	GOMP_start_callback_register(start_fn, end_fn);

	blame_shift_register(&bs_entry);

	idle_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(idle_metric_id, "p_req_core_idleness",
			MetricFlags_ValFmt_Real, 1);

	core_idle_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(core_idle_metric_id, "p_all_core_idleness",
			MetricFlags_ValFmt_Real, 1);

	thread_idle_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(thread_idle_metric_id, "p_all_thread_idleness",
			MetricFlags_ValFmt_Real, 1);

	work_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(work_metric_id, "p_work",
			MetricFlags_ValFmt_Int, 1);

	overhead_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(overhead_metric_id, "p_overhead",
			MetricFlags_ValFmt_Int, 1);

	count_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(count_metric_id, "count_helper",
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

#define ss_name idle
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

void
scale_fn(void *thread_data)
{
TMSG(SET_DEFER_CTXT, "max thread is %d", max_thread_num);
  thread_data_t *td = (thread_data_t *)thread_data;
  cct_node_t *root, *unresolved_root;
  root = td->epoch->csdata.top;
  unresolved_root = td->epoch->csdata.unresolved_root;
  hpcrun_cct_walk_node_1st(root, normalize_fn, NULL);
  hpcrun_cct_walk_node_1st(unresolved_root, normalize_fn, NULL);
}

void normalize_fn(cct_node_t *node, cct_op_arg_t arg, size_t level)
{
  if(hpcrun_cct_is_leaf(node)) {
    metric_set_t *set = hpcrun_get_metric_set(node);
    if(set) {
      cct_metric_data_t *mdata_idle  = hpcrun_metric_set_loc(set, idle_metric_id);
      cct_metric_data_t *mdata_count = hpcrun_metric_set_loc(set, count_metric_id);
      assert(mdata_idle);
      assert(mdata_count);
      mdata_idle->r *= (double)max_thread_num;
      mdata_idle->r -= (double)mdata_count->i;
    }
  }
}
// for dynamically loaded case, think the overhead comes from the non-idle samples
// in the openmp library
static int
is_overhead(cct_node_t *node)
{
  cct_addr_t *addr = hpcrun_cct_addr(node);
  load_module_t *lm = hpcrun_loadmap_findById(addr->ip_norm.lm_id);
  if(!lm) return 0;
  if(lm->id == omp_lm_id) return 1;
  if(strstr(lm->name, OMPstr))
  {
    omp_lm_id = addr->ip_norm.lm_id;
    return 1;
  }
  else
    return 0;
}

static void
process_blame_for_sample(cct_node_t *node, int metric_value)
{
  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle == 0) { // if thread is not idle
                double work_l = (double) work;
		double idle_l = 1.0;
  		if(work_l < 1.0) work_l = 1.0; // make sure the divider is not zero
		cct_metric_data_increment(idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});

                idle_l = (double)(omp_get_num_procs()) - work_l;
		cct_metric_data_increment(core_idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});
		
		idle_l = (double)(thread_num) - work_l;
		cct_metric_data_increment(thread_idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});
                if(is_overhead(node) || (td->overhead > 0))
		  cct_metric_data_increment(overhead_metric_id, node, (cct_metric_data_t){.i = metric_value});
		else if (!td->lockwait)
		  cct_metric_data_increment(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
  		cct_metric_data_increment(count_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}

void idle_fn()
{
  hpcrun_async_block();
  atomic_add_i64(&work, -1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 1;
  hpcrun_async_unblock();

  if(trace_isactive()) {
    hpcrun_async_block();
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 0, 1, NULL);
    hpcrun_async_unblock();
  }
}

void work_fn()
{
  hpcrun_async_block();
  atomic_add_i64(&work, 1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 0;
  hpcrun_async_unblock();

  if(trace_isactive()) {
    hpcrun_async_block();
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 0, 1, NULL);
    hpcrun_async_unblock();
  }
}

void start_fn(int rank)
{
  hpcrun_async_block();
  atomic_add_i64(&thread_num, 1L);
  if(thread_num > max_thread_num)
    fetch_and_store_i64(&max_thread_num, thread_num);

  atomic_add_i64(&work, 1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 0;

  if(ENABLED(SET_DEFER_WRITE)) {
    td->defer_write = 1;
  }
  else {
    // at the beginning of one thread, try to resolve any other threads
    resolve_other_cntxt(false);
  }

  hpcrun_async_unblock();
}

void end_fn()
{
  hpcrun_async_block();
  atomic_add_i64(&thread_num, -1L);
  atomic_add_i64(&work, -1L);

  thread_data_t *td = hpcrun_get_thread_data();
  if(td->defer_flag)
    resolve_cntxt_fini();
  else  if(ENABLED(SET_DEFER_WRITE)) {
    add_defer_td(td);
  }

  hpcrun_async_unblock();
}

