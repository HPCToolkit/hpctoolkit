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
// Copyright ((c)) 2002-2017, Rice University
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
#include "sample-sources/perf/perf-util.h"
#include "sample-sources/perf/perfmon-util.h"

#include "datacentric.h"

// list of precise events

#define EVNAME_SANDYBRIDGE_LATENCY  "snb::MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"
#define EVNAME_SANDYBRIDGE_STORE    "snb::MEM_TRANS_RETIRED:PRECISE_STORE"

#define EVNAME_KNL_OFFCORE_RESP_0   "knl::OFFCORE_RESPONSE_0"
#define EVNAME_KNL_CACHE_L2_HIT     "knl::MEM_UOPS_RETIRED:LD_L2_HIT"

/**
 * attention: the order of the array is very important.
 * It has to start from event from the latest architecture
 * to the old one, since sometimes newer architecture still keep
 * compatibility with the old ones.
 */
static const char *evnames[] = {
    EVNAME_KNL_OFFCORE_RESP_0,
    EVNAME_KNL_CACHE_L2_HIT,

    EVNAME_SANDYBRIDGE_LATENCY,
    EVNAME_SANDYBRIDGE_STORE
};

static void
set_default_perf_event_attr(struct perf_event_attr *attr, struct event_threshold_s *period)
{
	attr->size = sizeof(struct perf_event_attr);

	attr->sample_period  = period->threshold_num;
	attr->freq           = period->threshold_type == FREQUENCY ? 1 : 0;

	attr->sample_type    = PERF_SAMPLE_RAW | PERF_SAMPLE_CALLCHAIN
							 | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
							 | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
							 | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
							 | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
	#endif
										;
	attr->disabled      = 1;
	attr->sample_id_all = 1;
	attr->exclude_hv     = EXCLUDE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
	attr->exclude_callchain_kernel = INCLUDE_CALLCHAIN;
	attr->exclude_callchain_user = EXCLUDE_CALLCHAIN;
#endif
}

int
datacentric_hw_register(event_info_t *event_desc, struct event_threshold_s *period)
{
  int size = sizeof(evnames)/sizeof(const char*);
  struct perf_event_attr *event_attr = &(event_desc->attr);
  u64 sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_CALLCHAIN
                    | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                    | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
                    | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
           ;

  for(int i=0; i<size ; i++) {
    int isPMU = pfmu_getEventAttribute(evnames[i], event_attr);
    if (isPMU < 0) continue;

    //set_default_perf_event_attr(event_attr, period);
    bool is_period = period->threshold_type == PERIOD;
    perf_util_attr_init(event_attr, is_period, period->threshold_num, sample_type);
    perf_util_set_max_precise_ip(event_attr);

    // testing the feasibility;
    int ret = perf_util_event_open(event_attr,
			THREAD_SELF, CPU_ANY,
			GROUP_FD, PERF_FLAGS);
    if (ret >= 0) {
      close(ret);
      // just quit when the returned value is correct
      break;
    }
  }
  return 1;
}
