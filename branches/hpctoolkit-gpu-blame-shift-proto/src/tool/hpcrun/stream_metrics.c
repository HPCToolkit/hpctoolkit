// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/metrics.c $
// $Id: metrics.c 3629 2011-11-02 20:17:11Z mfagan $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "stream.h"

//struct  metric_set_t {
//  hpcrun_metricVal_t v1;
//};

//*************************** Local Data **************************

void
stream_cct_metric_data_increment(stream_data_t *st, int metric_id,
			  cct_node_t* x,
			  cct_metric_data_t incr)
{
  if (! hpcrun_stream_has_metric_set(st, x)) {
		metric_set_t *mtr = hpcrun_malloc(st->metrics->n_metrics * sizeof(hpcrun_metricVal_t));
    stream_cct2metrics_assoc(st, x, mtr);
  }
  metric_set_t* set = hpcrun_stream_get_metric_set(st, x);
  
  hpcrun_stream_metric_std_inc(st, metric_id, set, incr);
}

metric_set_t*
hpcrun_stream_get_metric_set(stream_data_t *st, cct_node_id_t cct_id)
{
  cct2metrics_t* map = st->cct2metrics_map;
  TMSG(CCT2METRICS, "GET_METRIC_SET for %p, using map %p", cct_id, map);
  if (! map) return NULL;

  map = stream_cct_metrics_splay(map, cct_id);
  st->cct2metrics_map = map;
  TMSG(CCT2METRICS, " -- After Splay map = %p", cct_id, map);

  if (map->node == cct_id) {
    TMSG(CCT2METRICS, " -- found %p, returning metrics", map->node);
    return map->metrics;
  }
  TMSG(CCT2METRICS, " -- cct_id NOT, found. Return NULL");
  return NULL;
}

//
// check to see if node already has metrics
//
bool
hpcrun_stream_has_metric_set(stream_data_t *st, cct_node_id_t cct_id)
{
  return (hpcrun_stream_get_metric_set(st, cct_id) != NULL);
}

//
// associate a metric set with a cct node
//
void stream_cct2metrics_assoc(stream_data_t *st, cct_node_id_t node, metric_set_t* metrics)
{
  cct2metrics_t* map = st->cct2metrics_map;
  TMSG(CCT2METRICS, "CCT2METRICS_ASSOC for %p, using map %p", node, map);
  if (! map) {
    map = cct2metrics_new(node, metrics);
    TMSG(CCT2METRICS, " -- new map created: %p", map);
  }
  else {
    cct2metrics_t* new = cct2metrics_new(node, metrics);
    map = stream_cct_metrics_splay(map, node);
    TMSG(CCT2METRICS, " -- map after splay = %p,"
         " node sought = %p, mapnode = %p", map, node, map->node);
    if (map->node == node) {
      EMSG("CCT2METRICS map assoc invariant violated");
    }
    else {
      if (map->node < node) {
        TMSG(CCT2METRICS, " -- less-than insert %p < %p", map->node, node);
        new->left = map;
        new->right = map->right;
        map->right = NULL;
      }
      else {
        TMSG(CCT2METRICS, " -- greater-than insert %p > %p", map->node, node);
        new->left = map->left;
        new->right = map;
        map->left = NULL;
      }
      map = new;
      TMSG(CCT2METRICS, " -- new map after insertion %p.(%p, %p)", map->node, map->left, map->right);
    }
  }
  st->cct2metrics_map = map;
  TMSG(CCT2METRICS, "METRICS_ASSOC final, THREAD_LOCAL_MAP = %p", st->cct2metrics_map);
  //if (ENABLED(CCT2METRICS)) splay_tree_dump(THREAD_LOCAL_MAP());
}




kind_info_t*
hpcrun_stream_metrics_new_kind(void)
{
  kind_info_t* rv = (kind_info_t*) hpcrun_malloc(sizeof(kind_info_t));
  *rv = (kind_info_t) {.idx = 0, .link = NULL};
  current_insert->link = rv;
  current_insert = rv;
  current_kind = rv;
  return rv;
}

void
hpcrun_stream_metrics_switch_kind(stream_data_t *st, kind_info_t* kind)
{
  st->metrics->current_kind = kind;
}


//***************************************************************************
//  Local functions
//***************************************************************************


//***************************************************************************
//  Interface functions
//***************************************************************************

