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

#define EVNAME_SANDYBRIDGE_LATENCY  "snb::MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"
#define EVNAME_SANDYBRIDGE_STORE    "MEM_TRANS_RETIRED:PRECISE_STORE"

/**
 * attention: the order of the array is very important.
 * It has to start from event from the latest architecture
 * to the old one, since sometimes newer architecture still keep
 * compatibility with the old ones.
 */
static const char *evnames[] = {
    EVNAME_SANDYBRIDGE_LATENCY,
    EVNAME_SANDYBRIDGE_STORE
};

int
datacentric_hw_register(event_info_t *event_desc, struct event_threshold_s *period)
{
  int size = sizeof(evnames)/sizeof(const char*);

  for(int i=0; i<size ; i++) {
    struct perf_event_attr *event_attr = &(event_desc->attr);
    int isPMU = pfmu_getEventAttribute(evnames[i], event_attr);

    if (isPMU >= 0) {
      // testing the feasibility;
      event_desc->attr.precise_ip = get_precise_ip(&(event_desc->attr));  // as precise as possible

      if (event_desc->attr.precise_ip > 0)
        break;
    }
  }

  event_desc->attr.size           = sizeof(struct perf_event_attr);
  event_desc->attr.sample_period  = period->threshold_num;
  event_desc->attr.freq           = period->threshold_type == FREQUENCY ? 1 : 0;

  event_desc->attr.sample_type    = PERF_SAMPLE_RAW  
                                    | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                                    | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
                                    | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
                                    ;
  event_desc->attr.disabled       = 1;
  event_desc->attr.exclude_kernel = 0;
  event_desc->attr.exclude_user   = 0;
  event_desc->attr.exclude_hv     = 0;
  event_desc->attr.exclude_guest  = 0;
  event_desc->attr.exclude_idle   = 0;
  event_desc->attr.exclude_host   = 0;
  event_desc->attr.pinned         = 0;
  event_desc->attr.mmap           = 1;

  event_desc->attr.sample_id_all = 1;
  event_desc->attr.read_format   = 0;
  return 1;
}
