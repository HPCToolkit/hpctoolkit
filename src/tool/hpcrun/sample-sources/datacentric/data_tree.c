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

#include "include/queue.h" // Singly-linkled list macros

#include "data_tree.h"

/******************************************************************************
 * type definitions
 *****************************************************************************/

struct list_data_thread_s {
  struct datainfo_s **root_ptr;
  int    thread_id;

  SLIST_ENTRY(events_list_s) entries;
};


/******************************************************************************
 * private data
 *****************************************************************************/

static struct datainfo_s __thread *datacentric_tree_root = NULL;
static spinlock_t memtree_lock = SPINLOCK_UNLOCKED;

static SLIST_HEAD(list_head, list_data_thread_s) list_data_thread_head =
	SLIST_HEAD_INITIALIZER(list_head);



/******************************************************************************
 * thread data operations
 *****************************************************************************/

static struct datainfo_s **
thread_find(int thread_id)
{
  struct list_data_thread_s *item = NULL;

  // check if we already have the event
  SLIST_FOREACH(item, &list_data_thread_head, entries) {
    if (item != NULL && item->thread_id == thread_id)
      return item->root_ptr;
  }
  return NULL;
}


static int
thread_add(int thread_id, struct datainfo_s **root_ptr)
{
  spinlock_lock(&memtree_lock);

  int return_value = 0;
  struct list_data_thread_s* item = thread_find(thread_id);

  if (item == NULL) {
    item = hpcrun_malloc(sizeof (struct list_data_thread_s));
    item->thread_id = thread_id;
    item->root_ptr  = root_ptr;

    SLIST_INSERT_HEAD(&list_data_thread_head, item, entries);

    return_value = 1 ;
  } 
  spinlock_unlock(&memtree_lock);

  return return_value;
}


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

/* interface for data-centric analysis */
static cct_node_t *
splay_lookup_with_root(struct datainfo_s *root, void *key, void **start, void **end)
{
  if(!root || !key) {
    return NULL;
  }

  root = interval_splay(root, key);
  if(!root) {
    return NULL;
  }
  if((root->memblock <= key) && (root->rmemblock > key)) {
    *start = root->memblock;
    *end   = root->rmemblock;
    return root->context;
  }
  return NULL;
}


/*
 * Insert a node
 */ 
void
splay_insert(struct datainfo_s *node)
{
  void *memblock = node->memblock;

  node->left = node->right = NULL;

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
}

/*
 * remove a node containing a memory block
 */ 
struct datainfo_s *
splay_delete(void *memblock)
{
  struct datainfo_s *result = NULL;

  if (datacentric_tree_root == NULL) {
    return NULL;
  }

  datacentric_tree_root = splay(datacentric_tree_root, memblock);

  if (memblock != datacentric_tree_root->memblock) {
    return NULL;
  }

  result = datacentric_tree_root;

  if (datacentric_tree_root->left == NULL) {
    datacentric_tree_root = datacentric_tree_root->right;
    return result;
  }

  datacentric_tree_root->left = splay(datacentric_tree_root->left, memblock);
  datacentric_tree_root->left->right = datacentric_tree_root->right;
  datacentric_tree_root =  datacentric_tree_root->left;
  return result;
}


/* interface for data-centric analysis */
cct_node_t *
splay_lookup(void *key, void **start, void **end)
{
  return splay_lookup_with_root(datacentric_tree_root, key, start, end);
}
