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

#ifndef CCT_BUNDLE_H
#define CCT_BUNDLE_H
#include <stdbool.h>

#include "cct.h"
#include "cct_ctxt.h"

#include <hpcrun/cct2metrics.h> // need to be placed after cct.h

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
#if 0
extern int hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* x,
                                    cct2metrics_t* cct2metrics_map);

#else
//YUMENG: add sparse_metrics to collect metric values and info
extern int hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* x,
                         cct2metrics_t* cct2metrics_map, hpcrun_fmt_sparse_metrics_t* sparse_metrics);
#endif


//
// utility functions
//
extern bool hpcrun_empty_cct(cct_bundle_t* cct);
extern cct_node_t* hpcrun_cct_bundle_get_idle_node(cct_bundle_t* cct);
extern cct_node_t* hpcrun_cct_bundle_get_no_activity_node(cct_bundle_t* cct);

#endif // CCT_BUNDLE_H
