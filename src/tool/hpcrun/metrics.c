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
// Copyright ((c)) 2002-2021, Rice University
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

#include <stdbool.h>
#include <stdlib.h>

#include <assert.h>

//*************************** User Include Files ****************************

#include "monitor.h"
#include "metrics.h"

#include <memory/hpcrun-malloc.h>

#include <messages/messages.h>

#include <cct/cct.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/spinlock.h>

//*************************** Concrete Data Types ***************************

//
// following typedef accomplishes
// metric_set_t* == array of metric values, but it does this abstractly
// so that clients of the metric datatype must use the interface.
//
struct  metric_set_t {
  hpcrun_metricVal_t v1;
};

typedef struct metric_desc_list_t {
  struct metric_desc_list_t* next;
  metric_desc_t val;
  metric_upd_proc_t*        proc;
  int id;
  int g_id;
} metric_desc_list_t;

//*************************** Local Data **************************

// some sample sources will pre-allocate some metrics ...
static metric_desc_list_t* pre_alloc = NULL;

//
// To accomodate block sparse representation,
// use 'kinds' == dense subarrays of metrics
//
// Sample use:
// Std metrics = 1 class,
// CUDA metrics = another class
//
// To use the mechanism, call hpcrun_metrics_new_kind.
// Then each call to hpcrun_new_metric will yield a slot in the
// new metric kind subarray.
//
// Future expansion to permit different strategies is possible, but
// unimplemented at this time

struct kind_info_t {
  int idx;     // current index in kind
  bool has_set_max;
  kind_info_t* link; // all kinds linked together in singly linked list
  // metric_tbl serves 2 purposes:
  //    1) mapping from metric_id ==> metric desc, so that samples will increment correct metric slot
  //       in the cct node
  //    2) metric info is written out in metric_tbl form
  //
  metric_desc_p_tbl_t metric_tbl;
// Dense array holding "0" values for nodes with no metrics
  hpcrun_metricVal_t *null_metrics;
// information about tracked metrics
  metric_desc_list_t* metric_data;
};

static kind_info_t *first_kind = NULL;
static kind_info_t **next_kind = &first_kind;
typedef enum { KIND_UNINITIALIZED, KIND_INITIALIZING, KIND_INITIALIZED } kind_state_t;
static _Atomic(kind_state_t) kind_state = ATOMIC_VAR_INIT(KIND_UNINITIALIZED);
static int num_kind_metrics; //YUMENG：change to num_total_metrics?
static struct dmap {
  metric_desc_t *desc;
  int id;
  kind_info_t *kind;
  metric_upd_proc_t *proc;
} *metric_data;

kind_info_t*
hpcrun_metrics_new_kind(void)
{
  kind_info_t* rv = (kind_info_t*) hpcrun_malloc(sizeof(kind_info_t));
  *rv = (kind_info_t) {.idx = 0, .metric_data = NULL, .has_set_max = 0, .link = NULL};
  *next_kind = rv;
  next_kind = &rv->link;
  return rv;
}

typedef struct metric_data_list_t {
  struct metric_data_list_t* next;
  kind_info_t *kind;
  metric_set_t *metrics;
} metric_data_list_t;


//***************************************************************************
//  Local functions
//***************************************************************************


//***************************************************************************
//  Interface functions
//***************************************************************************

void
hpcrun_pre_allocate_metrics(size_t num)
{
  for (int i=0; i < num; i++){
    metric_desc_list_t* n = (metric_desc_list_t*) hpcrun_malloc(sizeof(metric_desc_list_t));
    n->next = pre_alloc;
    pre_alloc = n;
  }
  // NOTE: actual metric count not incremented until a new metric is requested
}

const char *
check(int ans)
{
  static const char *answers[] = {"n", "y"};
  return answers[ans];
}