bool
hpcrun_stream_metrics_finalized(stream_data_t *st)
{
  return st->metrics->has_set_max_metrics;
}


void
hpcrun_stream_pre_allocate_metrics(stream_data_t *st, size_t num)
{
  if (st->metrics->has_set_max_metrics) {
    return;
  }
  for(int i=0; i < num; i++){
    metric_list_t* n = (metric_list_t*) hpcrun_malloc(sizeof(metric_list_t));
    n->next = st->metrics->pre_alloc;
    st->metrics->pre_alloc = n;
  }
  // NOTE: actual metric count not incremented until a new metric is requested
}


//
// first call to get_num_metrics will finalize
// the metric info table
//
int
hpcrun_stream_get_num_metrics(stream_data_t *st)
{
  //
  // create id->descriptor table, metric_tbl, and metric_proc tbl
  //
  if (! st->metrics->has_set_max_metrics) {
     st->metrics->id2metric = hpcrun_malloc( st->metrics->n_metrics * sizeof(metric_desc_t*));
     st->metrics->metric_tbl.len =  st->metrics->n_metrics;
     st->metrics->metric_tbl.lst =  st->metrics->id2metric;
    for(metric_list_t* l =  st->metrics->metric_data; l; l = l->next){
      TMSG(METRICS_FINALIZE,"metric_tbl[%d] = %s", l->id, l->val.name);
      st->metrics->id2metric[l->id] = &(l->val);
    }
    st->metrics->metric_proc_tbl = (metric_upd_proc_t**) hpcrun_malloc(st->metrics->n_metrics * sizeof(metric_upd_proc_t*));
    
    for(metric_proc_map_t* l = st->metrics->proc_map; l; l = l->next) {
    //    for(metric_proc_map_t* l = proc_map; l; l = l->next) {
      TMSG(METRICS_FINALIZE, "metric_proc[%d] = %p", l->id, l->proc);
      st->metrics->metric_proc_tbl[l->id] = l->proc;
    }
  // *** TEMPORARY ***
  // *** create a "NULL METRICS" dense array for use with
  // *** metric set dense copy

    st->metrics->null_metrics = (hpcrun_metricVal_t*) hpcrun_stream_metric_set_new(st);
    for (int i = 0; i < st->metrics->n_metrics; i++) {
      st->metrics->null_metrics[i].bits = 0;
    }
  }
  st->metrics->has_set_max_metrics = true;

  return st->metrics->n_metrics;
}


metric_desc_t*
hpcrun_stream_id2metric(stream_data_t *st, int id)
{
  if ((0 <= id) && (id < st->metrics->n_metrics)) {
    return st->metrics->id2metric[id];
  }
  return NULL;
}


metric_desc_p_tbl_t*
hpcrun_stream_get_metric_tbl(stream_data_t *st)
{
  hpcrun_stream_get_num_metrics(st); // make sure metric table finalized
                            // in case of no samples
  return &st->metrics->metric_tbl;
}


metric_list_t*
hpcrun_stream_get_metric_data(stream_data_t *st)
{
  return st->metrics->metric_data;
}


// *** FIXME? metric finalization may not be necessary
// when metric set repr changes
//
metric_upd_proc_t*
hpcrun_stream_get_metric_proc(stream_data_t *st, int metric_id)
{
  hpcrun_stream_get_num_metrics(st); // ensure that metrics are finalized
  return st->metrics->metric_proc_tbl[metric_id];
}

//
// Allocate new metric of a particular kind
//
int
hpcrun_stream_new_metric_of_kind(stream_data_t *st, kind_info_t* kind)
{
  if (st->metrics->has_set_max_metrics) {
    return 0;
  }

  metric_list_t* n = NULL;

  // if there are pre-allocated metrics, use them
  if (st->metrics->pre_alloc) {
    n = st->metrics->pre_alloc;
    st->metrics->pre_alloc = st->metrics->pre_alloc->next;
  }
  else {
    n = (metric_list_t*) hpcrun_malloc(sizeof(metric_list_t));
  }
  n->next = st->metrics->metric_data;
  n->id   = st->metrics->n_metrics;
  st->metrics->metric_data = n;

  kind->idx++;

  st->metrics->n_metrics++;
  
  //
  // No preallocation for metric_proc tbl
  //
  metric_proc_map_t* m = (metric_proc_map_t*) hpcrun_malloc(sizeof(metric_proc_map_t));
  m->next = st->metrics->proc_map;
  m->id   = st->metrics->metric_data->id;
  m->proc = (metric_upd_proc_t*) NULL;
  st->metrics->proc_map = m;
  
  return st->metrics->metric_data->id;
}

