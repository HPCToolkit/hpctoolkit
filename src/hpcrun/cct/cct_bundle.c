// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../common/lean/hpcrun-fmt.h"
#include "../../common/lean/placeholders.h"

#include "../hpcrun_return_codes.h"
#include "../messages/messages.h"
#include "../unresolved.h"

#include "cct_bundle.h"
#include "cct_addr.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_cct_bundle_init(cct_bundle_t* bundle, cct_ctxt_t* ctxt)
{
  bundle->top = hpcrun_cct_new();
  bundle->tree_root = bundle->top;

  bundle->thread_root = bundle->tree_root;
  bundle->ctxt = ctxt;
  bundle->num_nodes = 0;
  // FIXME: change to omp style here
  //
  // If there is a creation context (ie, this is a pthread),
  // then the creation context gets special treatment.
  //

  // If the -dd flag ATTACH_THREAD_CTXT is *set*, then
  // attach all thread-stopped call paths
  // to the call context prefix node instead of the top of the tree.
  //
  // By default (ATTACH_THREAD_CTXT is *cleared*), then attach
  // all thread-stopped call paths to thread root.
  //
  if (ENABLED(ATTACH_THREAD_CTXT) && ctxt) {
    hpcrun_cct_insert_path(&(bundle->thread_root), ctxt->context);
  }
  bundle->partial_unw_root = hpcrun_cct_new_partial();
  bundle->unresolved_root = hpcrun_cct_top_new(UNRESOLVED_ROOT, 0);
}

//
// Write to file for cct bundle:
//
int
hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* bndl,
                         cct2metrics_t* cct2metrics_map, hpcrun_fmt_sparse_metrics_t* sparse_metrics)
{
  if (!fs) { return HPCRUN_ERR; }

  cct_node_t* final = bndl->tree_root;
  cct_node_t* partial_insert = final;


  //
  // attach partial unwinds at appointed slot
  //
  hpcrun_cct_insert_node(partial_insert, bndl->partial_unw_root);

  //
  // attach unresolved root
  //
  // FIXME: show UNRESOLVED TREE
//  hpcrun_cct_insert_node(partial_insert, bndl->unresolved_root);

  // write out newly constructed cct
  return hpcrun_cct_fwrite(cct2metrics_map, bndl->top, fs, flags, sparse_metrics);
}

//
// cct_fwrite helpers
//

//
// utility functions
//
bool
hpcrun_empty_cct(cct_bundle_t* cct)
{
  bool rv = (cct->num_nodes <= 1);
  if (rv) {
    TMSG(CCT, "cct %p is empty", cct);
  }
  return rv;
}


cct_node_t*
hpcrun_cct_bundle_get_no_activity_node
(
 cct_bundle_t* cct
)
{
  cct_node_t *no_activity_cct = NULL;
  if (cct) {
    no_activity_cct = hpcrun_cct_insert_ip_norm(cct->top,
        get_placeholder_norm(hpcrun_placeholder_no_activity), true);
  }
  return no_activity_cct;
}
