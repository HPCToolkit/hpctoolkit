// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

/* metrics.c -- informing the system about what metrics to record */
#if defined(__ia64__) && defined(__linux__)
#include <libunwind.h>
#endif

#include <stdbool.h>

#include "epoch.h"
#include "metrics.h"
#include "state.h"
#include "monitor.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

/* total number of metrics we can track simultaneously */
static int csprof_max_metrics = 0;
/* information about tracked metrics */

static metric_tbl_t metric_data;

/* setting the total number of metrics to track */
static int has_set_max_metrics = false;

bool
hpcrun_metrics_finalized(void)
{
  return has_set_max_metrics;
}

int
csprof_get_max_metrics()
{
  if (! has_set_max_metrics) {
    EMSG("WARNING: csprof_get_max_metrics called BEFORE metrics are finalized!");
  }
  return csprof_max_metrics;
}

int
csprof_set_max_metrics(int max_metrics)
{
  TMSG(METRICS,"Set max metrics to %d", max_metrics);
  if(has_set_max_metrics) {
    TMSG(METRICS,"Set max metrics called AGAIN with %d", max_metrics);
    return -1;
  }
  else {
    TMSG(METRICS,"Set 'max_set' flag. Initialize metric data");
    has_set_max_metrics = true;
    csprof_max_metrics = max_metrics;

    TMSG(METRICS," set_max_metrics");
    metric_data.lst = csprof_malloc(max_metrics * sizeof(metric_desc_t));
    return max_metrics;
  }
}

metric_tbl_t*
hpcrun_get_metric_data(void)
{
  return &metric_data;
}

int
csprof_num_recorded_metrics(void)
{
  return metric_data.len;
}

/* providing hooks for the user to define their own metrics */

int
csprof_new_metric()
{
  if(metric_data.len >= csprof_max_metrics) {
    EMSG("Unable to allocate any more metrics");
    return -1;
  }
  else {
    int the_metric = metric_data.len;
    metric_data.len++;
    return the_metric;
  }
}

void
csprof_set_metric_info_and_period(int metric_id, char *name,
				  hpcrun_metricFlags_t flags, size_t period)
{
  TMSG(METRICS,"id = %d, name = %s, flags = %lx, period = %d", metric_id, name,flags, period);
  if(metric_id >= metric_data.len) {
    EMSG("Metric id `%d' is not a defined metric", metric_id);
  }
  if(name == NULL) {
    EMSG("Must supply a name for metric `%d'", metric_id);
    monitor_real_abort();
  }
  { 
    metric_desc_t *metric = &(metric_data.lst[metric_id]);
    metric->name = name;
    metric->period = period;
    metric->flags = flags;
  }
}

void
csprof_set_metric_info(int metric_id, char *name, hpcrun_metricFlags_t flags)
{
  csprof_set_metric_info_and_period(metric_id, name, flags, 1);
}

#if 0
void
csprof_record_metric(int metric_id, size_t value)
{

#ifdef NO
  /* step out of c_r_m_w_u and c_r_m */
  csprof_record_metric_with_unwind(metric_id, value, 2);
#endif
  /** Try it with 1 **/
  csprof_record_metric_with_unwind(metric_id, value, 1);
}

#endif
