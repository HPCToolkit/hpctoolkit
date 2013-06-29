// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-omp/src/tool/hpcrun/sample-sources/memleak.c $
// $Id: memleak.c 3680 2012-02-25 22:14:00Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include "simple_oo.h"
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>

#include <messages/messages.h>

static int ma_metric_id = -1;

// Metrics for numa related metrics
static int numa_match_metric_id = -1;
static int numa_mismatch_metric_id = -1;

static int low_offset_metric_id = -1;
static int high_offset_metric_id = -1;

static int *location_metric_id;

/******************************************************************************
 * method definitions
 *****************************************************************************/

void
hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i > incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r > incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}

void
hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i < incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r < incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}


static void
METHOD_FN(init)
{
  self->state = INIT; 

  // reset static variables to their virgin state
  ma_metric_id = -1;
}


static void
METHOD_FN(thread_init)
{
  TMSG(SOFTIBS, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(SOFTIBS, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  TMSG(SOFTIBS,"starting MEMLEAK");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(SOFTIBS, "thread fini (noop)");
}

static void
METHOD_FN(stop)
{
  TMSG(SOFTIBS,"stopping MEMLEAK");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"SOFTIBS") != NULL);
}
 

// Currently SOFTIBS creates one metric: number of memory accesses.

#define DEFAULT_PERIOD 1000000
static long period = DEFAULT_PERIOD;
long ma_period = 0;

static void
METHOD_FN(process_event_list,int lush_metrics)
{
  char* _p = METHOD_CALL(self, get_event_str);

  //
  // EVENT: Only 1 wallclock event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(ITIMER_CTL,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);
  ma_period = period;

  ma_metric_id = hpcrun_new_metric();

    // set up NUMA related metrics
  numa_match_metric_id = hpcrun_new_metric(); /* create numa metric id (access matches data location in NUMA node) */
  numa_mismatch_metric_id = hpcrun_new_metric(); /* create numa metric id (access does not match data location in NUMA node) */
  low_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (low offset) */
  high_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (high offset) */

  // create data location (numa node) metrics
  int numa_node_num = numa_num_configured_nodes();
  location_metric_id = (int *) hpcrun_malloc(numa_node_num * sizeof(int));
  for (int i = 0; i < numa_node_num; i++) {
    location_metric_id[i] = hpcrun_new_metric();
  }

  hpcrun_set_metric_info(ma_metric_id, "MEMORY ACCESSES");
  hpcrun_set_metric_info(numa_match_metric_id, "NUMA_MATCH");
  hpcrun_set_metric_info(numa_mismatch_metric_id, "NUMA_MISMATCH");
  hpcrun_set_metric_info_w_fn(low_offset_metric_id, "LOW_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_min, metric_property_none);
  hpcrun_set_metric_info_w_fn(high_offset_metric_id, "HIGH_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_max, metric_property_none);

  // set numa location metrics
  int j;
  for (j = 0; j < numa_node_num; j++) {
    char metric_name[128];
    sprintf(metric_name, "NUMA_NODE%d", j);
    hpcrun_set_metric_info(location_metric_id[j], strdup(metric_name));
  }
}


//
// Event sets not relevant for this sample source
// Events are generated by user code
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


//
//
//
static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available software IBS detection events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("SOFTIBS\t\tThe sampled memory accesses. Only works with instrumented LLVM code\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

//
// sync class is "SS_SOFTWARE" so that both synchronous and asynchronous sampling is possible
// 

#define ss_name softibs
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally

int
hpcrun_ma_metric_id() 
{
  return ma_metric_id;
}

int 
hpcrun_numa_match_metric_id()
{
  return numa_match_metric_id;
}

int 
hpcrun_numa_mismatch_metric_id()
{
  return numa_mismatch_metric_id;
}

int 
hpcrun_low_offset_metric_id()
{
  return low_offset_metric_id;
}

int 
hpcrun_high_offset_metric_id()
{
  return high_offset_metric_id;
}

int *
hpcrun_location_metric_id()
{
  return location_metric_id;
}

