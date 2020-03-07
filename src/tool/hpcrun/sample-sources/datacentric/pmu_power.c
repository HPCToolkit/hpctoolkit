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
// Copyright ((c)) 2002-2019, Rice University
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

#include <sample-sources/perf/pmu.h>

#include "datacentric.h"
#include "pmu_handler.h"

// list of precise events

#define EVNAME_POWER_RMEM	  "PM_MRK_DATA_FROM_RMEM"
#define EVNAME_POWER_DL4	  "PM_MRK_DATA_FROM_DL4"
#define EVNAME_POWER_LMEM 	  "PM_MRK_DATA_FROM_LMEM"
#define EVNAME_POWER_LL4	  "PM_MRK_DATA_FROM_LL4"
#define EVNAME_POWER_L2MISS       "PM_MRK_DATA_FROM_L2MISS"
#define EVNAME_POWER_OFFCHIP	  "PM_MRK_DATA_FROM_OFF_CHIP_CACHE"

#define EVNAME_POWER_DL4_CYC	  "PM_MRK_DATA_FROM_DL4_CYC"
#define EVNAME_POWER_LMEM_CYC 	  "PM_MRK_DATA_FROM_LMEM_CYC"
#define EVNAME_POWER_RMEM_CYC	  "PM_MRK_DATA_FROM_RMEM_CYC"
#define EVNAME_POWER_L2MISS_CYC   "PM_MRK_DATA_FROM_L2MISS_CYC"

/**
 * attention: the order of the array is very important. 
 * 
 * Some events are taken from Linux definition:
 *
 * https://github.com/torvalds/linux/blob/master/arch/powerpc/perf/power9-events-list.h
 */
static struct pmu_config_s  pmu_events[] = {
   
   { POWER9,   "PM_MRK_INST_CMPL/0x34340",  "perf_raw::r34340401e0" },   // see Linux Power 9 support mem-loads
   { POWER9,   "PM_MRK_INST_CMPL/0x343c0",  "perf_raw::r343c0401e0" },   // see Linux Power 9 support mem-stores

   { POWER8,   "PM_MRK_INST_CMPL/0x10",     "perf_raw::r10401e0"},   // see Linux Power 8 support mem-access

   { POWER7,   "RMEM",      EVNAME_POWER_RMEM },
   { POWER7,   "DIST-L4",   EVNAME_POWER_DL4  },
   { POWER7,   "LOCAL-MEM", EVNAME_POWER_LMEM }
};



void
datacentric_hw_handler(perf_mmap_data_t *mmap_data,
                       cct_node_t *datacentric_node,
                       cct_node_t *sample_node)
{
  pmu_handler_callback(mmap_data, datacentric_node, sample_node);
}

int
datacentric_hw_register(sample_source_t *self, 
                        kind_info_t     *kb_kind, 
                        event_custom_t  *event,
                        struct event_threshold_s *period)
{
  int size = sizeof(pmu_events)/sizeof(struct pmu_config_s);
  u64 sample_type = PERF_SAMPLE_CALLCHAIN
                    | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                    | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
           ;

  cpu_type_t cpu_type = get_cpuid();
  int num_pmu = 0;

  for(int i=0; i<size ; i++) {
    if (pmu_events[i].cpu != cpu_type)
      continue;

    struct perf_event_attr event_attr;

    int isPMU = pfmu_getEventAttribute(pmu_events[i].event, &event_attr);
    if (isPMU < 0) continue;

    //set_default_perf_event_attr(event_attr, period);
    bool is_period = period->threshold_type == PERIOD;

    perf_util_attr_init(pmu_events[i].event, &event_attr, 
                        is_period,  period->threshold_num, 
                        sample_type);

    perf_skid_set_max_precise_ip(&event_attr);

    // testing the feasibility;
    int ret = perf_event_open(&event_attr,
			THREAD_SELF, CPU_ANY, GROUP_FD, PERF_FLAGS);

    if (ret >= 0) {
      close(ret);
      num_pmu++;

      // ------------------------------------------
      // create metric data centric
      // ------------------------------------------
      int metric = hpcrun_set_new_metric_info_and_period(
            kb_kind, pmu_events[i].name,
            MetricFlags_ValFmt_Int, 1, metric_property_none);

      // ------------------------------------------
      // Register the event to the global list
      // ------------------------------------------
      event_info_t *einfo  = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
      einfo->metric_custom = event;
      memcpy(&einfo->attr, &event_attr, sizeof(struct perf_event_attr));

      METHOD_CALL(self, store_event_and_info,
                  event_attr.config,	/* event id     */
                  1,              	/* threshold    */
                  metric,         	/* metric id    */
                  einfo           	/* info pointer */ );

    }
  }
  if (num_pmu > 0)
    pmu_handler_init(kb_kind);

  return num_pmu;
}
