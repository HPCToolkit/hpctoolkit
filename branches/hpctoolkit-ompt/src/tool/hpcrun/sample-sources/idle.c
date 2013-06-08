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

//
// PAPI sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
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
#include "idle.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
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

#include "hpcrun/ompt.h"
#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <utilities/defer-write.h>
#include <utilities/defer-cntxt.h>
#include <utilities/task-cntxt.h>

#include <lib/support-lean/timer.h>
/******************************************************************************
 * macros
 *****************************************************************************/

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

void scale_fn(void *);
void normalize_fn(cct_node_t *node, cct_op_arg_t arg, size_t level);

static void idle_metric_process_blame_for_sample
(int metric_id, cct_node_t *node, int metric_value);


/******************************************************************************
 * local variables
 *****************************************************************************/
#ifdef GOMP
// for GOMP, the master thread forks all other worker threads and the master thread
// works as a worker thread
static uint64_t work = 1L;// by default work is 1 (1 thread)
static uint64_t thread_num = 1L;// by default thread is 1 (master thread)
// record the max thread number active concurrently
// use it to normalize the idleness at the end of measurement
static uint64_t max_thread_num = 1L;
#else
// all worker threads are forked
static uint64_t work = 0L;
static uint64_t thread_num = 0L;
static uint64_t max_thread_num = 0L;
#endif

static int idle_metric_id = -1;//idle for requested cores
static int core_idle_metric_id = -1;//idle for all hardware cores
static int thread_idle_metric_id = -1;//idle for all threads
static int work_metric_id = -1;
static int overhead_metric_id = -1;
static int count_metric_id = -1;

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
  if (!blame_shift_source_available(bs_type_timer) && !blame_shift_source_available(bs_type_cycles)) {
    STDERR_MSG("HPCToolkit: IDLE metric needs either a REALTIME, CPUTIME, WALLCLOCK, or PAPI_TOT_CYC source.");
    monitor_real_exit(1);
  }

#if 0
  if (idleness_blame_information_source_present == false) {
    STDERR_MSG("HPCToolkit: IDLE metric specified without a plugin that measures "
        "idleness and work.\n" 
        "For dynamic binaries, specify an appropriate plugin with an argument to hpcrun.\n"
	"For static binaries, specify an appropriate plugin with an argument to hpclink.\n");
    monitor_real_exit(1);
  }
#endif
}

static void
METHOD_FN(thread_fini_action)
{
  thread_data_t *td = hpcrun_get_thread_data();
  if(!td->omp_thread) return;
  // it is necessary because it can resolve/partial resolve
  // the region (temporal concern)
  if(td->defer_flag) resolve_cntxt_fini(td);
}