void
hpcrun_metrics_data_dump()
{
  hpcrun_metrics_data_finalize();

  for (kind_info_t *kind = first_kind; kind != NULL; kind = kind->link) {
    hpcrun_get_num_metrics(kind);
    for(metric_desc_list_t* l = kind->metric_data; l; l = l->next) {
      printf("metric_data[%d].(desc=%p (%s), id=%d (%s), kind=%p (%s), proc=%p (%s))\n", 
	     l->g_id,
	     metric_data[l->g_id].desc,
	     check(metric_data[l->g_id].desc == &l->val),
	     metric_data[l->g_id].id,
	     check(metric_data[l->g_id].id == l->id),
	     metric_data[l->g_id].kind, 
	     check(metric_data[l->g_id].kind == kind),
	     metric_data[l->g_id].proc,
	     check(metric_data[l->g_id].proc == l->proc));
    }
  }
}


void
hpcrun_metrics_data_finalize()
{
  if (atomic_load(&kind_state) != KIND_INITIALIZED) {
    kind_state_t old_state = KIND_UNINITIALIZED;
    if (atomic_compare_exchange_strong(&kind_state, &old_state, KIND_INITIALIZING)) {
      metric_data = hpcrun_malloc(num_kind_metrics * sizeof(struct dmap));

      for (kind_info_t *kind = first_kind; kind != NULL; kind = kind->link) {
        hpcrun_get_num_metrics(kind);
        for(metric_desc_list_t* l = kind->metric_data; l; l = l->next) {
          metric_data[l->g_id].desc = &l->val;
          metric_data[l->g_id].id = l->id;
          metric_data[l->g_id].kind = kind;
          metric_data[l->g_id].proc = l->proc;
        }
      }
      atomic_store(&kind_state, KIND_INITIALIZED);
    } else {
      while (atomic_load(&kind_state) != KIND_INITIALIZED);
    }
  }
}

// Note: (johnmc) needs double-checked locking if all_kinds_done not 
// set prior to multithreading
int
hpcrun_get_num_kind_metrics()
{
  hpcrun_metrics_data_finalize();

  return num_kind_metrics;
}

//
// first call to get_num_metrics will finalize
// the metric info table
//
int
hpcrun_get_num_metrics(kind_info_t *kind)
{
  int n_metrics = kind->idx;
  //
  // create id->descriptor table, metric_tbl, and metric_proc tbl
  //
  if (!kind->has_set_max) {
    kind->metric_tbl.len = n_metrics;
    kind->metric_tbl.lst = hpcrun_malloc(n_metrics * sizeof(metric_desc_t*));
    for(metric_desc_list_t* l = kind->metric_data; l; l = l->next)
      kind->metric_tbl.lst[l->id] = &l->val;
  // *** TEMPORARY ***
  // *** create a "NULL METRICS" dense array for use with
  // *** metric set dense copy

    kind->null_metrics = hpcrun_malloc(n_metrics * sizeof(hpcrun_metricVal_t));
    for (int i = 0; i < n_metrics; i++) {
      kind->null_metrics[i].bits = 0;
    }
  }
  kind->has_set_max = true;

  return n_metrics;
}

// Finalize metrics

void hpcrun_close_kind(kind_info_t *kind) 
{
  hpcrun_get_num_metrics(kind);
}

metric_desc_t*
hpcrun_id2metric(int metric_id)
{
  int n_metrics = hpcrun_get_num_kind_metrics();
  if ((0 <= metric_id) && (metric_id < n_metrics)) {
    return metric_data[metric_id].desc;
  }
  return NULL;
}


// non finalizing
metric_desc_t*
hpcrun_id2metric_linked(int metric_id)
{
  for (kind_info_t *kind = first_kind; kind != NULL; kind = kind->link) {
    for(metric_desc_list_t* l = kind->metric_data; l; l = l->next) {
      if (l->g_id == metric_id) return &l->val;
    }
  }
  return NULL;
}


// non finalizing
void hpcrun_set_display(int metric_id, uint8_t show) {
  metric_desc_t* mdesc = hpcrun_id2metric_linked(metric_id);
  mdesc->flags.fields.show = show;
}


// non finalizing
void hpcrun_set_percent(int metric_id, uint8_t show_percent) {
  metric_desc_t* mdesc = hpcrun_id2metric_linked(metric_id);
  mdesc->flags.fields.showPercent = show_percent;
}


