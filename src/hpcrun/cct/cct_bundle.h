// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef CCT_BUNDLE_H
#define CCT_BUNDLE_H
#include <stdbool.h>

#include "cct.h"
#include "cct_ctxt.h"

#include "../cct2metrics.h" // need to be placed after cct.h

//
// Data type not opaque : FIXME ??
//

typedef struct cct_bundle_t {

  cct_node_t* top;                // top level tree (at creation)

  cct_node_t* tree_root;          // main cct for full unwinds from samples
                                  // that terminate in main:
                                  //    1) All unthreaded cases
                                  //    2) All threaded cases where thread stack
                                  //       has been synthetically attached to main
                                  //       (as in some implementations of OpenMP)

  cct_node_t* thread_root;        // root for full unwinds that terminate in 'pthread_create'

  cct_node_t* partial_unw_root;   // adjunct tree for partial unwinds

  cct_node_t* unresolved_root;    // special collection of ccts for omp deferred context

  cct_node_t* special_idle_node;  // node to signify "idle" resource (used by trace facility).

  cct_node_t* special_no_thread_node; // trace node when outside the thread

  cct_ctxt_t* ctxt;               // creation context for bundle

  unsigned long num_nodes;        // utility to count nodes. NB: MIGHT go away
} cct_bundle_t;



//
// Interface procedures
//

extern void hpcrun_cct_bundle_init(cct_bundle_t* bundle, cct_ctxt_t* ctxt);
//
// IO for cct bundle
//
extern int hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* x,
                         cct2metrics_t* cct2metrics_map, hpcrun_fmt_sparse_metrics_t* sparse_metrics);


//
// utility functions
//
extern bool hpcrun_empty_cct(cct_bundle_t* cct);
extern cct_node_t* hpcrun_cct_bundle_get_idle_node(cct_bundle_t* cct);
extern cct_node_t* hpcrun_cct_bundle_get_no_activity_node(cct_bundle_t* cct);

#endif // CCT_BUNDLE_H
