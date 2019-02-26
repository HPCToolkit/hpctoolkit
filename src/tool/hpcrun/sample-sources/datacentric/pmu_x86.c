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

#include <hpcrun/messages/messages.h>
#include <utilities/arch/cpuid.h>

#include "sample-sources/perf/perf_event_open.h"
#include "sample-sources/perf/perf-util.h"
#include "sample-sources/perf/perf_skid.h"
#include "sample-sources/perf/perfmon-util.h"
#include "sample-sources/perf/event_custom.h"

#include "datacentric.h"
#include "pmu_x86.h"


int
datacentric_hw_register(sample_source_t *self, event_custom_t *event,
                        struct event_threshold_s *period)
{
  int size = sizeof(pmu_events)/sizeof(struct pmu_config_s);
  u64 sample_type = PERF_SAMPLE_CALLCHAIN
                    | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                    | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
                    | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
           ;

  int num_pmu = 0;
  cpu_type_t cpu_type = get_cpuid();

  for(int i=0; i<size ; i++) {

    if (pmu_events[i].cpu != cpu_type)
      continue;

    struct perf_event_attr event_attr;
    memset(&event_attr, 0, sizeof(event_attr));

    if (pfmu_getEventAttribute(pmu_events[i].event, &event_attr) < 0) {
      EMSG("Cannot initialize event %s", pmu_events[i].event);
      continue;
    }

    //set_default_perf_event_attr(event_attr, period);
    bool is_period = period->threshold_type == PERIOD;
    perf_util_attr_init(pmu_events[i].event, &event_attr, is_period, period->threshold_num, sample_type);
    perf_skid_set_max_precise_ip(&event_attr);

    num_pmu++;

    // ------------------------------------------
    // create metric data centric
    // ------------------------------------------
    int metric = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period(
          metric, pmu_events[i].event,
          MetricFlags_ValFmt_Int, 1, metric_property_none);

    // ------------------------------------------
    // Register the event to the global list
    // ------------------------------------------
    event_info_t *einfo  = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
    einfo->metric_custom = event;
    memcpy(&einfo->attr, &event_attr, sizeof(struct perf_event_attr));

    METHOD_CALL(self, store_event_and_info,
                event_attr.config,     /* event id     */
                1,              /* threshold    */
                metric,         /* metric id    */
                einfo           /* info pointer */ );

  }
  return num_pmu;
}
