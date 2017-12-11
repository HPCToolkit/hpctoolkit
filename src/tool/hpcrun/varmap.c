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

#include <stdlib.h>
#include <assert.h>

#include <memory/hpcrun-malloc.h>
#include <lib/prof-lean/splay-macros.h>

#include "loadmap.h"

#include "varmap.h"

#include "sample-sources/datacentric/data_tree.h"




//splay tree node type for static data
typedef struct static_data_t {
  void *start;
  void *end;
  struct static_data_t *left;
  struct static_data_t *right;
} static_data_t;

struct static_data_t *data_root;


// ----------------------------------------------------------------------
// Private methods
// ----------------------------------------------------------------------

#if 0
static struct static_data_t *
static_data_interval_splay(void *key)
{
  INTERVAL_SPLAY_TREE(static_data_t, data_root, key, start, end, left, right);
  return data_root;
}

static void
static_data_interval_splay_insert(struct static_data_t *node)
{
  void *start = node->start;

  node->left = node->right = NULL;

  if (data_root != NULL) {
    data_root = static_data_interval_splay(start);

    if (start < data_root->start) {
      node->left = data_root->left;
      node->right = data_root;
      data_root->left = NULL;
    } else if (start > data_root->start) {
      node->left = data_root;
      node->right = data_root->right;
      data_root->right = NULL;
    } else {
      assert(0);
    }
  }
}

#endif
// ----------------------------------------------------------------------
// Public methods
// ----------------------------------------------------------------------


// splay tree query
void *
static_data_interval_splay_lookup(void *key, void **start, void **end)
{
#if 0
  if(!data_root) {
    return NULL;
  }

  struct static_data_t *info = static_data_interval_splay(key);

  if(!info) {
    return NULL;
  }
  data_root = info;

  if((info->start <= key) && (info->end > key)) {
    *start = info->start;
    *end = info->end;

    return info->start;
  }
#endif
  cct_node_t *node = splay_lookup(key, start, end);
  return node;
}

#if 0
void *
static_data_lookup(hpcrun_loadmap_t* s_loadmap_ptr, void *key, void **start, void **end, uint16_t *lmid)
{
  for (load_module_t* x = s_loadmap_ptr->lm_head; (x); x = x->next) {
    if (x->dso_info) {
      void *normalized_key = key - x->dso_info->start_to_ref_dist;
      void *result = static_data_interval_splay_lookup(normalized_key, start, end);
      if (result) {
        *lmid = x->id;
        return result;
      }
    }
  }
  return NULL;
}
#endif

void
insert_var_table(dso_info_t *dso, void **var_table, unsigned long num)
{
  if(!var_table) return;
  int i;
  for (i = 0; i < num; i+=2) {
    // create splay node
    /*static_data_t *node = (static_data_t *)hpcrun_malloc(sizeof(static_data_t));
    node->start = var_table[i];
    node->end = var_table[i]+(long)var_table[i+1];
    node->left = node->right = NULL;
    static_data_interval_splay_insert(node);
    */
    struct datainfo_s *data_info = hpcrun_malloc(sizeof(struct datainfo_s));

    data_info->memblock = var_table[i];
    data_info->bytes    = var_table[i+1] - var_table[i];
    data_info->left     = data_info->right = NULL;

    data_info->magic    = DATA_STATIC_MAGIC;
    data_info->context  = (void*) DATA_STATIC_CONTEXT;

    data_info->rmemblock = data_info->memblock + data_info->bytes;

    splay_insert(data_info);
  }
}


