// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-omp/src/tool/hpcrun/sample-sources/idle.c $
// $Id: idle.c 3691 2012-03-05 21:21:08Z xl10 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
 * system includes
 *****************************************************************************/

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
#include <sample-sources/blame-shift/blame-shift.h>

#include <hpcrun/hpcrun_options.h>

#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>
#include <messages/messages.h>
#include <utilities/tokenize.h>

#include <lib/prof-lean/atomic.h>



/******************************************************************************
 * macros
 *****************************************************************************/

// elide debugging support for performance
#undef TMSG
#define TMSG(...)



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void idle_metric_process_blame_for_sample(void* arg, int metric_id, 
						 cct_node_t *node, int metric_value);



/******************************************************************************
 * local variables
 *****************************************************************************/
static long active_worker_count = 1; // by default is 1 (1 thread)
static long total_threads       = 1; // by default is 1 (1 thread)

static int idle_metric_id = -1;
static int work_metric_id = -1;

static bs_fn_entry_t bs_entry;
static bool idleness_measurement_enabled = false;
static bool idleness_blame_information_source_present = false;




/******************************************************************************
 * sample source interface 
 *****************************************************************************/

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
  if (!blame_shift_source_available(bs_type_timer) && 
      !blame_shift_source_available(bs_type_cycles)) {
    STDERR_MSG("HPCToolkit: IDLE metric needs either a REALTIME, " 
	       "CPUTIME, WALLCLOCK, or PAPI_TOT_CYC source.");
    monitor_real_exit(1);
  }
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
  if (idleness_blame_information_source_present == false) {
    STDERR_MSG("HPCToolkit: IDLE metric specified without a plugin that measures "
        "idleness and work.\n" 
        "For dynamic binaries, specify an appropriate plugin with an argument to hpcrun.\n"
	"For static binaries, specify an appropriate plugin with an argument to hpclink.\n");
    monitor_real_exit(1);
  }

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
  TMSG(IDLE, "Process event list");
  idleness_measurement_enabled = true;
  bs_entry.fn = idle_metric_process_blame_for_sample;
  bs_entry.next = 0;

  blame_shift_register(&bs_entry);

  idle_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(idle_metric_id, "idle",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  work_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(work_metric_id, "work",
				    MetricFlags_ValFmt_Int, 1, metric_property_none);
  TMSG(IDLE, "Metric ids = idle (%d), work(%d)",
       idle_metric_id, work_metric_id);

}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("NO Available IDLE preset events\n");
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

static void
idle_metric_process_blame_for_sample(void* arg, int metric_id, cct_node_t *node, int metric_incr)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);
 
  // Only blame shift idleness for time and cycle metrics. 
  if ( ! (metric_desc->properties.time | metric_desc->properties.cycles) ) 
    return;
  
  int metric_value = metric_desc->period * metric_incr;
  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle == 0) { // if this thread is not idle
    // capture active_worker_count into a local variable to make sure that the count doesn't change
    // between the time we test it and the time we use the value
    long workers = active_worker_count;
    double working_threads = (workers > 0 ? workers : 1.0 );

    double idle_threads = (total_threads - working_threads);
		
    cct_metric_data_increment(idle_metric_id, node, (cct_metric_data_t){.r = (idle_threads / working_threads) * ((double) metric_value)});
    cct_metric_data_increment(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}


static void 
idle_metric_adjust_workers(long adjustment)
{
  atomic_add(&active_worker_count, adjustment);
  atomic_add(&total_threads, adjustment);

  TMSG(IDLE, "idle_metric_adjust_workers called, work = %d", active_worker_count);
}



/******************************************************************************
 * interface operations 
 *****************************************************************************/

void 
idle_metric_thread_start()
{
  idle_metric_adjust_workers(1);
}


void 
idle_metric_thread_end()
{
  idle_metric_adjust_workers(-1);
}


void
idle_metric_register_blame_source()
{
   idleness_blame_information_source_present = true;
}


void
idle_metric_blame_shift_idle(void)
{
  if (! idleness_measurement_enabled) return;

  thread_data_t *td = hpcrun_get_thread_data();
  if (td->idle++ > 0) return;

  TMSG(IDLE, "blame shift idle work BEFORE decr: %ld", active_worker_count);
  atomic_add(&active_worker_count, -1L);
  TMSG(IDLE, "blame shift idle after td->idle incr = %d", td->idle);

  // get a tracing sample out
  if(hpcrun_trace_isactive()) {
    if ( ! hpcrun_safe_enter()) return;
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 1, 1);
    hpcrun_safe_exit();
  }
}


void
idle_metric_blame_shift_work(void)
{
  if (! idleness_measurement_enabled) return;

  thread_data_t *td = hpcrun_get_thread_data();
  TMSG(IDLE, "blame shift idle before td->idle decr = %d", td->idle);
  if (--td->idle > 0) return;

  TMSG(IDLE, "blame shift work, work BEFORE incr: %ld", active_worker_count);
  atomic_add(&active_worker_count, 1);

  // get a tracing sample out
  if(hpcrun_trace_isactive()) {
    if ( ! hpcrun_safe_enter()) return;
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id, 0, 1, 1);
    hpcrun_safe_exit();
  }
}

