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
// Copyright ((c)) 2002-2017, Rice University
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


/******************************************************************************
 * local include files
 *****************************************************************************/

#include <messages/messages.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>

#include <include/queue.h> // slist

#include <memory/hpcrun-malloc.h>

#include "data_tree.h"

/******************************************************************************
 * type definitions
 *****************************************************************************/




/******************************************************************************
 * private data
 *****************************************************************************/

static spinlock_t datatree_lock = SPINLOCK_UNLOCKED;

static struct datatree_info_s  *datacentric_tree_root = NULL;



/******************************************************************************
 * PRIVATE splay operations
 *****************************************************************************/


static struct datatree_info_s *
splay(struct datatree_info_s *root, void *key)
{
  REGULAR_SPLAY_TREE(datatree_info_s, root, key, memblock, left, right);
  return root;
}

static struct datatree_info_s *
interval_splay(struct datatree_info_s *root, void *key)
{
  INTERVAL_SPLAY_TREE(datatree_info_s, root, key, memblock, rmemblock, left, right);
  return root;
}

/* interface for data-centric analysis */
static struct datatree_info_s *
splay_lookup_with_root(struct datatree_info_s *root, void *key, void **start, void **end)
{
  if(!root || !key) {
    return NULL;
  }

  spinlock_lock(&datatree_lock);

  root = interval_splay(root, key);

  spinlock_unlock(&datatree_lock);

  if(!root) {
    return NULL;
  }
  if((root->memblock <= key) && (root->rmemblock > key)) {
    *start = root->memblock;
    *end   = root->rmemblock;
    return root;
  }
  return NULL;
}


/******************************************************************************
 * PUBLIC splay operations
 *****************************************************************************/


/*
 * Insert a node
 */ 
void
datatree_splay_insert(struct datatree_info_s *node)
{
  void *memblock = node->memblock;

  node->left = node->right = NULL;

  spinlock_lock(&datatree_lock);

  if (datacentric_tree_root != NULL) {

    datacentric_tree_root = splay(datacentric_tree_root, memblock);

    if (memblock < datacentric_tree_root->memblock) {
      node->left = datacentric_tree_root->left;
      node->right = datacentric_tree_root;
      datacentric_tree_root->left = NULL;
    } else if (memblock > datacentric_tree_root->memblock) {
      node->left = datacentric_tree_root;
      node->right = datacentric_tree_root->right;
      datacentric_tree_root->right = NULL;
    }
  }
  datacentric_tree_root = node;

  spinlock_unlock(&datatree_lock);

#if 0
  TMSG(DATACENTRIC, "[%x] add ctx %x addr %x (%d bytes)",
      node->magic, node->context, node->memblock, node->bytes);
#endif
}

/*
 * remove a node containing a memory block
 */ 
struct datatree_info_s *
datatree_splay_delete(void *memblock)
{
  struct datatree_info_s *result = NULL;

  if (datacentric_tree_root == NULL) {
    return NULL;
  }

  spinlock_lock(&datatree_lock);

  datacentric_tree_root = splay(datacentric_tree_root, memblock);

  if (memblock != datacentric_tree_root->memblock) {
    spinlock_unlock(&datatree_lock);
    return NULL;
  }

  result = datacentric_tree_root;


  if (datacentric_tree_root->left == NULL) {
    datacentric_tree_root = datacentric_tree_root->right;
    spinlock_unlock(&datatree_lock);
    return result;
  }

  datacentric_tree_root->left = splay(datacentric_tree_root->left, memblock);
  datacentric_tree_root->left->right = datacentric_tree_root->right;
  datacentric_tree_root =  datacentric_tree_root->left;

  spinlock_unlock(&datatree_lock);
  return result;
}


/* interface for data-centric analysis */
struct datatree_info_s *
datatree_splay_lookup(void *key, void **start, void **end)
{
  return splay_lookup_with_root(datacentric_tree_root, key, start, end);
}
