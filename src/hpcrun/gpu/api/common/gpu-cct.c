// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

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
