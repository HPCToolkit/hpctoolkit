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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef CCT2METRICS_H
#define CCT2METRICS_H

//
// cct_node -> metrics map
//
#include "cct/cct.h"

#include "hpcrun/metrics.h"

#include <stdbool.h>

//
// ******** Typedef ********
//
//
typedef struct cct2metrics_t cct2metrics_t;
//
// ******** initialization
//
// The cct2metrics map gets explicit reference
// only at initialization time.
//
// Thereafter, the interface ops refer to it
// implicitly.
// [Singleton pattern for GangOf4 enthusiasts]

extern void hpcrun_cct2metrics_init(cct2metrics_t** map);

// ******** Interface operations **********
//

//
// for a given cct node, return the metric set
// associated with it.
//
// if there are no metrics for the node, then
// create a metric set, and return it.
//
extern metric_data_list_t* hpcrun_reify_metric_set(cct_node_id_t cct_id, int metric_id);

//
// get metric data list for a node (NULL value is ok).
//
extern metric_data_list_t*
hpcrun_get_metric_data_list_specific(cct2metrics_t** map, cct_node_id_t cct_id);

extern metric_data_list_t* hpcrun_get_metric_data_list(cct_node_id_t cct_id);

//
// move metric data list from one node to another
//
extern metric_data_list_t* hpcrun_move_metric_data_list_specific(
    cct2metrics_t** map, cct_node_id_t dest_id, cct_node_id_t source_id);

extern metric_data_list_t*
hpcrun_move_metric_data_list(cct_node_id_t dest_id, cct_node_id_t source_id);

extern void cct2metrics_assoc(cct_node_t* node, metric_data_list_t* kind_metrics);

// extern cct2metrics_t* cct2metrics_new(cct_node_id_t node, metric_set_t** kind_metrics);

typedef enum { SET, INCR } update_metric_t;

static inline void
cct_metric_data_update(int metric_id, cct_node_t* x, update_metric_t type, cct_metric_data_t incr) {
  metric_data_list_t* set = hpcrun_reify_metric_set(x, metric_id);

  if (type == SET)
    hpcrun_metric_std_set(metric_id, set, incr);
  else if (type == INCR)
    hpcrun_metric_std_inc(metric_id, set, incr);
}

static inline void cct_metric_data_set(int metric_id, cct_node_t* x, cct_metric_data_t val) {
  cct_metric_data_update(metric_id, x, SET, val);
}

static inline void cct_metric_data_increment(int metric_id, cct_node_t* x, cct_metric_data_t incr) {
  cct_metric_data_update(metric_id, x, INCR, incr);
}

#endif  // CCT2METRICS_H
