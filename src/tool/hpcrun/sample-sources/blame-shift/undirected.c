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
// Copyright ((c)) 2002-2014, Rice University
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

#include <hpcrun/cct/cct.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>

#include <hpcrun/sample-sources/blame-shift/blame-map.h>
#include <hpcrun/sample-sources/blame-shift/undirected.h>
#include <hpcrun/sample-sources/blame-shift/metric_info.h>

#include <lib/prof-lean/atomic.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define idle_count(bi) (*(bi->get_idle_count_ptr())) 
#define participates(bi) (bi->participates()) 
#define working(bi) (bi->working()) 


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
  hpcrun_sample_callpath(&uc, bi->idle_metric_id, 0, bi->levels_to_skip + 1, 1);
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
    long workers = bi->active_worker_count;
    long total_threads = bi->total_worker_count;

    double working_threads = (workers > 0 ? workers : 1.0 );

    double idle_threads = (total_threads - working_threads);
		
    if (idle_threads > 0) {
      double idleness = (idle_threads/working_threads) * metric_value;
      
      cct_metric_data_increment(bi->idle_metric_id, node, 
				(cct_metric_data_t){.r = idleness});
    }

    cct_metric_data_increment(bi->work_metric_id, node, 
                              (cct_metric_data_t){.i = metric_value});
  }
}