metric_desc_p_tbl_t*
hpcrun_get_metric_tbl(kind_info_t **curr)
{
  if (*curr == NULL)
    *curr = first_kind;
  else
    *curr = (*curr)->link;

  return &(*curr)->metric_tbl;
}


// *** FIXME? metric finalization may not be necessary
// when metric set repr changes
//
metric_upd_proc_t*
hpcrun_get_metric_proc(int metric_id)
{
  int n_metrics = hpcrun_get_num_kind_metrics(); // ensure that metrics are finalized

  if ((0 <= metric_id) && (metric_id < n_metrics)) {
    return metric_data[metric_id].proc;
  }

  return NULL;
}

//
// Allocate new metric of a particular kind
//
int
hpcrun_set_new_metric_info_w_fn(kind_info_t *kind, const char* name,
        MetricFlags_ValFmt_t valFmt, size_t period,
        metric_upd_proc_t upd_fn, metric_desc_properties_t prop)
{
  return hpcrun_set_new_metric_desc(kind, name, name, valFmt, period, upd_fn, prop);
}

//
// create a new metric description
// returns the new metric ID
//
int
hpcrun_set_new_metric_desc(kind_info_t *kind, const char* name,
        			const char *description,
				MetricFlags_ValFmt_t valFmt, size_t period,
				metric_upd_proc_t upd_fn, metric_desc_properties_t prop)
{
  if (kind->has_set_max)
    return -1;

  int metric_id = num_kind_metrics++;
  metric_desc_list_t* n = NULL;

  // if there are pre-allocated metrics, use them
  if (pre_alloc) {
    n = pre_alloc;
    pre_alloc = pre_alloc->next;
  }
  else {
    n = (metric_desc_list_t*) hpcrun_malloc(sizeof(metric_desc_list_t));
  }
  n->next = kind->metric_data;
  kind->metric_data = n;
  n->proc = upd_fn;
  n->id   = kind->idx++;
  n->g_id = metric_id;
  
  metric_desc_t* mdesc = &(n->val);
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
  mdesc->description = (char*) description; 
  mdesc->period = period;
  mdesc->flags.fields.ty     = MetricFlags_Ty_Raw; // FIXME
  mdesc->flags.fields.valFmt = valFmt;
  mdesc->formula = NULL;
  mdesc->format = NULL;
  mdesc->properties = prop;
  return metric_id;
}


int
hpcrun_set_new_metric_desc_and_period(kind_info_t *kind, const char* name, const char *description,
				      MetricFlags_ValFmt_t valFmt, size_t period, metric_desc_properties_t prop)
{
  return hpcrun_set_new_metric_desc(kind, name, description, valFmt, period,
					 hpcrun_metric_std_inc, prop);
}

int
hpcrun_set_new_metric_info_and_period(kind_info_t *kind, const char* name,
				      MetricFlags_ValFmt_t valFmt, size_t period, metric_desc_properties_t prop)
{
  return hpcrun_set_new_metric_info_w_fn(kind, name, valFmt, period,
					 hpcrun_metric_std_inc, prop);
}


//
// utility routine to make an Async metric with period 1
//
int
hpcrun_set_new_metric_info(kind_info_t *kind, const char* name)
{
  return hpcrun_set_new_metric_info_and_period(kind, name, MetricFlags_ValFmt_Int, 1,
					       metric_property_none);
}


//
// return an lvalue from metric_set_t*
//
cct_metric_data_t*
hpcrun_metric_set_loc(metric_data_list_t *rv, int id)
{
  metric_data_list_t *curr;
  for (curr = rv; curr != NULL && curr->kind != metric_data[id].kind;
    rv = curr, curr = curr->next);
  if (curr == NULL) {
    curr = hpcrun_new_metric_data_list(id);
    rv->next = curr;
  }
  rv = curr;

  return &(rv->metrics->v1) + metric_data[id].id;
}


