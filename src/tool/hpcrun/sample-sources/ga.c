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

//***************************************************************************
// system includes
//***************************************************************************

#include <stddef.h>
#include <stdio.h>
#include <string.h>


//***************************************************************************
// local includes
//***************************************************************************

#include <sample-sources/simple_oo.h>
#include <sample-sources/sample_source_obj.h>
#include <sample-sources/common.h>
#include <sample-sources/ga.h>

#include <hpcrun/metrics.h>
#include <hpcrun/thread_data.h>
#include <messages/messages.h>
#include <utilities/tokenize.h>


//***************************************************************************
// local variables
//***************************************************************************

const static long periodDefault = 293;

uint64_t hpcrun_ga_period = 0;

int hpcrun_ga_metricId_onesidedOp    = -1;
int hpcrun_ga_metricId_collectiveOp  = -1;
int hpcrun_ga_metricId_latency       = -1;
int hpcrun_ga_metricId_latencyExcess = -1;
int hpcrun_ga_metricId_bytesXfr      = -1;

#if (GA_DataCentric_Prototype)
int hpcrun_ga_metricId_dataTblIdx_next = -1;
int hpcrun_ga_metricId_dataTblIdx_max  = -1; // exclusive upper bound

hpcrun_ga_metricId_dataDesc_t hpcrun_ga_metricId_dataTbl[hpcrun_ga_metricId_dataTblSz];
#endif // GA_DataCentric_Prototype


//***************************************************************************
// method definitions
//***************************************************************************

static void
METHOD_FN(init)
{
  TMSG(GA, "init");
  self->state = INIT;

  hpcrun_ga_metricId_onesidedOp    = -1;
  hpcrun_ga_metricId_collectiveOp  = -1;
  hpcrun_ga_metricId_latency       = -1;
  hpcrun_ga_metricId_latencyExcess = -1;
  hpcrun_ga_metricId_bytesXfr      = -1;

#if (GA_DataCentric_Prototype)
  hpcrun_ga_metricId_dataTblIdx_next = 0;
  hpcrun_ga_metricId_dataTblIdx_max = hpcrun_ga_metricId_dataTblSz;
#endif
}


static void
METHOD_FN(thread_init)
{
  TMSG(GA, "thread init (no-op)");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(GA, "thread init action (no-op)");
}


static void
METHOD_FN(start)
{
  TMSG(GA, "starting GA sample source");
  TD_GET(ss_state)[self->sel_idx] = START;
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(GA, "thread fini action (no-op)");
}


static void
METHOD_FN(stop)
{
  TMSG(GA, "stopping GA sample source");
  TD_GET(ss_state)[self->sel_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  TMSG(GA, "shutdown GA sample source");
  METHOD_CALL(self, stop);
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  // FIXME: this message comes too early and goes to stderr instead of
  // the log file.
  // TMSG(GA, "test support event: %s", ev_str);
  return hpcrun_ev_is(ev_str, "GA");
}


static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(GA, "create GA metrics");

  char* evStr = METHOD_CALL(self, get_event_str);

  hpcrun_ga_period = periodDefault;
  char evName[32];
  hpcrun_extract_ev_thresh(evStr, sizeof(evName), evName,
			   (long*)&hpcrun_ga_period, periodDefault);
  
  TMSG(GA, "GA: %s sampling period: %"PRIu64, evName, hpcrun_ga_period);

  // number of one-sided operations
  hpcrun_ga_metricId_onesidedOp = hpcrun_new_metric();
  hpcrun_set_metric_info(hpcrun_ga_metricId_onesidedOp, "1-sided op");

  // number of collective operations
  hpcrun_ga_metricId_collectiveOp = hpcrun_new_metric();
  hpcrun_set_metric_info(hpcrun_ga_metricId_collectiveOp, "collective op");

  // exposed latency
  hpcrun_ga_metricId_latency = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(hpcrun_ga_metricId_latency, "latency (us)",
				    MetricFlags_ValFmt_Real, 1, metric_property_none);

  // exposed excess latency
  //hpcrun_ga_metricId_latencyExcess = hpcrun_new_metric();
  //hpcrun_set_metric_info(hpcrun_ga_metricId_latencyExcess, "exs lat (us)");

  hpcrun_ga_metricId_bytesXfr = hpcrun_new_metric();
  hpcrun_set_metric_info(hpcrun_ga_metricId_bytesXfr, "bytes xfr");

#if (GA_DataCentric_Prototype)
  for (int i = 0; i < hpcrun_ga_metricId_dataTblSz; ++i) {
    hpcrun_ga_metricId_dataDesc_t* desc = hpcrun_ga_metricId_dataTbl_find(i);
    desc->metricId = hpcrun_new_metric();
    snprintf(desc->name, hpcrun_ga_metricId_dataStrLen, "ga-data-%d", i);
    desc->name[hpcrun_ga_metricId_dataStrLen - 1] = '\0';
    hpcrun_set_metric_info(desc->metricId, desc->name);
  }
#endif
}


static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  TMSG(GA, "gen event set (no-op)");
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available Global Arrays events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("GA\t\tCollect Global Arrays metrics by sampling GA operations;\n");
  printf("\t\tconfigurable sample period (default %ld ops/sample)\n", periodDefault);
  printf("\n");
}


//***************************************************************************
// object
//***************************************************************************

// sync class is "SS_SOFTWARE" so that both synchronous and
// asynchronous sampling is possible

#define ss_name ga
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


//***************************************************************************
// interface functions
//***************************************************************************

#if (GA_DataCentric_Prototype)
int
hpcrun_ga_dataIdx_new(const char* name)
{
  if (hpcrun_ga_metricId_dataTblIdx_next < hpcrun_ga_metricId_dataTblIdx_max) {
    int idx = hpcrun_ga_metricId_dataTblIdx_next;
    hpcrun_ga_metricId_dataTblIdx_next++;

    hpcrun_ga_metricId_dataDesc_t* desc = hpcrun_ga_metricId_dataTbl_find(idx);
    strncpy(desc->name, name, hpcrun_ga_metricId_dataStrLen);
    desc->name[hpcrun_ga_metricId_dataStrLen - 1] = '\0';
    //hpcrun_set_metric_name(desc->metricId, desc->name);

    TMSG(GA, "hpcrun_ga_dataIdx_new: %s -> metric %d", name, desc->metricId);

    return idx;
  }
  else {
    return -1;
  }
}
#endif // GA_DataCentric_Prototype
