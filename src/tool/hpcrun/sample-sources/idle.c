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
#include "blame-shift.h"
#include "idle.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/ompt/ompt-interface.h>

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

static void idle_metric_process_blame_for_sample
(int metric_id, cct_node_t *node, int metric_incr);


/******************************************************************************
 * local variables
 *****************************************************************************/
static uintptr_t active_worker_count = 1; // start with one thread
static uint64_t total_threads = 1;        // start with one thread

static int idle_blame_metric_id = -1;
static int idle_wait_metric_id = -1;
static int work_metric_id = -1;

static bs_fn_entry_t bs_entry;
static bool idleness_measurement_enabled = false;
static bool idleness_blame_information_source_present = false;

idle_blame_participant_fn idle_blame_participant = 0;


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
  if (!blame_shift_heartbeat_available(bs_heartbeat_timer) && 
      !blame_shift_heartbeat_available(bs_heartbeat_cycles)) {
    STDERR_MSG("HPCToolkit: IDLE metric needs either a REALTIME, CPUTIME, "
	       "WALLCLOCK, or PAPI_TOT_CYC source.");
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
  if (idleness_blame_information_source_present == false) {
    STDERR_MSG("HPCToolkit: IDLE metric requested but no information source "
	       "provided that measures idleness and work.\n" 
	       "For dynamic binaries, specify an appropriate plugin with " 
	       "an argument to hpcrun.\n"
	       "For static binaries, specify an appropriate plugin with "
	       "an argument to hpclink.\n");
  }
}


static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


// Return true if IDLE recognizes the name
static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return hpcrun_ev_is(ev_str, "IDLE");
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(IDLE, "Process event list");
  idle_metric_enable();
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("======================================"
	 "=====================================\n");
  printf("Available idle preset events\n");
  printf("======================================"
	 "=====================================\n");
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
idle_metric_process_blame_for_sample(int metric_id, cct_node_t *node, 
				     int metric_incr)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);
 
  // Only blame shift idleness for time and cycle metrics. 
  if ( ! (metric_desc->properties.time | metric_desc->properties.cycles) ) 
    return;
  
  thread_data_t *td = hpcrun_get_thread_data();
  td->last_sample++;

  if (idle_blame_participant()) { 
    // it is an openmp worker thread, so it participates in blame shifting

    int metric_value = metric_desc->period * metric_incr;
    if (td->idle == 0 && td->blame_target == 0) { 
      // if this thread is not idle, capture active_worker_count into 
      // a local variable to make sure that the count doesn't change
      // between the time we test it and the time we use the value
      long workers = active_worker_count;
      double working_threads = (workers > 0 ? workers : 1.0 );

      double idle_threads = total_threads - working_threads;
		
      double idle_blame = (idle_threads / working_threads) * metric_value;
      cct_metric_data_increment
	(idle_blame_metric_id, node, (cct_metric_data_t){.r = idle_blame});
      cct_metric_data_increment
	(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
    } else {
      if (td->idle != 0 && td->blame_target == 0) { 
	cct_metric_data_increment
	  (idle_wait_metric_id, node, (cct_metric_data_t){.i = metric_value});
      }
    }
  }
}


/******************************************************************************
 * interface operations 
 *****************************************************************************/

void
idle_metric_register_blame_source(idle_blame_participant_fn participant)
{
   idleness_blame_information_source_present = true;
   idle_blame_participant = participant;
}


void
idle_metric_blame_shift_idle(void)
{
  if (! idleness_measurement_enabled) return;

  thread_data_t *td = hpcrun_get_thread_data();
  hpcrun_safe_enter();
  if (td->idle++ > 0) {
    hpcrun_safe_exit();
    return;
  }
  atomic_add(&active_worker_count, -1L);

  hpcrun_safe_exit();

  TMSG(IDLE, "blame shift idle work after decr: %ld", active_worker_count);
  TMSG(IDLE, "blame shift idle after td->idle incr = %d", td->idle);
}


void
idle_metric_blame_shift_work(void)
{
  if (! idleness_measurement_enabled) return;

  thread_data_t *td = hpcrun_get_thread_data();
  hpcrun_safe_enter();
  if (--td->idle > 0) {
    hpcrun_safe_exit();
    return;
  }
  atomic_add(&active_worker_count, 1L);
  hpcrun_safe_exit();

  TMSG(IDLE, "blame shift idle after td->idle decr = %d", td->idle);
  TMSG(IDLE, "blame shift work, work after incr: %ld", active_worker_count);
}


void
idle_metric_thread_start()
{
  hpcrun_safe_enter();
  atomic_add_i64(&active_worker_count, 1L);
  atomic_add_i64(&total_threads, 1L);

  thread_data_t *td = hpcrun_get_thread_data();
  td->omp_thread = 1;
  td->defer_write = 1;
  hpcrun_safe_exit();
}


void
idle_metric_thread_end()
{
  hpcrun_safe_enter();
  atomic_add_i64(&active_worker_count, -1L);
  atomic_add_i64(&total_threads, -1L);

  thread_data_t *td = hpcrun_get_thread_data();
  td->omp_thread = 0;
  td->defer_write = 0;
  hpcrun_safe_exit();
}


void
idle_metric_enable() 
{
  if (idleness_measurement_enabled == false) {
    idleness_measurement_enabled = true;
    bs_entry.fn = idle_metric_process_blame_for_sample;
    bs_entry.next = 0;
    
    blame_shift_register(&bs_entry, bs_type_undirected);
    
    idle_blame_metric_id = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period
      (idle_blame_metric_id, "IDLE_BLAME",
       MetricFlags_ValFmt_Real, 1, metric_property_none);

    idle_wait_metric_id = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period
      (idle_wait_metric_id, "IDLE_WAIT",
       MetricFlags_ValFmt_Int, 1, metric_property_none);

    work_metric_id = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period
      (work_metric_id, "WORK",
       MetricFlags_ValFmt_Int, 1, metric_property_none);

    TMSG(IDLE, "Metric ids = idle_wait (%d) idle_blame (%d), work(%d)", 
	 idle_wait_metric_id, idle_blame_metric_ic, work_metric_id);
  }
}