void
hpcrun_metric_std(int metric_id, metric_data_list_t* set,
		  char operation, hpcrun_metricVal_t val)
{
  metric_desc_t* minfo = metric_data[metric_id].desc;
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if (operation == '+')
        loc->i += val.i; 
      else if (operation == '=')
        loc->i = val.i;
      break;
    case MetricFlags_ValFmt_Real:
      if (operation == '+')
        loc->r += val.r;
      else if (operation == '=')
        loc->r = val.r;
      break;
    default:
      assert(false);
  }
}
//
// replace the old value with the new value
//
void
hpcrun_metric_std_set(int metric_id, metric_data_list_t* set,
		      hpcrun_metricVal_t value)
{
  hpcrun_metric_std(metric_id, set, '=', value);
}

// increasing the value of metric
//
void
hpcrun_metric_std_inc(int metric_id, metric_data_list_t* set,
		      hpcrun_metricVal_t incr)
{
  hpcrun_metric_std(metric_id, set, '+', incr);
}

metric_data_list_t *
hpcrun_new_metric_data_list(int metric_id)
{
  metric_data_list_t *curr = hpcrun_malloc(sizeof(metric_data_list_t));
  hpcrun_get_num_kind_metrics();
  curr->kind = metric_data[metric_id].kind;
  int n_metrics = hpcrun_get_num_metrics(curr->kind);
  curr->metrics = hpcrun_malloc(n_metrics * sizeof(hpcrun_metricVal_t));
  // FIXME(Keren): duplicate?
  for (int i = 0; i < n_metrics; i++)
    curr->metrics[i].v1 = curr->kind->null_metrics[i];
  memset(curr->metrics, 0, n_metrics * sizeof(hpcrun_metricVal_t));
  curr->next = NULL;
  return curr;
}

metric_data_list_t *
hpcrun_reify_metric_data_list_kind(metric_data_list_t* rv, int metric_id)
{
  kind_info_t *kind = metric_data[metric_id].kind;
  metric_data_list_t *curr = NULL;
  metric_data_list_t *prev = NULL;
  for (curr = rv; curr != NULL && curr->kind != kind; curr = curr->next) {
    prev = curr;
  }
  if (curr == NULL) {
    curr = hpcrun_new_metric_data_list(metric_id);
    if (prev != NULL) {
      prev->next = curr;
    }
  }
  return curr;
}

metric_data_list_t *
hpcrun_new_metric_data_list_kind(kind_info_t *kind)
{
  metric_data_list_t *curr = hpcrun_malloc(sizeof(metric_data_list_t));
  hpcrun_get_num_kind_metrics();
  curr->kind = kind;
  int n_metrics = hpcrun_get_num_metrics(curr->kind);
  curr->metrics = hpcrun_malloc(n_metrics * sizeof(hpcrun_metricVal_t));
  // FIXME(Keren): duplicate?
  for (int i = 0; i < n_metrics; i++)
    curr->metrics[i].v1 = curr->kind->null_metrics[i];
  memset(curr->metrics, 0, n_metrics * sizeof(hpcrun_metricVal_t));
  curr->next = NULL;
  return curr;
}

// only apply this method while writing out thread profile data
metric_data_list_t *
hpcrun_new_metric_data_list_kind_final(kind_info_t *kind)
{
  metric_data_list_t *curr = malloc(sizeof(metric_data_list_t));
  hpcrun_get_num_kind_metrics();
  curr->kind = kind;
  int n_metrics = hpcrun_get_num_metrics(curr->kind);
  curr->metrics = malloc(n_metrics * sizeof(hpcrun_metricVal_t));
  // FIXME(Keren): duplicate?
  for (int i = 0; i < n_metrics; i++)
    curr->metrics[i].v1 = curr->kind->null_metrics[i];
  memset(curr->metrics, 0, n_metrics * sizeof(hpcrun_metricVal_t));
  curr->next = NULL;
  return curr;
}

//
// copy a metric set
//
void
hpcrun_metric_set_dense_copy(cct_metric_data_t* dest,
			     metric_data_list_t* list,
			     int num_metrics)
{
  kind_info_t *curr_k;
  metric_data_list_t *curr;

  for (curr_k = first_kind; curr_k != NULL; curr_k = curr_k->link) {
    for (curr = list; curr != NULL && curr->kind != curr_k; curr = curr->next);
    metric_set_t* actual = curr ? curr->metrics : (metric_set_t*) curr_k->null_metrics;
    memcpy((char*) dest, (char*) actual, curr_k->idx * sizeof(cct_metric_data_t));
    dest += curr_k->idx;
  }
}

