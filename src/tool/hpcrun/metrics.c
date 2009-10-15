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
#include <stdlib.h>

#include "epoch.h"
#include "metrics.h"
#include "state.h"
#include "monitor.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

// number of metrics requested
static int n_metrics = 0;

// information about tracked metrics
static metric_list_t* metric_data = NULL;

// flag to indicate that metric allocation is finalized
static bool has_set_max_metrics = false;

// some sample sources will pre-allocate some metrics ...
static metric_list_t* pre_alloc = NULL;

// need an index-->metric desc mapping, so that samples will increment metrics correctly
static metric_desc_t** id2metric;

// local metric_tbl serves 2 purposes:
//    1) mapping from metric_id ==> metric desc, so that samples will increment correct metric slot
//       in the cct node
//    2) metric info is written out in metric_tbl form
//
static metric_desc_p_tbl_t metric_tbl;

bool
hpcrun_metrics_finalized(void)
{
  return has_set_max_metrics;
}


void
hpcrun_pre_allocate_metrics(size_t num)
{
  if (has_set_max_metrics) {
    return;
  }
  for(int i=0; i < num; i++){
    metric_list_t* n = (metric_list_t*) hpcrun_malloc(sizeof(metric_list_t));
    n->next = pre_alloc;
    pre_alloc = n;
  }
  // NOTE: actual metric count not incremented until a new metric is requested
}

//
// first call to get_num_metrics will finalize
// the metric info table
//
int
hpcrun_get_num_metrics(void){
  //
  // create id->descriptor table and metric_tbl
  //
  if (!has_set_max_metrics) {
    id2metric = hpcrun_malloc(n_metrics * sizeof(metric_desc_t*));
    metric_tbl.len = n_metrics;
    metric_tbl.lst = id2metric;
    for(metric_list_t* l = metric_data; l; l = l->next){
      TMSG(METRICS_FINALIZE,"metric_tbl[%d] = %s", l->id, l->val.name);
      id2metric[l->id] = &(l->val);
    }
  }
  has_set_max_metrics = true;
  return n_metrics;
}

metric_desc_t*
hpcrun_id2metric(int id)
{
  if ((0 <= id) && (id < n_metrics)) {
    return id2metric[id];
  }
  return NULL;
}

metric_desc_p_tbl_t*
hpcrun_get_metric_tbl(void)
{
  return &metric_tbl;
}

metric_list_t*
hpcrun_get_metric_data(void)
{
  return metric_data;
}

int
hpcrun_new_metric(void)
{
  if (has_set_max_metrics) {
    return 0;
  }

  metric_list_t* n = NULL;

  // if there are pre-alllocated metrics, use them
  if (pre_alloc) {
    n = pre_alloc;
    pre_alloc = pre_alloc->next;
  }
  else {
    n = (metric_list_t*) hpcrun_malloc(sizeof(metric_list_t));
  }
  n->next = metric_data;
  n->id   = n_metrics;
  metric_data = n;
  n_metrics++;

  return metric_data->id;
}

void
hpcrun_set_metric_info_and_period(int metric_id, const char* name,
				  hpcrun_metricFlags_t flags, size_t period)
{
  if (has_set_max_metrics) {
    return;
  }

  metric_desc_t* metric = NULL;
  for (metric_list_t* l = metric_data; l; l = l->next){
    if (l->id == metric_id){
      metric = &(l->val);
      break;
    }
  }
  TMSG(METRICS,"id = %d, name = %s, flags = %lx, period = %d", metric_id, name, flags, period);
  if (! metric) {
    EMSG("Metric id is NULL (likely unallocated)");
    monitor_real_abort();
  }
  if(name == NULL) {
    EMSG("Must supply a name for metric");
    monitor_real_abort();
  }
  metric->name = (char*) name;
  metric->description = (char*) name; // TODO
  metric->period = period;
  metric->flags = flags;
}

void
hpcrun_set_metric_info(int metric_id, const char* name,
		       hpcrun_metricFlags_t flags)
{
  hpcrun_set_metric_info_and_period(metric_id, name, flags, 1);
}