static void
METHOD_FN(stop)
{
  //scale the requested core idleness here
  thread_data_t *td = hpcrun_get_thread_data();
  td->core_profile_trace_data.scale_fn = scale_fn;
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
  return hpcrun_ev_is(ev_str, "IDLE");
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  bs_entry.fn = idle_metric_process_blame_for_sample;
  bs_entry.next = 0;

  blame_shift_register(&bs_entry);

  idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(idle_metric_id, "p_req_core_idleness",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  core_idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(core_idle_metric_id, "p_all_core_idleness",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  thread_idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(thread_idle_metric_id, "p_all_thread_idleness",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  work_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(work_metric_id, "p_work",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  overhead_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(overhead_metric_id, "p_overhead",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);

  count_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(count_metric_id, "count_helper",
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
register_blame_shift()
{
  ompt_set_callback(ompt_event_thread_create, (ompt_callback_t)start_fn);
  ompt_set_callback(ompt_event_thread_exit, (ompt_callback_t)end_fn);
  ompt_set_callback(ompt_event_idle_begin, (ompt_callback_t)idle_fn);
  ompt_set_callback(ompt_event_idle_end, (ompt_callback_t)work_fn);
  ompt_set_callback(ompt_event_wait_barrier_begin, (ompt_callback_t)bar_wait_begin);
  ompt_set_callback(ompt_event_wait_barrier_end, (ompt_callback_t)bar_wait_end);
}

void
scale_fn(void *td)
{
  core_profile_trace_data_t *cptd = (core_profile_trace_data_t *)td;
  cct_node_t *root;
  root = cptd->epoch->csdata.top;
  hpcrun_cct_walk_node_1st(root, normalize_fn, NULL);
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
  ompt_wait_id_t wait_id;
  if((ompt_get_state(&wait_id) == ompt_state_overhead) ||
     (ompt_get_state(&wait_id) == ompt_state_wait_critical) ||
     (ompt_get_state(&wait_id) == ompt_state_wait_lock) ||
     (ompt_get_state(&wait_id) == ompt_state_wait_nest_lock) ||
     (ompt_get_state(&wait_id) == ompt_state_wait_atomic) ||
     (ompt_get_state(&wait_id) == ompt_state_wait_ordered))
    return 1;
  return 0;
}

static void
#ifdef XU_OLD
process_blame_for_sample(cct_node_t *node, uint64_t metric_value)
#endif
idle_metric_process_blame_for_sample(int metric_id, cct_node_t *node, int metric_incr)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);
 
  // Only blame shift idleness for time and cycle metrics. 
  if ( ! (metric_desc->properties.time | metric_desc->properties.cycles) ) 
    return;
  
  int metric_value = metric_desc->period * metric_incr;
  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle == 0) { // if thread is not idle
                double work_l = (double) work;
		double idle_l = 1.0;
  		if(work_l < 1.0) work_l = 1.0; // make sure the divider is not zero
		cct_metric_data_increment(idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});

                idle_l = (double)(omp_get_num_procs()) - work_l;
  		if(idle_l < 0.0) idle_l = 0.0;
		cct_metric_data_increment(core_idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});
		
		idle_l = (double)(omp_get_num_threads()) - work_l;
		cct_metric_data_increment(thread_idle_metric_id, node, 
					  (cct_metric_data_t){.r = (idle_l/work_l)*metric_value});
                if(is_overhead(node))
		  cct_metric_data_increment(overhead_metric_id, node, (cct_metric_data_t){.i = metric_value});
		else if (!td->lockwait)
		  cct_metric_data_increment(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
  		cct_metric_data_increment(count_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}

// take synchronous samples when the time difference is
// larger than diff_time_thres
#define DIFF_TIME_THRES 5000
static void 
idle(bool thres_check)
{
  uint64_t cur_bar_time_us;
  int ret = time_getTimeReal(&cur_bar_time_us);
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    abort();
  }
  uint64_t diff_time = cur_bar_time_us - TD_GET(last_bar_time_us);

  if(thres_check && diff_time < DIFF_TIME_THRES) return;
  hpcrun_safe_enter();
  ucontext_t uc;
  getcontext(&uc);
  hpcrun_sample_callpath(&uc, idle_metric_id, 0, 2, 1, NULL);
  hpcrun_safe_exit();
  ret = time_getTimeReal(&TD_GET(last_bar_time_us));
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    abort();
  }
}

static void 
thread_create_exit()
{
  hpcrun_safe_enter();
  ucontext_t uc;
  getcontext(&uc);
  hpcrun_sample_callpath(&uc, idle_metric_id, 0, 0, 1, NULL);
  hpcrun_safe_exit();
}

void idle_fn(ompt_data_t *thread_data)
{
  hpcrun_safe_enter();
  atomic_add_i64(&work, -1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 1;
  hpcrun_safe_exit();

  if(hpcrun_trace_isactive()) {
    idle(true);
    // block samples between idle_fn and work_fn
    // it will be unblocked at work_fn()
    hpcrun_safe_enter();
  }
}

void work_fn(ompt_data_t *thread_data)
{
  hpcrun_safe_enter();
  atomic_add_i64(&work, 1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 0;
  hpcrun_safe_exit();

  if(hpcrun_trace_isactive()) {
    idle(true);
  }
  hpcrun_safe_exit();
}

void start_fn(ompt_data_t *thread_data)
{
  hpcrun_safe_enter();
  atomic_add_i64(&thread_num, 1L);
  if(thread_num > max_thread_num)
    fetch_and_store_i64(&max_thread_num, thread_num);

  atomic_add_i64(&work, 1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 0;
  td->omp_thread = 1;
  td->defer_write = 1;

//  if(td->defer_flag) resolve_cntxt_fini();

  if(hpcrun_trace_isactive()) {
    thread_create_exit();
    int ret = time_getTimeReal(&TD_GET(last_bar_time_us));
    if (ret != 0) {
      EMSG("time_getTimeReal (clock_gettime) failed!");
      abort();
    }  
  }

  hpcrun_safe_exit();
}

void end_fn(ompt_data_t *thread_data)
{
  hpcrun_safe_enter();
  atomic_add_i64(&thread_num, -1L);
  atomic_add_i64(&work, -1L);

  if(hpcrun_trace_isactive()) {
    thread_create_exit();
  }

  hpcrun_safe_exit();
}

void bar_wait_begin(ompt_data_t *task_data, ompt_parallel_id_t parallel_id)
{
  hpcrun_safe_enter();
  atomic_add_i64(&work, -1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 1;
  hpcrun_safe_exit();

  if(hpcrun_trace_isactive()) {
    idle(true);
    // block samples between bar_wait_begin and bar_wait_end
    // it will be unblocked at bar_wait_end()
    hpcrun_safe_enter();
  }
}

void bar_wait_end(ompt_data_t *task_data, ompt_parallel_id_t parallel_id)
{
  hpcrun_safe_enter();
  atomic_add_i64(&work, 1L);
  thread_data_t *td = hpcrun_get_thread_data();
  td->idle = 0;
  hpcrun_safe_exit();

  if(hpcrun_trace_isactive()) {
    idle(true);
  }
  hpcrun_safe_exit();
}