//
// make a sparse copy - YUMENG 
//

//used to show what the datalist looks like, test purpose
#if 0
void
datalist_display(metric_data_list_t *data_list)
{
  metric_data_list_t *curr;
  metric_desc_list_t *curr_l;
  for (curr = data_list; curr != NULL; curr = curr->next){

    metric_desc_list_t *metric_list = curr->kind->metric_data;
    printf("; kind info - name:");
    for (curr_l = metric_list; curr_l != NULL; curr_l = curr_l->next) {
      printf("%s ",metric_list->val.name);
    }
    printf("; metrics vals:");
    metric_set_t* actual = curr->metrics;
    for (int i = 0; i < curr->kind->idx; ++i) {
      uint64_t v = actual[i].v1.bits; // local copy of val
      printf(" %d",v);
    /*int shift = 0, num_write = 0, c;
  
    for (shift = 56; shift >= 0; shift -= 8) {
      printf("%ld : ", ((v >> shift) & 0xff));
    }*/
    }
    printf("\n");
  }
  printf("END\n");
  
}
#endif

uint64_t
hpcrun_metric_sparse_count(metric_data_list_t* list)
{
  
  kind_info_t *curr_k;
  metric_data_list_t *curr;
  uint64_t num_nzval = 0;
  
  for (curr_k = first_kind; curr_k != NULL; curr_k = curr_k->link) {
    for (curr = list; curr != NULL && curr->kind != curr_k; curr = curr->next);
    if(curr){
      metric_set_t* actual = curr->metrics;
      for(int i = 0; i < curr_k->idx; i++ ){
        if(actual[i].v1.i != 0){
          num_nzval++;         
        }
      }
    }
  }
  return num_nzval;

}


uint64_t
hpcrun_metric_set_sparse_copy(cct_metric_data_t* val, uint16_t* metric_ids,
			     metric_data_list_t* list, int initializing_idx)
{
  
  kind_info_t *curr_k;
  metric_data_list_t *curr;
  
  cct_metric_data_t curr_m;

  int curr_id = 0;//global id of metrics
  uint64_t num_nzval = 0;//current number of non-zero values

  for (curr_k = first_kind; curr_k != NULL; curr_k = curr_k->link) {
    for (curr = list; curr != NULL && curr->kind != curr_k; curr = curr->next);
    if(curr){     
      metric_set_t* actual = curr->metrics;
      for(int i = 0; i < curr_k->idx; i++ ){
        curr_m = actual[i].v1;
        if(curr_m.i != 0){ 
          val[initializing_idx + num_nzval] = curr_m;
          metric_ids[initializing_idx + num_nzval] = (uint16_t)curr_id;
          num_nzval++;
        }
        curr_id++;
      }
    }else{//if curr not exist, meaning no metrics for this kind, still needs to update the metric id to match the kind
      curr_id += curr_k->idx;
    } 
  }

  //num_nzval will be offset for next cct's metrics
  return num_nzval;

}


//
// merge two metrics list
// pre-condition: dest_list is not NULL
//
metric_data_list_t *
hpcrun_merge_cct_metrics(metric_data_list_t *dest_list, metric_data_list_t *source_list)
{
  metric_data_list_t *curr_source = NULL;
  metric_data_list_t *curr_dest = NULL;

  for (curr_source = source_list; curr_source != NULL; curr_source = curr_source->next) {
    metric_data_list_t *rv = dest_list;
    for (curr_dest = rv; curr_dest != NULL && curr_dest->kind != curr_source->kind;
      rv = curr_dest, curr_dest = curr_dest->next);
    // Allocate a new metric_data_list
    if (curr_dest == NULL) {
      curr_dest = hpcrun_new_metric_data_list_kind_final(curr_source->kind);
      rv->next = curr_dest;
    }
    int n_metrics = hpcrun_get_num_metrics(curr_source->kind);
    for (int i = 0; i < n_metrics; i++)
      curr_dest->metrics[i].v1.i += curr_source->metrics[i].v1.i;
  }

  return dest_list;
}
