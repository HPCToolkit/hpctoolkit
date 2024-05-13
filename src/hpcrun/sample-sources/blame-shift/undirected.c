// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

#include <pthread.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include "../../libmonitor/monitor.h"



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../../cct/cct.h"
#include "../../safe-sampling.h"
#include "../../sample_event.h"

#include "blame-map.h"
#include "undirected.h"
#include "metric_info.h"

#include <stdatomic.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define idle_count(bi) (*(bi->get_idle_count_ptr()))
#define participates(bi) (bi->participates())
#define working(bi) (bi->working())

#define atomic_add(loc, value) \
        atomic_fetch_add_explicit(loc, value, memory_order_relaxed)


/******************************************************************************
 * private operations
 *****************************************************************************/


static inline void
undirected_blame_workers(undirected_blame_info_t *bi, long adjustment)
{
  atomic_add(&bi->active_worker_count, adjustment);
  atomic_add(&bi->total_worker_count, adjustment);
}


//FIXME: must be marked no-inline
static void
trace_current_context(undirected_blame_info_t *bi)
{
  if (!hpcrun_safe_enter()) return;
  ucontext_t uc;
  getcontext(&uc);
  hpcrun_metricVal_t zero = {.i = 0};
  hpcrun_sample_callpath(&uc, bi->idle_metric_id, zero, bi->levels_to_skip + 1, 1, NULL);
  hpcrun_safe_exit();
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

void
undirected_blame_thread_start(undirected_blame_info_t *bi)
{
  undirected_blame_workers(bi, 1);
}


void
undirected_blame_thread_end(undirected_blame_info_t *bi)
{
  undirected_blame_workers(bi, -1);
}

inline void
undirected_blame_idle_begin(undirected_blame_info_t *bi)
{
  //  if ((idle_count(bi))++ > 0) return;
  atomic_add(&bi->active_worker_count, -1L);
}


inline void
undirected_blame_idle_end(undirected_blame_info_t *bi)
{
  // if (--(idle_count(bi)) > 0) return;
  atomic_add(&bi->active_worker_count, 1);
}

void
undirected_blame_idle_begin_trace(undirected_blame_info_t *bi)
{
  undirected_blame_idle_begin(bi);
  trace_current_context(bi);
}


void
undirected_blame_idle_end_trace(undirected_blame_info_t *bi)
{
  undirected_blame_idle_end(bi);
  trace_current_context(bi);
}


void
undirected_blame_sample(void* arg, int metric_id, cct_node_t *node,
                        int metric_incr)
{
  undirected_blame_info_t *bi = (undirected_blame_info_t *) arg;

  if (!participates(bi) || !working(bi)) return;

  int metric_period;

  if (!metric_is_timebase(metric_id, &metric_period)) return;

  double metric_value = metric_period * metric_incr;

  // if (idle_count(bi) == 0)
  { // if this thread is not idle
    // capture active_worker_count into a local variable to make sure
    // that the count doesn't change between the time we test it and
    // the time we use the value

    long active = atomic_load_explicit(&bi->active_worker_count, memory_order_relaxed);
    long total = atomic_load_explicit(&bi->total_worker_count, memory_order_relaxed);

    active = (active > 0 ? active : 1 ); // ensure active is positive

    double working_threads = active;

    double idle_threads = (total - active);

    if (idle_threads > 0) {
      double idleness = (idle_threads/working_threads) * metric_value;

      cct_metric_data_increment(bi->idle_metric_id, node,
                                (cct_metric_data_t){.r = idleness});
    }

    cct_metric_data_increment(bi->work_metric_id, node,
                              (cct_metric_data_t){.i = metric_value});
  }
}
