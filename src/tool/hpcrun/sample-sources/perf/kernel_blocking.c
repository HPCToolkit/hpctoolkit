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
// Copyright ((c)) 2002-2018, Rice University
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
#include <assert.h>
#include <include/linux_info.h>
// #include <linux/version.h> // not used anymore. the build will decide if the
                           // kernel is recent enough or not

#ifdef ENABLE_PERFMON
#include "perfmon-util.h"
#endif

#include "kernel_blocking.h"

#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"
#include "event_custom.h"

/******************************************************************************
 * Macros
 *****************************************************************************/

#define KERNEL_BLOCKING_DEBUG 0

// -----------------------------------------------------
// Predefined events
// -----------------------------------------------------

#define EVNAME_KERNEL_BLOCK     "BLOCKTIME"
#define EVNAME_CONTEXT_SWITCHES "CS"


//******************************************************************************
// forward declaration
//******************************************************************************



//******************************************************************************
// local variables
//******************************************************************************

// metric index for kernel blocking
// usually each thread has the same metric index, so it's safe to make it global
// for each thread (I hope).
static int metric_blocking_index = -1;

static __thread u64          time_cs_out = 0;    // time when leaving the application process
static __thread cct_node_t  *cct_kernel  = NULL; // cct of the last access to kernel
static __thread u32          cpu = 0;           // cpu of the last sample
static __thread u32          pid = 0, tid = 0;  // last pid/tid

/******************************************************************************
 * private operations
 *****************************************************************************/

static void
blame_kernel_time(event_thread_t *current_event, cct_node_t *cct_kernel,
    perf_mmap_data_t *mmap_data)
{
  // make sure the time is is zero or positive
  if (mmap_data->time < time_cs_out) {
    TMSG(LINUX_PERF, "old t: %l, c: %d, p: %d, td: %d -- vs -- t: %l, c: %d, p: %d, td: %d",
        time_cs_out, cpu, pid, tid, mmap_data->time, mmap_data->cpu, mmap_data->pid, mmap_data->tid);
    return;
  }

  uint64_t delta = mmap_data->time - time_cs_out;

  // ----------------------------------------------------------------
  // blame the time spent in the kernel to the cct kernel
  // ----------------------------------------------------------------

  cct_metric_data_increment(metric_blocking_index, cct_kernel,
      (cct_metric_data_t){.i = delta});

  // ----------------------------------------------------------------
  // it's important to always count the number of samples for debugging purpose
  // ----------------------------------------------------------------

  thread_data_t *td = hpcrun_get_thread_data();
  td->core_profile_trace_data.perf_event_info[metric_blocking_index].num_samples++;
}

/***********************************************************************
 * this function is called when a cycle or time event occurs, OR a
 *  context switch event occurs.
 *
 * to compute kernel blocking time:
 *  time_outside_kernel - time_inside_kernel
 *
 * algorithm:
 *  if this is the first time entering kernel (time_cs_out == 0) then
 *    set time_cs_out
 *    store the cct to cct_kernel
 *  else
 *    if we are outside the kernel, then:
 *      compute the blocking time
 *      blame the time to the kernel cct
 *
 *    else if we are still inside the kernel with different cct_kernel:
 *      compute the time
 *      blame the time to the old cct_kernel
 *      set time_cs_out to the current time
 *      set cct_kernel with the new cct
 *    end if
 *  end if
 ***********************************************************************/
void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if (metric_blocking_index < 0)
    return; // not initialized or something wrong happens in the initialization

  if (mmap_data == NULL) {

    // somehow, at the end of the execution, a sample event is still triggered
    // and in this case, the arguments are null. Is this our bug ? or gdb ?

    return;
  }

  if (mmap_data->context_switch_time > 0) {
    // ----------------------------------------------------------------
    // when PERF_RECORD_SWITCH (context switch out) occurs:
    //  if this is the first time, (time_cs_out is zero), then freeze the
    //    cs time and the current cct
    // ----------------------------------------------------------------

    if (time_cs_out == 0) {
      // ----------------------------------------------------------------
      // this is the first time we enter the kernel (leaving the current process)
      // needs to store the time to compute the blocking time in
      //  the next step when leaving the kernel
      // ----------------------------------------------------------------
      time_cs_out = mmap_data->context_switch_time;
      cpu = mmap_data->cpu;
      pid = mmap_data->pid;
      tid = mmap_data->tid;
    }
  } else {

    // ----------------------------------------------------------------
    // when PERF_SAMPLE_RECORD occurs:
    // check whether we were previously in the kernel or not (time_cs_out > 0)
    // if we were in the kernel, then we blame the different time to the
    //   cct kernel
    // ----------------------------------------------------------------

    if (current_event->event->attr.config == PERF_COUNT_SW_CONTEXT_SWITCHES) {

      if (cct_kernel != NULL && time_cs_out > 0) {
        // corner case : context switch within a context switch !
        blame_kernel_time(current_event, cct_kernel, mmap_data);
        time_cs_out  = 0;
      }
      // context switch inside the kernel:  record the new cct
      cct_kernel  = sv.sample_node;

    } else if (cct_kernel != NULL) {
      // other event than cs occurs (it can be cycles, clocks or others)

      if ((mmap_data->time > 0) && (time_cs_out > 0) && (mmap_data->nr==0)) {
#if KERNEL_BLOCKING_DEBUG
        unsigned int cpumode = mmap_data->header_misc & PERF_RECORD_MISC_CPUMODE_MASK;
        assert(cpumode == PERF_RECORD_MISC_USER);
#endif

        blame_kernel_time(current_event, cct_kernel, mmap_data);
        // important: need to reset the value to inform that we are leaving the kernel
        cct_kernel   = NULL;
        time_cs_out  = 0;
      }
    }
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

  metric_blocking_index = event_desc->metric_custom->metric_index;
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

  struct perf_event_attr *attr = &(event_desc->attr);
  attr->config = PERF_COUNT_SW_CONTEXT_SWITCHES;
  attr->type   = PERF_TYPE_SOFTWARE;

  perf_attr_init( attr,
      true        /* use_period*/,
      1           /* sample every context switch*/,
      sample_type /* need additional info for sample type */
  );

  event_desc->attr.context_switch = 1;
  event_desc->attr.sample_id_all = 1;
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

void kernel_blocking_init()
{
  // unfortunately, the older version doesn't support context switch event properly

  event_custom_t *event_kernel_blocking = hpcrun_malloc(sizeof(event_custom_t));
  event_kernel_blocking->name         = EVNAME_KERNEL_BLOCK;
  event_kernel_blocking->desc         = "Approximation of a thread's blocking time."  
					" This event requires another event (such as CYCLES) to profile with."  
					" The unit time is hardware-dependent but mostly in microseconds."  
					" This event is only available on Linux kernel 4.3 or newer.";
  event_kernel_blocking->register_fn  = register_blocking;   // call backs
  event_kernel_blocking->handler_fn   = NULL; 		// No call backs: we want all event to call us
  event_kernel_blocking->metric_index = 0;   		// these fields to be defined later
  event_kernel_blocking->metric_desc  = NULL; 	 	// these fields to be defined later

  event_custom_register(event_kernel_blocking);
}
