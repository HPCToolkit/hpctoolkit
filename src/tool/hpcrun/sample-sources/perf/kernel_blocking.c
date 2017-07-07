// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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
 * local includes
 *****************************************************************************/

/**
 * WARNING : THIS IS AN EXPERIMENTAL FEATURE
 *
 * Kernel blocking event is not validated yet, and only works for Kernel 4.3
 * (at least). This file will be updated once we find a way to make it work
 * properly.
 */

#include <include/linux_info.h>
#include <linux/version.h>

#ifdef ENABLE_PERFMON
#include "perfmon-util.h"
#endif

#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"
#include "event_custom.h"

/******************************************************************************
 * Macros
 *****************************************************************************/

// -----------------------------------------------------
// Predefined events
// -----------------------------------------------------

#define EVNAME_KERNEL_BLOCK     "KERNEL_BLOCKING"
#define EVNAME_CONTEXT_SWITCHES "CS"


//******************************************************************************
// forward declaration
//******************************************************************************



//******************************************************************************
// local variables
//******************************************************************************




/***********************************************************************
 * Method to handle kernel blocking. Called for every context switch event
 ***********************************************************************/
static void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if (current_event != NULL && sv.sample_node != NULL) {
    u64 delta = mmap_data[1].time;

    int metric_index =  current_event->event->metric_custom->metric_index;
    cct_metric_data_increment(metric_index,
                                sv.sample_node,
                               (cct_metric_data_t){.i = delta});

    // it's important to always count the number of samples for debugging purpose
    metric_aux_info_t *info = &current_event->event->metric_custom->metric_desc->info_data;
    info->num_samples++;
  }
}

/***************************************************************
 * Register events to compute blocking time in the kernel
 * We use perf's sofrware context switch event to compute the
 * time spent inside the kernel. For this, we need to sample everytime
 * a context switch occurs, and compute the time when entering the
 * kernel vs leaving the kernel. See perf_event_handler.
 * We need two metrics for this:
 * - blocking time metric to store the time spent in the kernel
 * - context switch metric to store the number of context switches
 ****************************************************************/
static void
register_blocking(event_info_t *event_desc)
{
  // ------------------------------------------
  // create metric to compute blocking time
  // ------------------------------------------
  event_desc->metric_custom->metric_index = hpcrun_new_metric();
  event_desc->metric_custom->metric_desc  = hpcrun_set_metric_info_and_period(
      	event_desc->metric_custom->metric_index, EVNAME_KERNEL_BLOCK,
        MetricFlags_ValFmt_Int, 1 /* period */, metric_property_none);

  // ------------------------------------------
  // create metric to store context switches
  // ------------------------------------------
  event_desc->metric      = hpcrun_new_metric();
  event_desc->metric_desc = hpcrun_set_metric_info_and_period(
      event_desc->metric, EVNAME_CONTEXT_SWITCHES,
      MetricFlags_ValFmt_Real, 1 /* period*/, metric_property_none);

  // ------------------------------------------
  // set context switch event description to be used when creating
  //  perf event of this type on each thread
  // ------------------------------------------
  /* PERF_SAMPLE_STACK_USER may also be good to use */
  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID       |
		    PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN |
		    PERF_SAMPLE_CPU  | PERF_SAMPLE_PERIOD;

  perf_attr_init(PERF_COUNT_SW_CONTEXT_SWITCHES, PERF_TYPE_SOFTWARE,
                 &(event_desc->attr),
                 true        /* use_period*/,
		         1           /* sample every context switch*/,
                 sample_type /* need additional info for sample type */
				 );

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
  event_desc->attr.context_switch = 1;
#endif
  event_desc->attr.sample_id_all = 1;
  // ------------------------------------------
  // additional info for perf event metric
  // ------------------------------------------
  event_desc->metric_desc->info_data.is_frequency = (event_desc->attr.freq == 1);
}


void kernel_blocking_init()
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
  event_custom_t *event_kernel_blocking = hpcrun_malloc(sizeof(event_custom_t));
  event_kernel_blocking->name = EVNAME_KERNEL_BLOCK;
  event_kernel_blocking->register_fn  = register_blocking;   // call backs
  event_kernel_blocking->handler_fn   = kernel_block_handler; // call backs
  event_kernel_blocking->metric_index = 0;   		// these fields to be defined later
  event_kernel_blocking->metric_desc  = NULL; 	 	// these fields to be defined later

  event_custom_register(event_kernel_blocking);
#endif
}
