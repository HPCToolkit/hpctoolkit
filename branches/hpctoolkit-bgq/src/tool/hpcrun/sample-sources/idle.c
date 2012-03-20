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
#include <hpcrun/trace.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <omp.h>
//
// include omp registration stuff
//
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define OMPstr "gomp"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_blame_for_sample(cct_node_t *node, int metric_value);

/******************************************************************************
 * local variables
 *****************************************************************************/
static uint64_t work = 1L;// by default work is 1 (1 thread)

static int idle_metric_id = -1;
static int work_metric_id = -1;
static int always_metric_id = -1;

static int omp_lm_id = -1;

static bs_fn_entry_t bs_entry;
static bool idleness_measurement_enabled = false;
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

// Return true if IDLE recognizes the name
static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, "IDLE") != NULL);
}
 
extern void init_hack(void);

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(IDLE, "Process event list");
  idleness_measurement_enabled = true;
  bs_entry.fn = process_blame_for_sample;
  bs_entry.next = 0;

  blame_shift_register(&bs_entry);

  idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(idle_metric_id, "idle",
				    MetricFlags_ValFmt_Real, 1);

  work_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(work_metric_id, "work",
				    MetricFlags_ValFmt_Int, 1);
  always_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(always_metric_id, "!Always",
				    MetricFlags_ValFmt_Int, 1);
  TMSG(IDLE, "Metric ids = idle (%d), work(%d), always(%d)",
       idle_metric_id, work_metric_id, always_metric_id);
  init_hack();

	overhead_metric_id = hpcrun_new_metric();
	hpcrun_set_metric_info_and_period(overhead_metric_id, "p_overhead",
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
// for dynamically loaded case, think the overhead comes from the non-idle samples
// in the openmp library
static int
is_overhead(cct_node_t *node)
{
  cct_addr_t *addr = hpcrun_cct_addr(node);
  load_module_t *lm = hpcrun_loadmap_findById(addr->ip_norm.lm_id);
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
  // hack to check results
  metric_value = 1;
  TMSG(IDLE, "Log !Always count");
  cct_metric_data_increment(always_metric_id, node,
			    (cct_metric_data_t){.i = 1});
  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle == 0) { // if thread is not idle
    double work_l = 1.0;
    if (! work) {
      EMSG("Timing anomaly: work_l set to 1.0 !!");
    }
    else {
      work_l = (double) work;
    }
    //		double idle_l = (double)(omp_get_max_threads())- work_l;
    double idle_l = (double)(atoi(getenv("OMP_NUM_THREADS"))) - work_l;
		
    cct_metric_data_increment(idle_metric_id, node, (cct_metric_data_t){.r = (idle_l/work_l) * ((double) metric_value)});
    cct_metric_data_increment(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}

void
blame_shift_idle(void)
{
  if (! idleness_measurement_enabled) return;
  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle++ > 0) return;
  TMSG(IDLE, "blame shift idle work BEFORE decr: %ld", work);
  atomic_add_i64(&work, -1L);
  TMSG(IDLE, "blame shift idle after td->idle incr = %d", td->idle);

  // get a tracing sample out
  if (! trace_isactive()) return;
  hpcrun_async_block();

  if(trace_isactive()) {
    hpcrun_async_block();
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 0, 1);
    hpcrun_async_unblock();
  }
}

void
 blame_shift_work(void)
{
  if (! idleness_measurement_enabled) return;
  thread_data_t *td = hpcrun_get_thread_data();
  TMSG(IDLE, "blame shift idle before td->idle decr = %d", td->idle);
  if (--td->idle > 0)return;

  TMSG(IDLE, "blame shift work, work BEFORE incr: %ld", work);
  atomic_add_i64(&work, 1L);

  // get a tracing sample out
  if (! trace_isactive()) return;
  hpcrun_async_block();

  if(trace_isactive()) {
    hpcrun_async_block();
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 0, 1);
    hpcrun_async_unblock();
  }
}

void init_hack()
{
  work = atoi(getenv("OMP_NUM_THREADS"));
  TMSG(IDLE, "init_hack called, work = %d", work);
}
