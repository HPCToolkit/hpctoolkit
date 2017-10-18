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
// Copyright ((c)) 2002-2012, Rice University
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

#include "data_tree.h"

/******************************************************************************
 * type definitions
 *****************************************************************************/


/******************************************************************************
 * private data
 *****************************************************************************/

static struct datainfo_s *datacentric_tree_root = NULL;
static spinlock_t memtree_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * splay operations
 *****************************************************************************/


static struct datainfo_s *
splay(struct datainfo_s *root, void *key)
{
  REGULAR_SPLAY_TREE(datainfo_s, root, key, memblock, left, right);
  return root;
}

static struct datainfo_s *
interval_splay(struct datainfo_s *root, void *key)
{
  INTERVAL_SPLAY_TREE(datainfo_s, root, key, memblock, rmemblock, left, right);
  return root;
}

/*
 * Insert a node
 */ 
void
splay_insert(struct datainfo_s *node)
{
  void *memblock = node->memblock;

  node->left = node->right = NULL;

  spinlock_lock(&memtree_lock);  
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
    } else {
      TMSG(DATACENTRIC, "datacentric splay tree: unable to insert %p (already present)", 
	   node->memblock);
      assert(0);
    }
  }
  datacentric_tree_root = node;
  spinlock_unlock(&memtree_lock);  
}

/*
 * remove a node containing a memory block
 */ 
struct datainfo_s *
splay_delete(void *memblock)
{
  struct datainfo_s *result = NULL;

  spinlock_lock(&memtree_lock);  
  if (datacentric_tree_root == NULL) {
    spinlock_unlock(&memtree_lock);  
    TMSG(DATACENTRIC, "datacentric splay tree empty: unable to delete %p", memblock);
    return NULL;
  }

  datacentric_tree_root = splay(datacentric_tree_root, memblock);

  if (memblock != datacentric_tree_root->memblock) {
    spinlock_unlock(&memtree_lock);  
    TMSG(DATACENTRIC, "datacentric splay tree: %p not in tree", memblock);
    return NULL;
  }

  result = datacentric_tree_root;

  if (datacentric_tree_root->left == NULL) {
    datacentric_tree_root = datacentric_tree_root->right;
    spinlock_unlock(&memtree_lock);  
    return result;
  }

  datacentric_tree_root->left = splay(datacentric_tree_root->left, memblock);
  datacentric_tree_root->left->right = datacentric_tree_root->right;
  datacentric_tree_root =  datacentric_tree_root->left;
  spinlock_unlock(&memtree_lock);  
  return result;
}

/* interface for data-centric analysis */
cct_node_t *
splay_lookup(void *key, void **start, void **end)
{
  if(!datacentric_tree_root || !key) {
    return NULL;
  }

  struct datainfo_s *info;
  spinlock_lock(&memtree_lock);  
  datacentric_tree_root = interval_splay(datacentric_tree_root, key);
  info = datacentric_tree_root;
  if(!info) {
    spinlock_unlock(&memtree_lock);  
    return NULL;
  }
  if((info->memblock <= key) && (info->rmemblock > key)) {
    *start = info->memblock;
    *end = info->rmemblock;
    spinlock_unlock(&memtree_lock);  
    return info->context;
  }
  spinlock_unlock(&memtree_lock);  
  return NULL;
}


