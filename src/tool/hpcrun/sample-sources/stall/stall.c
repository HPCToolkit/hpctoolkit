// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

#include <linux/version.h>
#include <linux/perf_event.h>

#include <messages/messages.h>
#include <utilities/arch/cpuid.h>

#include <sample-sources/sample_source_obj.h>
#include <sample-sources/simple_oo.h>

#include "sample-sources/perf/perfmon-util.h"
#include "sample-sources/perf/perf-util.h"
#include "sample-sources/perf/event_custom.h"

#include "stall.h"

#include <utilities/arch/cpuid.h>

#include <sample-sources/perf/pmu.h>



/**
 * attention: the order of the array can be important.
 *
 * stalls cycle information taken from intel discussion forum
 * https://software.intel.com/en-us/forums/software-tuning-performance-optimization-platform-monitoring/topic/514733
 * https://software.intel.com/en-us/forums/software-tuning-performance-optimization-platform-monitoring/topic/561596
 *
 * and the list of hardware counter at:
 * https://oprofile.sourceforge.io/docs/
 * https://download.01.org/perfmon/index/
 */
static struct pmu_config_s stall_events[] = {
    {INTEL_SNB,    "RESOURCE_STALLS.ANY"},
    {INTEL_SNB,    "CYCLE_ACTIVITY.CYCLES_L2_PENDING"},
    {INTEL_SNB,    "CYCLE_ACTIVITY.CYCLES_L1D_PENDING"},

    {INTEL_SNB_EP, "RESOURCE_STALLS.ANY"},
    {INTEL_SNB_EP, "CYCLE_ACTIVITY.CYCLES_L2_PENDING"},
    {INTEL_SNB_EP, "CYCLE_ACTIVITY.CYCLES_L1D_PENDING"},

    {INTEL_IVB_EX, "RESOURCE_STALLS.ANY"},
    {INTEL_IVB_EX, "CYCLE_ACTIVITY.CYCLES_L2_PENDING"},
    {INTEL_IVB_EX, "CYCLE_ACTIVITY.CYCLES_L1D_PENDING"},

    {INTEL_SKX,    "RESOURCE_STALLS.ANY"},
    {INTEL_SKX,    "CYCLE_ACTIVITY.CYCLES_MEM_ANY"},
    {INTEL_SKX,    "CYCLE_ACTIVITY.CYCLES_L3_MISS"},
    {INTEL_SKX,    "CYCLE_ACTIVITY.CYCLES_L2_MISS"},
    {INTEL_SKX,    "CYCLE_ACTIVITY.CYCLES_L1D_MISS"},

    {INTEL_BDX,    "RESOURCE_STALLS.ANY"},
    {INTEL_BDX,    "CYCLE_ACTIVITY.CYCLES_MEM_ANY"},
    {INTEL_BDX,    "CYCLE_ACTIVITY.CYCLES_L2_MISS"},
    {INTEL_BDX,    "CYCLE_ACTIVITY.CYCLES_L1D_MISS"},

    {INTEL_HSX,    "RESOURCE_STALLS.ANY"},
    {INTEL_HSX,    "CYCLE_ACTIVITY.CYCLES_L1D_PENDING"},
    {INTEL_HSX,    "CYCLE_ACTIVITY.CYCLES_L2_PENDING"},
    {INTEL_HSX,    "CYCLE_ACTIVITY.CYCLES_L1D_PENDING"},

    {INTEL_NHM_EX, "RESOURCE_STALLS.ANY"}
};

static enum pluginStatus_t {UNINITIALIZED, INITIALIZED} status = UNINITIALIZED;


/******************************************************************************
 * PRIVATE method definitions
 *****************************************************************************/

static event_accept_type_t
stall_post_handler(event_handler_arg_t *args)
{
  // no special treatment needed

  return ACCEPT_EVENT;
}


/******************************************************************************
 * PUBLIC method definitions
 *****************************************************************************/

int
stall_hw_register(sample_source_t *self,
                  event_custom_t  *event,
                  struct event_threshold_s *period)
{
  u64 sample_type     = PERF_SAMPLE_PERIOD;
  cpu_type_t cpu_type = get_cpuid();
  int num_pmu         = 0;

  int size = sizeof(stall_events)/sizeof(struct pmu_config_s);

  for(int i=0; i<size ; i++) {
    if (stall_events[i].cpu != cpu_type)
      continue;

    struct perf_event_attr event_attr;
    memset(&event_attr, 0, sizeof(event_attr));

    if (pfmu_getEventAttribute(stall_events[i].event, &event_attr) < 0) {
      EMSG("Cannot initialize event %s", stall_events[i].event);
      continue;
    }
    bool is_period = period->threshold_type == PERIOD;
    perf_util_attr_init(stall_events[i].event, &event_attr, is_period, period->threshold_num, sample_type);

    num_pmu++;

    // ------------------------------------------
    // create metric data centric
    // ------------------------------------------
    int metric = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period(
          metric, stall_events[i].event,
          MetricFlags_ValFmt_Real, 1, metric_property_none);

    // ------------------------------------------
    // Register the event to the global list
    // ------------------------------------------
    event_info_t *einfo  = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
    einfo->metric_custom = event;
    einfo->id            = stall_events[i].event;

    memcpy(&einfo->attr, &event_attr, sizeof(struct perf_event_attr));

    int threshold = 1;
    if (is_period) threshold = period->threshold_num;

    METHOD_CALL(self, store_event_and_info,
                event_attr.config,     /* event id     */
                threshold,             /* threshold    */
                metric,                /* metric id    */
                einfo                  /* info pointer */ );
  }

  return num_pmu;
}

void
stall_init()
{
  if (status == UNINITIALIZED) {
    event_custom_t *event_stall     = hpcrun_malloc(sizeof(event_custom_t));
    event_stall->name         = EVNAME_STALL;
    event_stall->desc         = "Experimental counter: counting stalls.";
    event_stall->register_fn  = stall_hw_register;   // call backs
    event_stall->handle_type  = EXCLUSIVE;// call me only for my events

    event_stall->post_handler_fn   = stall_post_handler;
    event_stall->pre_handler_fn    = NULL;

    event_custom_register(event_stall);
  }
}




