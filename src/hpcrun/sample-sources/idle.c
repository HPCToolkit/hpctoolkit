// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

#include <pthread.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include "../hpcrun_options.h"

#include "../audit/audit-api.h"
#include "../metrics.h"
#include "../sample_sources_registered.h"
#include "blame-shift/blame-shift.h"
#include "../safe-sampling.h"
#include "../sample_event.h"
#include "../thread_data.h"
#include "../trace.h"
#include "../messages/messages.h"
#include "../utilities/tokenize.h"


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

// start with 1 total thread, 1 worker
static atomic_uintptr_t total_threads       = 1;
static atomic_uintptr_t active_worker_count = 1;

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
    auditor_exports->exit(1);
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
    auditor_exports->exit(1);
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

  kind_info_t *idle_kind = hpcrun_metrics_new_kind();
  idle_metric_id = hpcrun_set_new_metric_info_and_period
    (idle_kind, "idle", MetricFlags_ValFmt_Real, 1, metric_property_none);
  work_metric_id = hpcrun_set_new_metric_info(idle_kind, "work");
  hpcrun_close_kind(idle_kind);
  TMSG(IDLE, "Metric ids = idle (%d), work(%d)",
       idle_metric_id, work_metric_id);

}

static void
METHOD_FN(finalize_event_list)
{
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

    long active = atomic_load_explicit(&active_worker_count, memory_order_relaxed);
    long total  = atomic_load_explicit(&total_threads, memory_order_relaxed);

    active = (active > 0 ? active : 1); // ensure active is positive

    double working_threads = active;

    double idle_threads = (total - active);

    cct_metric_data_increment(idle_metric_id, node, (cct_metric_data_t){.r = (idle_threads / working_threads) * ((double) metric_value)});
    cct_metric_data_increment(work_metric_id, node, (cct_metric_data_t){.i = metric_value});
  }
}


static void
idle_metric_adjust_workers(long adjustment)
{
  atomic_fetch_add_explicit(&active_worker_count, adjustment, memory_order_relaxed);
  atomic_fetch_add_explicit(&total_threads, adjustment, memory_order_relaxed);

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

  TMSG(IDLE, "blame shift idle work BEFORE decr: %ld", atomic_load_explicit(&active_worker_count, memory_order_relaxed));
  atomic_fetch_add_explicit(&active_worker_count, -1, memory_order_relaxed);
  TMSG(IDLE, "blame shift idle after td->idle incr = %d", td->idle);

  // get a tracing sample out
  if(hpcrun_trace_isactive()) {
    if ( ! hpcrun_safe_enter()) return;
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id,
      (hpcrun_metricVal_t) {.i=0}, 1, 1, NULL);
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

  TMSG(IDLE, "blame shift work, work BEFORE incr: %ld", atomic_load_explicit(&active_worker_count, memory_order_relaxed));
  atomic_fetch_add_explicit(&active_worker_count, 1, memory_order_relaxed);

  // get a tracing sample out
  if(hpcrun_trace_isactive()) {
    if ( ! hpcrun_safe_enter()) return;
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_sample_callpath(&uc, idle_metric_id,
      (hpcrun_metricVal_t) {.i=0}, 1, 1, NULL);
    hpcrun_safe_exit();
  }
}
