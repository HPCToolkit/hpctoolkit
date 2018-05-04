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

#include <stdio.h>

#include <hpcrun/messages/messages.h>
#include "sample-sources/perf/perf-util.h"

#include "datacentric.h"

#define IBS_OP_TYPE_FILE "/sys/bus/event_source/devices/ibs_op/type"

#define PERF_IBS_CONFIG   (1ULL<<19)

int
datacentric_hw_register(sample_source_t *self, event_custom_t *event,
                        struct event_threshold_s *period, event_info_t **event_info)
{
  // get the type of ibs op

  FILE *ibs_file = fopen(IBS_OP_TYPE_FILE, "r");
  u32 type = 0;

  if (!ibs_file) {
    EMSG("Cannot open file: %s", IBS_OP_TYPE_FILE);
    return 0;
  }
  fscanf(ibs_file, "%d", &type);
  event_info_t *einfo = (event_info_t *) hpcrun_malloc(sizeof(event_info_t));
  memset(einfo, 0, sizeof(event_info_t));

  einfo->attr.config = PERF_IBS_CONFIG;
  einfo->attr.type   = type;
  einfo->attr.freq   = period->threshold_type == FREQUENCY ? 1 : 0;

  einfo->attr.sample_period  = period->threshold_num;

  u64 sample_type    = PERF_SAMPLE_RAW
                        | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                        | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
                        | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID;

  bool is_period = period->threshold_type == PERIOD;
  perf_util_attr_init(&einfo->attr, is_period, period->threshold_num, sample_type);
  perf_util_set_max_precise_ip(&einfo->attr);

  // testing the feasibility;
  int ret = perf_util_event_open( &einfo->attr,
    THREAD_SELF, CPU_ANY, GROUP_FD, PERF_FLAGS);

  if (ret >= 0) {
    close(ret);
    return 0;
  }

  einfo->metric_custom = event;

  // ------------------------------------------
  // create metric data centric
  // ------------------------------------------
  int metric = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(
        metric, "IBS-OP",
        MetricFlags_ValFmt_Int, 1, metric_property_none);

  // ------------------------------------------
  // Register the event to the global list
  // ------------------------------------------
  METHOD_CALL(self, store_event_and_info,
      einfo->attr.config, 1, hpcrun_new_metric(), einfo );;

  *event_info = einfo;

  return 1;
}

