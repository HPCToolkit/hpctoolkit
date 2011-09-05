// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/data-splay.c $
// $Id: data-splay.c 3575 2011-08-02 23:52:25Z xl10 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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
 * standard include files
 *****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>

/******************************************************************************
 * local include files
 *****************************************************************************/

#include <hpcrun/sample-sources/data-splay.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <utilities/arch/inline-asm-gctxt.h>

/******************************************************************************
 * private data
 *****************************************************************************/

static struct datainfo_s *datacentric_tree_root = NULL;
static spinlock_t datacentric_memtree_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * splay operations
 *****************************************************************************/

struct datainfo_s *
splay(struct datainfo_s *root, void *key)
{
  INTERVAL_SPLAY_TREE(datainfo_s, root, key, start, end, left, right);
  return root;
}

void
splay_insert(struct datainfo_s *node)
{
  struct datainfo_s *t;
 
  if(node->start >= node->end) {
    TMSG(MEMLEAK, "In splay insert: start is greater or equal than end");
    return;
  }

  spinlock_lock(&datacentric_memtree_lock);
  if(datacentric_tree_root == NULL) {
    node->left = NULL;
    node->right = NULL;
    datacentric_tree_root = node;
    spinlock_unlock(&datacentric_memtree_lock);
    return;
  }

  node->left = node->right = NULL;
  
  if(datacentric_tree_root != NULL) {
    datacentric_tree_root = splay(datacentric_tree_root, node->start);
    /* insert left of the root */
    if(node->end <= datacentric_tree_root->start) {
      node->left = datacentric_tree_root->left;
      node->right = datacentric_tree_root;
      datacentric_tree_root->left = NULL;
      datacentric_tree_root = node;
      spinlock_unlock(&datacentric_memtree_lock);
      return;
    }
    /* insert right of the root */
    t = splay(datacentric_tree_root->right, datacentric_tree_root->start);
    if((datacentric_tree_root->end <= node->start) &&
       (t == NULL || node->end <= t->start)) {
      node->left = datacentric_tree_root;
      node->right = t;
      datacentric_tree_root->right = NULL;
      datacentric_tree_root = node;
      spinlock_unlock(&datacentric_memtree_lock);
      return;
    }

    datacentric_tree_root->right = t;
    spinlock_unlock(&datacentric_memtree_lock);
    return;
  }
}

struct datainfo_s *
splay_delete(void *start, void *end)
{
  struct datainfo_s *ltree, *rtree, *t, *del;

  spinlock_lock(&datacentric_memtree_lock);
  /* empty tree */
  if(datacentric_tree_root == NULL) {
    spinlock_unlock(&datacentric_memtree_lock);
    TMSG(MEMLEAK, "datacentric splay tree empty: unable to delete %p", start);
    return NULL;
  }

  t = splay(datacentric_tree_root, start);
  if(t->end <= start) {
    ltree = t;
    t = t->right;
    ltree->right = NULL;
  } else {
    ltree = t->left;
    t->left = NULL;
  }

  t = splay(t, end);
  if(t == NULL) {
    del = NULL;
    rtree = NULL;
  } else if(end <= t->start) {
    del = t->left;
    rtree = t;
    t->left = NULL;
  } else {
    del = t;
    rtree = t->right;
    t->right = NULL;
  }

  /* combine the left and right pieces to make the new tree */
  if(ltree == NULL) {
    datacentric_tree_root = rtree;
  } else if(rtree == NULL) {
    datacentric_tree_root = ltree;
  } else {
    datacentric_tree_root = splay(ltree, end);
    datacentric_tree_root->right = rtree;
  }
  
  spinlock_unlock(&datacentric_memtree_lock);

  return del;
}

struct datainfo_s *
splay_lookup(void *p)
{
  datacentric_tree_root = splay(datacentric_tree_root, p);
  if(datacentric_tree_root != NULL &&
     datacentric_tree_root->start <= p &&
     p < datacentric_tree_root->end)
    return datacentric_tree_root;
  return NULL;
}