int
hpcrun_stream_new_metric(stream_data_t *st)
{
  return hpcrun_stream_new_metric_of_kind(st, st->metrics->current_kind);
}

void
hpcrun_stream_set_metric_info_w_fn(stream_data_t *st, int metric_id, const char* name,
			    MetricFlags_ValFmt_t valFmt, size_t period,
			    metric_upd_proc_t upd_fn)
{
  if (st->metrics->has_set_max_metrics) {
    return;
  }

  metric_desc_t* mdesc = NULL;
  for (metric_list_t* l = st->metrics->metric_data; l; l = l->next) {
    if (l->id == metric_id) {
      mdesc = &(l->val);
      break;
    }
  }
  TMSG(METRICS,"id = %d, name = %s, flags = %d, period = %d", metric_id, name, valFmt, period);
  if (! mdesc) {
    EMSG("Metric id is NULL (likely unallocated)");
    monitor_real_abort();
  }
  if (name == NULL) {
    EMSG("Must supply a name for metric");
    monitor_real_abort();
  }

  *mdesc = metricDesc_NULL;
  mdesc->flags = hpcrun_metricFlags_NULL;

  mdesc->name = (char*) name;
  mdesc->description = (char*) name; // TODO
  mdesc->period = period;
  mdesc->flags.fields.ty     = MetricFlags_Ty_Raw; // FIXME
  mdesc->flags.fields.valFmt = valFmt;
  mdesc->formula = NULL;
  mdesc->format = NULL;

  //
  // manage metric proc mapping
  //
  for (metric_proc_map_t* l = st->metrics->proc_map; l; l = l->next){
    if (l->id == metric_id){
      l->proc = upd_fn;
      break;
    }
  }
}


void
hpcrun_stream_set_metric_info_and_period(stream_data_t *st, int metric_id, const char* name,
				  MetricFlags_ValFmt_t valFmt, size_t period)
{
  hpcrun_stream_set_metric_info_w_fn(st, metric_id, name, valFmt, period,
			      hpcrun_metric_std_inc);
}


//
// utility routine to make an Async metric with period 1
//
void
hpcrun_stream_set_metric_info(stream_data_t *st, int metric_id, const char* name)
{
  hpcrun_stream_set_metric_info_and_period(st, metric_id, name, MetricFlags_ValFmt_Int, 1);
}


//
// hpcrun_set_metric_name function is primarily used by
// synchronous sample sources that need to name the metrics
// after they have been allocated.
//
// makes a call to hpcrun_get_num_metrics just to ensure that
// metric_tbl has been finalized.
//
void
hpcrun_stream_set_metric_name(stream_data_t *st, int metric_id, char* name)
{
  hpcrun_stream_get_num_metrics(st);
  st->metrics->id2metric[metric_id]->name        = name;
  st->metrics->id2metric[metric_id]->description = name;
}

//
// Metric set interface
//



metric_set_t*
hpcrun_stream_metric_set_new(stream_data_t *st)
{
  return hpcrun_malloc(st->metrics->n_metrics * sizeof(hpcrun_metricVal_t));
}

//
// return an lvalue from metric_set_t*
//
cct_metric_data_t*
hpcrun_stream_metric_set_loc(metric_set_t* s, int id)
{
  return &(s->v1) + id;
}


void
hpcrun_stream_metric_std_inc(stream_data_t *st, int metric_id, metric_set_t* set,
		      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_stream_id2metric(st, metric_id);
  hpcrun_metricVal_t* loc = hpcrun_stream_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      loc->i += incr.i; break;
    case MetricFlags_ValFmt_Real:
      loc->r += incr.r; break;
    default:
      assert(false);
  }
}

//
// copy a metric set
//
void
hpcrun_stream_metric_set_dense_copy(stream_data_t *st, cct_metric_data_t* dest,
			     metric_set_t* set,
			     int num_metrics)
{
  metric_set_t* actual = set ? set : (metric_set_t*) st->metrics->null_metrics;
  memcpy((char*) dest, (char*) actual, num_metrics * sizeof(cct_metric_data_t));
}
