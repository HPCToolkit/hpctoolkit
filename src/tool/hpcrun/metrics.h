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

#ifndef METRICS_H
#define METRICS_H

#include <sys/types.h>
#include <stdbool.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

// tallent: I have moved flags into hpcfile_csprof.h.  The flags don't
// really belong there but:
// 1) metrics.c uses hpcfile_hpcrun_data_t to implement metrics
//    info, which already confuses boundaries
// 2) metric info needs to exist in a library so csprof (hpcrun),
//    xcsprof (hpcprof) and hpcfile can use it.  hpcfile at least
//    satisfies this.

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

typedef hpcrun_metricVal_t cct_metric_data_t;

// abstract the metric set
//
typedef struct metric_set_t metric_set_t;
typedef struct metric_data_list_t metric_data_list_t;

typedef void metric_upd_proc_t(int metric_id, metric_data_list_t* set, cct_metric_data_t datum);

typedef cct_metric_data_t (*metric_bin_fn)(cct_metric_data_t v1, cct_metric_data_t v2);

//YUMENG
struct metric_position_t{
  uint16_t mid;
  uint64_t offset;
};
typedef struct metric_position_t metric_position_t;

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
// For complicated metric assignment, hpcrun_switch_metric_kind(kind),
// and hpcrun_get_new_metric_of_kind(kind) enable fine-grain control
// of metric sloc allocation
//
// Default case is 1 kind ("STD")
//
// Future expansion to permit different strategies is possible, but
// unimplemented at this time

typedef struct kind_info_t kind_info_t;

kind_info_t* hpcrun_metrics_new_kind();

void hpcrun_close_kind(kind_info_t *kind);

void hpcrun_pre_allocate_metrics(size_t num);

int hpcrun_get_num_metrics(kind_info_t *kind);

void hpcrun_metrics_data_finalize();

int hpcrun_get_num_kind_metrics(void);

metric_data_list_t* hpcrun_reify_metric_data_list_kind(metric_data_list_t* rv, int metric_id);

metric_desc_t* hpcrun_id2metric(int id);

void hpcrun_metrics_data_dump();

// non finalizing
metric_desc_t* hpcrun_id2metric_linked(int metric_id);

// non finalizing
void hpcrun_set_display(int metric_id, uint8_t show);

void hpcrun_set_percent(int metric_id, uint8_t show_percent);

metric_desc_p_tbl_t* hpcrun_get_metric_tbl(kind_info_t**);

metric_upd_proc_t* hpcrun_get_metric_proc(int metric_id);

int hpcrun_set_new_metric_info_w_fn(kind_info_t *kind, const char* name,
				    MetricFlags_ValFmt_t valFmt, size_t period,
				    metric_upd_proc_t upd_fn, metric_desc_properties_t prop);

int hpcrun_set_new_metric_desc(kind_info_t *kind, const char* name,
		        const char *description,
				MetricFlags_ValFmt_t valFmt, size_t period,
				metric_upd_proc_t upd_fn, metric_desc_properties_t prop);

int hpcrun_set_new_metric_desc_and_period(kind_info_t *kind, const char* name, const char *description,
				      MetricFlags_ValFmt_t valFmt, size_t period, metric_desc_properties_t prop);

int hpcrun_set_new_metric_info_and_period(kind_info_t *kind, const char* name,
					  MetricFlags_ValFmt_t valFmt, size_t period, metric_desc_properties_t prop);

int hpcrun_set_new_metric_info(kind_info_t *kind, const char* name);

void hpcrun_set_metric_name(int metric_id, char* name);

// metric set operations

extern cct_metric_data_t* hpcrun_metric_set_loc(metric_data_list_t* rv, int id);
extern void hpcrun_metric_std_set(int metric_id, metric_data_list_t* set,
				  hpcrun_metricVal_t value);
extern void hpcrun_metric_std_inc(int metric_id, metric_data_list_t* set,
				  hpcrun_metricVal_t incr);
extern metric_data_list_t* hpcrun_new_metric_data_list(int metric_id);
extern metric_data_list_t* hpcrun_new_metric_data_list_kind(kind_info_t *kind);
extern metric_data_list_t* hpcrun_new_metric_data_list_kind_final(kind_info_t *kind);

//
// copy a metric set
//
extern void hpcrun_metric_set_dense_copy(cct_metric_data_t* dest,
					 metric_data_list_t* list,
					 int num_metrics);

//
// make a sparse copy - YUMENG
//
//extern void datalist_display(metric_data_list_t *data_list);

extern uint64_t hpcrun_metric_set_sparse_copy(cct_metric_data_t* val, uint16_t* metric_ids,
					 metric_data_list_t* list, int initializing_offset);

extern uint64_t hpcrun_metric_sparse_count(metric_data_list_t* list);



extern metric_data_list_t *hpcrun_merge_cct_metrics(metric_data_list_t *dest, metric_data_list_t *source);

#endif // METRICS_H
