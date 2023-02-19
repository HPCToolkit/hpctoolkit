// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
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

//***************************************************************************
//
// File:
//   gpu-cct.c
//
// Purpose:
//   A general interface for inserting new CCT nodes
//
//***************************************************************************

//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-cct.h"

//******************************************************************************
// interface operations
//******************************************************************************


void
gpu_cct_insert
(
 cct_node_t *cct_node,
 ip_normalized_t ip
)
{
  // if the phaceholder was previously inserted, it will have a child
  // we only want to insert a child if there isn't one already. if the
  // node contains a child already, then the gpu monitoring thread
  // may be adding children to the splay tree of children. in that case
  // trying to add a child here (which will turn into a lookup of the
  // previously added child, would race with any insertions by the
  // GPU monitoring thread.
  //
  // INVARIANT: avoid a race modifying the splay tree of children by
  // not attempting to insert a child in a worker thread when a child
  // is already present
  if (hpcrun_cct_children(cct_node) == NULL) {
    cct_node_t *new_node =
      hpcrun_cct_insert_ip_norm(cct_node, ip, true);
    hpcrun_cct_retain(new_node);
  }
}


cct_node_t *
gpu_cct_insert_always
(
 cct_node_t *cct_node,
 ip_normalized_t ip
)
{
  cct_node_t *node = hpcrun_cct_insert_ip_norm(cct_node, ip, true);
  hpcrun_cct_retain(node);

  return node;
}
