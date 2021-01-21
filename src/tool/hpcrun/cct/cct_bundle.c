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

//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-fmt.h>

#include <hpcrun/hpcrun_return_codes.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/hpcrun-placeholders.h>

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
#if 0
int 
hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* bndl,
                         cct2metrics_t* cct2metrics_map)           
#else
//YUMENG: add sparse_metrics to collect metric values and info
int 
hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* bndl,
                         cct2metrics_t* cct2metrics_map, hpcrun_fmt_sparse_metrics_t* sparse_metrics)
#endif
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
#if 0
  return hpcrun_cct_fwrite(cct2metrics_map, bndl->top, fs, flags);
#else 
//YUMENG: add sparse_metrics to collect metric values and info
  return hpcrun_cct_fwrite(cct2metrics_map, bndl->top, fs, flags, sparse_metrics);
#endif
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
    placeholder_t *nap = 
      hpcrun_placeholder_get(hpcrun_placeholder_type_no_activity);
    no_activity_cct = 
      hpcrun_cct_insert_ip_norm(cct->top, nap->pc_norm); 
  }
  return no_activity_cct;
}
