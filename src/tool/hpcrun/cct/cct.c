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

//***************************************************************************
//
// File:
//   cct.c
//
// Purpose:
//   A variable degree-tree for storing call stack samples.  Each node
//   may have zero or more children and each node contains a single
//   instruction pointer value.  Call stack samples are represented
//   implicitly by a path from some node x (where x may or may not be
//   a leaf node) to the tree root (with the root being the bottom of
//   the call stack).
//
//   The basic tree functionality is based on NonUniformDegreeTree.h/C
//   from HPCView/HPCTools.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

//*************************** User Include Files ****************************

#include <memory/hpcrun-malloc.h>
#include <hpcrun/metrics.h>
#include <messages/messages.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/spinlock.h>
#include <hpcrun/hpcrun_return_codes.h>

#include "cct.h"
#include "cct_addr.h"
#include "cct2metrics.h"
#include "../memory/hpcrun-malloc.h"
//#include "../ompt/ompt-interface.h"
//#include "../memory/hpcrun-malloc.h"

#define HPCRUN_CCT_KEEP_DUMMY 1

//***************************** concrete data structure definition **********

struct cct_node_t {

  // ---------------------------------------------------------
  // a persistent node id is assigned for each node. this id
  // is used both to reassemble a tree when reading it from
  // a file as well as to identify call paths. a call path
  // can simply be represented by the node id of the deepest
  // node in the path.
  // ---------------------------------------------------------
  int32_t persistent_id;

 // bundle abstract address components into a data type
  cct_addr_t addr;

  bool is_leaf;

  // ---------------------------------------------------------
  // tree structure
  // ---------------------------------------------------------

  // parent node and the beginning of the child list
  // vi3: also used as a next pointer for freelist of trees
  struct cct_node_t* parent;
  struct cct_node_t* children;

  // left and right pointers for splay tree of siblings
  struct cct_node_t* left;
  struct cct_node_t* right;

};

#if 0
//
// cache of info from most recent splay
//
static struct {
  cct_node_tt* node;
  bool found;
  cct_addr_t* addr;
} splay_cache;
#endif

//
// ******************* Local Routines ********************
//
static uint32_t
new_persistent_id()
{
  // by default, all persistent ids are even; odd ids signify that we need
  // to retain them as call path ids associated with a trace.
  // Furthermore, global ids start at 12: 0,1 are special ids, 2-11 are for
  // users (and conceivably hpcrun).
  //
  static atomic_uint_least32_t global_persistent_id = ATOMIC_VAR_INIT(12);
  return atomic_fetch_add_explicit(&global_persistent_id, 2, memory_order_relaxed);
}

static cct_node_t*
cct_node_create(cct_addr_t* addr, cct_node_t* parent)
{
  size_t sz = sizeof(cct_node_t);
  cct_node_t *node;

  // FIXME: when multiple epochs really work, this will always be freeable.
  // WARN ME (krentel) if/when we really use freeable memory.
  if (ENABLED(FREEABLE)) {
    node = hpcrun_malloc_freeable(sz);
  }
  else {
//    node = hpcrun_malloc(sz);
    node = hpcrun_cct_node_alloc();
  }

  memset(node, 0, sz);

  node->addr.as_info = addr->as_info; // LUSH
  node->addr.ip_norm = addr->ip_norm;
  node->addr.lip = addr->lip;         // LUSH

  node->persistent_id = new_persistent_id();

  node->parent = parent;
  node->children = NULL;
  node->left = NULL;
  node->right = NULL;

  node->is_leaf = false;

  return node;
}

//
// ******* SPLAY TREE section ********
// [ Thanks to Mark Krentel ]
//

//
// local comparison macros
//
// NOTE: argument asymmetry due to
//     type of key value passed in = cct_addr_t*, BUT
//     type of key in splay tree   = cct_addr_t
//

#define l_lt(a, b) cct_addr_lt(a, &(b))
#define l_gt(a, b) cct_addr_gt(a, &(b))

static cct_node_t*
splay(cct_node_t* cct, cct_addr_t* addr)
{
  GENERAL_SPLAY_TREE(cct_node_t, cct, addr, addr, addr, left, right, l_lt, l_gt);
  return cct;
}

#undef l_lt
#undef l_gt

//
// helper for walking functions
//

//
// lrs abbreviation for "left-right-self"
//
static void
walk_child_lrs(cct_node_t* cct,
               cct_op_t op, cct_op_arg_t arg, size_t level,
               void (*wf)(cct_node_t* n, cct_op_t o, cct_op_arg_t a, size_t l))
{
  if (!cct) return;

  walk_child_lrs(cct->left, op, arg, level, wf);
  walk_child_lrs(cct->right, op, arg, level, wf);
  wf(cct, op, arg, level);
}

static void
walkset_l(cct_node_t* cct, cct_op_t fn, cct_op_arg_t arg, size_t level)
{
  if (! cct) return;
  walkset_l(cct->left, fn, arg, level);
  walkset_l(cct->right, fn, arg, level);
  fn(cct, arg, level);
}

//
// walker op used by counting utility
//
typedef struct {
  bool count_dummy;
  size_t n;

  //YUMENG: help count number of non-zero values for each cct
  cct2metrics_t* cct2metrics_map;
  uint64_t num_nzval;
  uint32_t num_nz_cct_nodes;
} count_arg_t;

static void
l_count(cct_node_t* n, cct_op_arg_t arg, size_t level)
{
  count_arg_t *count_arg = (count_arg_t *)arg;
  if (hpcrun_cct_is_dummy(n) && !count_arg->count_dummy) {
    return;
  }

  //YUMENG: count the number of non-zero values
  metric_data_list_t *data_list =
    hpcrun_get_metric_data_list_specific(&(count_arg->cct2metrics_map), n);
  uint64_t num_nzval = hpcrun_metric_sparse_count(data_list);
  (count_arg->num_nzval) += num_nzval;
  if(num_nzval != 0) (count_arg->num_nz_cct_nodes)++; 
  (count_arg->n)++;
}

//
// Special purpose path walking helper
//
static void
walk_path_l(cct_node_t* node, cct_op_t op, cct_op_arg_t arg, size_t level)
{
  if (! node) return;
  walk_path_l(node->parent, op, arg, level+1);
  op(node, arg, level);
}

//
// Writing helpers
//
typedef struct {
  hpcfmt_uint_t num_kind_metrics;
  FILE* fs;
  epoch_flags_t flags;
  hpcrun_fmt_cct_node_t* tmp_node;
  cct2metrics_t* cct2metrics_map;

  //YUMENG: get metric values while walking through cct
  hpcrun_fmt_sparse_metrics_t* sparse_metrics;

} write_arg_t;


//
// Merge the metrics of dummy nodes to their parents
//
static void
collapse_dummy_node(cct_node_t *node, cct_op_arg_t arg, size_t level)
{
  if (!hpcrun_cct_is_dummy(node)) {
    return;
  }

  // get thread specific map
  write_arg_t *write_arg = (write_arg_t *)arg;
  cct2metrics_t **map = &(write_arg->cct2metrics_map);

  // merge dummy child metrics
  cct_node_t* parent = hpcrun_cct_parent(node);
  metric_data_list_t *node_metrics = hpcrun_get_metric_data_list_specific(map, node);
  if (node_metrics != NULL) {
    metric_data_list_t *parent_metrics = hpcrun_get_metric_data_list_specific(map, parent);
    if (parent_metrics != NULL) {
      hpcrun_merge_cct_metrics(parent_metrics, node_metrics);
    } else {
      hpcrun_move_metric_data_list_specific(map, parent, node);
    }
  }
}

static void
l_dummy(cct_node_t* n, cct_op_arg_t arg, size_t level)
{
  bool *dummy = (bool *)arg;
  if (!hpcrun_cct_is_dummy(n)) {
    *dummy = false;
  }
}


#if 0
//YUMENG: count non_zero values for each cct, already merged with l_count
static void
lcount_nzval(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  // avoid writing dummy nodes
  if (!HPCRUN_CCT_KEEP_DUMMY) {
    if (hpcrun_cct_is_dummy(node)) {
      return;
    }
  }

  sizes_arg_t* my_arg = (sizes_arg_t*) arg;
  metric_data_list_t *data_list =
    hpcrun_get_metric_data_list_specific(&(my_arg->cct2metrics_map), node);

  //count the number of non-zero values
   uint64_t num_nzval = hpcrun_metric_sparse_count(data_list);
   (my_arg->num_nzval) += num_nzval;
}
#endif

static void
lwrite(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  // avoid writing dummy nodes
  if (!HPCRUN_CCT_KEEP_DUMMY) {
    if (hpcrun_cct_is_dummy(node)) {
      return;
    }
  }

  // skip all the dummy parents
  cct_node_t* parent = hpcrun_cct_parent(node);

  if (!HPCRUN_CCT_KEEP_DUMMY) {
    while (parent != NULL && hpcrun_cct_is_dummy(parent)) {
      parent = hpcrun_cct_parent(parent);
    }
  }

  bool all_children_dummy;
  if (!HPCRUN_CCT_KEEP_DUMMY) {
    all_children_dummy = true;
    hpcrun_cct_walk_node_1st(node, l_dummy, &all_children_dummy);
  } else {
    all_children_dummy = false;
  }

  write_arg_t* my_arg = (write_arg_t*) arg;
  hpcrun_fmt_sparse_metrics_t* sparse_metrics = my_arg->sparse_metrics;
  hpcrun_fmt_cct_node_t* tmp = my_arg->tmp_node;
  epoch_flags_t flags = my_arg->flags;
  cct_addr_t* addr    = hpcrun_cct_addr(node);

  tmp->id = hpcrun_cct_persistent_id(node);
  tmp->id_parent = parent ? hpcrun_cct_persistent_id(parent) : 0;

  //YUMENG: seems no need to inform new prof about being leaf
  // if no children, chg sign of id when written out
  //if (hpcrun_cct_no_children(node) || all_children_dummy) {
  //  tmp->id = -tmp->id;
  //}

  if (flags.fields.isLogicalUnwind){
    tmp->as_info = addr->as_info;
    lush_lip_init(&tmp->lip);
    if (addr->lip) {
      memcpy(&(tmp->lip), &(addr->lip), sizeof(lush_lip_t));
    }
  }
  tmp->lm_id = (addr->ip_norm).lm_id;

  // double casts to avoid warnings when pointer is < 64 bits
  tmp->lm_ip = (hpcfmt_vma_t) (uintptr_t) (addr->ip_norm).lm_ip;

#if 1
  // YUMENG's code
  metric_data_list_t *data_list =
    hpcrun_get_metric_data_list_specific(&(my_arg->cct2metrics_map), node);

  //set_sparse_copy: copy the values into sparse_metrics
  uint64_t curr_cct_node_idx = sparse_metrics->cur_cct_node_idx; 
  uint64_t num_nzval = hpcrun_metric_set_sparse_copy(sparse_metrics->values, sparse_metrics->mids, data_list, curr_cct_node_idx);
  if(num_nzval != 0){
    (sparse_metrics->cct_node_ids)[sparse_metrics->num_nz_cct_nodes] = tmp->id;
    (sparse_metrics->cct_node_idxs)[sparse_metrics->num_nz_cct_nodes] = curr_cct_node_idx;
    (sparse_metrics->num_nz_cct_nodes)++;
  }
  sparse_metrics->cur_cct_node_idx += num_nzval;

  tmp->num_metrics = 0;
#elif 0
  // keren's code
  tmp->num_metrics = my_arg->num_kind_metrics;
  metric_data_list_t *data_list =
    hpcrun_get_metric_data_list_specific(&(my_arg->cct2metrics_map), node);
  hpcrun_metric_set_dense_copy(tmp->metrics, data_list, my_arg->num_kind_metrics);
#else
  // code from master
  tmp->num_metrics = my_arg->num_metrics;
  metric_set_t* ms = hpcrun_get_metric_set_specific(&(my_arg->cct2metrics_map), node);

  hpcrun_metric_set_dense_copy(tmp->metrics, ms, my_arg->num_metrics);
#endif

  hpcrun_fmt_cct_node_fwrite(tmp, flags, my_arg->fs);
}

//
// ********************* Interface procedures **********************
//

//
// ********** Constructors
//

cct_node_t*
hpcrun_cct_new(void)
{
  return cct_node_create(&(ADDR(CCT_ROOT)), NULL);
}

cct_node_t*
hpcrun_cct_new_partial(void)
{
  return cct_node_create(&(ADDR(PARTIAL_ROOT)), NULL);
}

cct_node_t*
hpcrun_cct_new_special(void* addr)
{
  ip_normalized_t tmp_ip = hpcrun_normalize_ip(addr, NULL);

  cct_addr_t tmp = NON_LUSH_ADDR_INI(tmp_ip.lm_id, tmp_ip.lm_ip);

  return cct_node_create(&tmp, NULL);
}

cct_node_t*
hpcrun_cct_top_new(uint16_t lmid, uintptr_t lmip)
{
  return cct_node_create(&(ADDR2(lmid, lmip)), NULL);
}

//
// ********** Accessor functions
//
cct_node_t*
hpcrun_cct_parent(cct_node_t* x)
{
  return x? x->parent : NULL;
}

cct_node_t*
hpcrun_cct_children(cct_node_t* x)
{
    return x? x->children : NULL;
}

cct_node_t*
hpcrun_leftmost_child(cct_node_t* x)
{
  cct_node_t *leftmost = x->children;
  if (leftmost != NULL) {
    for (;;) {
      cct_node_t *more_left = leftmost->left;
      if (more_left == NULL) break;
      leftmost = more_left;
    }
  }
  return leftmost;
}

int32_t
hpcrun_cct_persistent_id(cct_node_t* x)
{
  return x ? x->persistent_id : -1;
}

cct_addr_t*
hpcrun_cct_addr(cct_node_t* node)
{
  return node ? &(node->addr) : NULL;
}

bool
hpcrun_cct_is_leaf(cct_node_t* node)
{
  return node ? (node->is_leaf) || (!(node->children)) : false;
}

//
// NOTE: having no children is not exactly the same as being a leaf
//       A leaf represents a full path. There might be full paths
//       that are a prefix of other full paths. So, a "leaf" can have children
//
bool
hpcrun_cct_no_children(cct_node_t* node)
{
  return node ? ! node->children : false;
}

bool
hpcrun_cct_is_root(cct_node_t* node)
{
  return ! node->parent;
}

bool
hpcrun_cct_is_dummy(cct_node_t* node)
{
  cct_addr_t* addr = hpcrun_cct_addr(node);
  if ((addr->ip_norm).lm_id == HPCRUN_DUMMY_NODE) {
    return true;
  }
  return false;
}

//
// ********** Mutator functions: modify a given cct
//

cct_node_t*
hpcrun_cct_insert_ip_norm(cct_node_t* node, ip_normalized_t ip_norm)
{
  cct_addr_t frm;

  memset(&frm, 0, sizeof(cct_addr_t));
  frm.ip_norm = ip_norm;

  cct_node_t *child = hpcrun_cct_insert_addr(node, &frm);

  return child;
}


//
// Fundamental mutation operation: insert a given addr into the
// set of children of a given cct node. Return the cct_node corresponding
// to the inserted addr [NB: if the addr is already in the node children, then
// the already-present node is returned. Otherwise, a new node is created, linked in,
// and returned]
//
cct_node_t*
hpcrun_cct_insert_addr(cct_node_t* node, cct_addr_t* frm)
{
  if ( ! node)
    return NULL;

  cct_node_t* found    = splay(node->children, frm);
    //
    // !! SPECIAL CASE for cct splay !!
    // !! The splay tree (represented by the root) is the data structure for the set
    // !! of children of the parent. Consequently, when the splay operation changes the root,
    // !! the parent's children pointer must point to the NEW root node
    // !! NOT the old (pre-splay) root node
    //

  node->children = found;

  if (found && cct_addr_eq(frm, &(found->addr))){
    return found;
  }
  //  cct_node_t* new = cct_node_create(frm->as_info, frm->ip_norm, frm->lip, node);
  cct_node_t* new = cct_node_create(frm, node);

  node->children = new;
  if (! found){
    return new;
  }
  if (cct_addr_lt(frm, &(found->addr))){
    new->left = found->left;
    new->right = found;
    found->left = NULL;
  }
  else { // addr > addr of found
    new->left = found;
    new->right = found->right;
    found->right = NULL;
  }
  return new;
}

cct_node_t*
hpcrun_cct_insert_dummy(cct_node_t* node, uint16_t lm_ip)
{
  ip_normalized_t ip = { .lm_id = HPCRUN_DUMMY_NODE, .lm_ip = lm_ip };
  cct_addr_t frm = { .ip_norm = ip };
  cct_node_t *dummy = hpcrun_cct_insert_addr(node, &frm);
  return dummy;
}

cct_node_t*
hpcrun_cct_delete_addr(cct_node_t* node, cct_addr_t* frm)
{
  if(!node) return NULL;

  cct_node_t* found = splay(node->children, frm);

  node->children = found;

  if(!found || !cct_addr_eq(frm, &(found->addr)))
    return NULL;

  if(node->children->left == NULL) {
    node->children = node->children->right;
    return found;
  }
  node->children->left = splay(node->children->left, frm);
  node->children->left->right = node->children->right;
  node->children = node->children->left;
  return found;
}

// insert a path to the root and return the path in the root
cct_node_t*
hpcrun_cct_insert_path_return_leaf(cct_node_t *root, cct_node_t *path)
{
  if(!path || ! path->parent) return root;
  root = hpcrun_cct_insert_path_return_leaf(root, path->parent);
  return hpcrun_cct_insert_addr(root, &(path->addr));
}

// remove the sub-tree rooted at cct from it's parent
//
// TODO: actual freelist manipulation required
//       for now, do nothing
//
void
hpcrun_cct_delete_self(cct_node_t *cct)
{
  hpcrun_cct_delete_addr(cct->parent, &cct->addr);
  // FIXME vi3: I think below should be added, because of freelist
  // cause previous function remove node from parent tree,
  // but do not remove parent, left and right
  cct->left = NULL;
  cct->right = NULL;
  cct->parent = NULL;
  hpcrun_cct_node_free(cct);
}

//
// 2nd fundamental mutator: mark a node as "terminal". That is,
//   it is the last node of a path
//
void
hpcrun_cct_terminate_path(cct_node_t* node)
{
  node->is_leaf = true;
}

//
// Special purpose mutator:
// This operation is somewhat akin to concatenation.
// An already constructed cct ('src') is inserted as a
// child of the 'target' cct. The addr field of the src
// cct is ASSUMED TO BE DIFFERENT FROM ANY ADDR IN target's
// child set. [Otherwise something recursive has to happen]
//
//
cct_node_t*
hpcrun_cct_insert_node(cct_node_t* target, cct_node_t* src)
{
  src->parent = target;

  cct_node_t* found = splay(target->children, &(src->addr));
  target->children = src;
  if (! found) {
    return src;
  }

  // NOTE: Assume equality cannot happen

  if (cct_addr_lt(&(src->addr), &(found->addr))){
    src->left = found->left;
    src->right = found;
    found->left = NULL;
  }
  else { // addr > addr of found
    src->left = found;
    src->right = found->right;
    found->right = NULL;
  }
  return src;
}

// mark a node for retention as the leaf of a traced call path.
// for marked nodes, hpcprof must preserve the association between
// the node number recorded in the trace and its call path so that
// hpctraceviewer can recover the call path for a trace record.
void
hpcrun_cct_retain(cct_node_t* x)
{
  x->persistent_id |= HPCRUN_FMT_RetainIdFlag;
}


// check if a node was marked for retention as the leaf of a traced
// call path.
int
hpcrun_cct_retained(cct_node_t* x)
{
  return (x->persistent_id & HPCRUN_FMT_RetainIdFlag);
}

//
// Walking functions section:
//

//
//     general walking functions: (client may select starting level)
//       visits every node in the cct, calling op(node, arg, level)
//       level has property that node at level n ==> children at level n+1
//     there are 2 different walking strategies:
//       1) walk the children, then walk the node
//       2) walk the node, then walk the children
//     there is no implied children ordering
//

//
// visting order: children first, then node
//
void
hpcrun_cct_walk_child_1st_w_level(cct_node_t* cct, cct_op_t op, cct_op_arg_t arg, size_t level)
{
  if (!cct) return;
  walk_child_lrs(cct->children, op, arg, level+1,
		 hpcrun_cct_walk_child_1st_w_level);
  op(cct, arg, level);
}

//
// visting order: node first, then children
//
void
hpcrun_cct_walk_node_1st_w_level(cct_node_t* cct, cct_op_t op, cct_op_arg_t arg, size_t level)
{
  if (!cct) return;
  op(cct, arg, level);
  walk_child_lrs(cct->children, op, arg, level+1,
		 hpcrun_cct_walk_node_1st_w_level);
}

//
// utility walker for cct sets (part of the substructure of a cct)
//
void
hpcrun_cct_walkset(cct_node_t* cct, cct_op_t fn, cct_op_arg_t arg)
{
  if(!cct->children) return;
  walkset_l(cct->children, fn, arg, 0);
}

//
// Special routine to walk a path represented by a cct node.
// The actual path represented by a node is list reversal of the nodes
//  linked by the parent link. So walking a path means visiting the
// path nodes in list reverse order
//

void
hpcrun_walk_path(cct_node_t* node, cct_op_t op, cct_op_arg_t arg)
{
  walk_path_l(node, op, arg, 0);
}


//
// helper for inserting creation contexts
//
static void
l_insert_path(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  // convenient constant cct_addr_t's
  static cct_addr_t root = ADDR_I(CCT_ROOT);

  cct_addr_t* addr = hpcrun_cct_addr(node);
  if (cct_addr_eq(addr, &root)) return;

  cct_node_t** tree = (cct_node_t**) arg;
  *tree = hpcrun_cct_insert_addr(*tree, addr);
}


// Inserts cct path pointed by 'path' into a cct rooted at 'root'

void
hpcrun_cct_insert_path(cct_node_t ** root, cct_node_t* path)
{
  hpcrun_walk_path(path, l_insert_path, (cct_op_arg_t) root);
}

#if 0
int
hpcrun_cct_fwrite(cct2metrics_t* cct2metrics_map, cct_node_t* cct, FILE* fs, epoch_flags_t flags)
#else
//YUMENG: add sparse_metrics to collect metric values
int
hpcrun_cct_fwrite(cct2metrics_t* cct2metrics_map, cct_node_t* cct, FILE* fs, epoch_flags_t flags, hpcrun_fmt_sparse_metrics_t* sparse_metrics)
#endif
{
  if (!fs) return HPCRUN_ERR;

  //YUMENG: count number of nodes & number of non-zero values for all nodes
  size_t nodes = 0;
  uint64_t num_nzval = 0;
  uint32_t num_nz_cct_nodes = 0;
  if (HPCRUN_CCT_KEEP_DUMMY) {
    nodes = hpcrun_cct_num_nodes(cct, true, &cct2metrics_map, &num_nzval, &num_nz_cct_nodes);
  } else {
    nodes = hpcrun_cct_num_nodes(cct, false, &cct2metrics_map, &num_nzval, &num_nz_cct_nodes);
  }
  sparse_metrics->num_cct_nodes = nodes;

  //YUMENG: record cct_node_ids:cct_node_idxs pair
  sparse_metrics->cur_cct_node_idx = 0;
  sparse_metrics->cct_node_idxs = (uint64_t *) hpcrun_malloc((num_nz_cct_nodes+1)*sizeof(uint64_t));
  sparse_metrics->cct_node_ids = (uint32_t *) hpcrun_malloc((num_nz_cct_nodes+1)*sizeof(uint32_t));
  sparse_metrics->num_nz_cct_nodes = 0;

  hpcfmt_int8_fwrite((uint64_t) nodes, fs);
  TMSG(DATA_WRITE, "num cct nodes = %d", nodes);

  hpcfmt_uint_t num_kind_metrics = hpcrun_get_num_kind_metrics();
  TMSG(DATA_WRITE, "num metrics in a cct node = %d", num_kind_metrics);

  hpcrun_fmt_cct_node_t tmp_node;

  sparse_metrics->num_vals = num_nzval;
  sparse_metrics->values = (cct_metric_data_t *) hpcrun_malloc(num_nzval * sizeof(cct_metric_data_t));
  sparse_metrics->mids = (uint16_t *) hpcrun_malloc(num_nzval * sizeof(uint16_t));

  write_arg_t write_arg = {
    .num_kind_metrics = num_kind_metrics,
    .fs          = fs,
    .flags       = flags,
    .tmp_node    = &tmp_node,

    // multithreaded code: add personalized cct2metrics_map for multithreading programs
    // this is to allow a thread to write the profile data of another thread.
    .cct2metrics_map = cct2metrics_map,

    //YUMENG: collect metric values and info while walking through the cct
    .sparse_metrics = sparse_metrics
  };


//YUMENG: no metricTbl info needed to write cct
#if 0
  hpcrun_metricVal_t metrics[num_kind_metrics];
  tmp_node.metrics = &(metrics[0]);
#endif

  if (!HPCRUN_CCT_KEEP_DUMMY) {
    hpcrun_cct_walk_child_1st(cct, collapse_dummy_node, &write_arg);
  }
  hpcrun_cct_walk_node_1st(cct, lwrite, &write_arg);

  //one extra entry in cct_node_id&idx pairs to mark the end index of the last cct node
  sparse_metrics->cct_node_ids[num_nz_cct_nodes] = LastNodeEnd;
  sparse_metrics->cct_node_idxs[num_nz_cct_nodes] = sparse_metrics->cur_cct_node_idx;


  //YUMENG: try to make sure the recorded info are correct
  //sparse_metrics->id_tuple.length should be changed to something else that represents a file
  if(sparse_metrics->num_nz_cct_nodes != num_nz_cct_nodes) {
    hpcrun_cct_fwrite_errmsg_w_fn(fs, sparse_metrics->id_tuple.length, "recorded number of non-zero cct nodes after walking through the cct don't match");
    return HPCRUN_ERR;
  }
  if(sparse_metrics->cur_cct_node_idx != sparse_metrics->num_vals){
    hpcrun_cct_fwrite_errmsg_w_fn(fs, sparse_metrics->id_tuple.length, "number of nzvals and cur_cct_node_idx are not equal after walking through the cct");
    return HPCRUN_ERR;
  } 
    
  return HPCRUN_OK;
}

//YUMENG: help write error message with profile name
void hpcrun_cct_fwrite_errmsg_w_fn(FILE* fs, uint32_t tid, char* msg)
{
  int MAXSIZE = 128;
  char proclink[MAXSIZE];
  char filename[MAXSIZE];
  sprintf(proclink, "/proc/self/fd/%d", fileno(fs));
  ssize_t r = readlink(proclink, filename, MAXSIZE);
  if(r < 0) {
    EEMSG("ERROR: %s for profile with thread %d", msg, tid);
  }else{
    filename[r] = '\0';
    EEMSG("ERROR: %s for '%s'", msg, filename);
  }
}

//
// Utilities
//
size_t
hpcrun_cct_num_nodes(cct_node_t* cct, bool count_dummy, cct2metrics_t **cct2metrics_map,uint64_t* num_nzval, uint32_t* num_nz_cct_nodes)
{
  count_arg_t count_arg = {
    .count_dummy = count_dummy,
    .n = 0,

    //YUMENG: count number of non-zero values
    .cct2metrics_map = *cct2metrics_map,
    .num_nzval = *num_nzval,
    .num_nz_cct_nodes = *num_nz_cct_nodes
  };
  hpcrun_cct_walk_node_1st(cct, l_count, &count_arg);
  *cct2metrics_map = count_arg.cct2metrics_map;
  *num_nzval = count_arg.num_nzval;
  *num_nz_cct_nodes = count_arg.num_nz_cct_nodes;
  return count_arg.n;
}

//
// look up addr in the set of cct's children
// return the found node or NULL
//
cct_node_t*
hpcrun_cct_find_addr(cct_node_t* cct, cct_addr_t* addr)
{
  if ( ! cct)
    return NULL;

  cct_node_t* found    = splay(cct->children, addr);
    //
    // !! SPECIAL CASE for cct splay !!
    // !! The splay tree (represented by the root) is the data structure for the set
    // !! of children of the parent. Consequently, when the splay operation changes the root,
    // !! the parent's children pointer must point to the NEW root node
    // !! NOT the old (pre-splay) root node
    //

  cct->children = found;

  if (found && cct_addr_eq(addr, &(found->addr))){
    return found;
  }
  return NULL;
}

//
// Merging operation: Given 2 ccts : CCT_A, CCT_B,
//    merge means add all paths in CCT_B that are NOT in CCT_A
//    to CCT_A. For paths that are common, perform the merge operation on
//    each common node, using auxiliary arg merge_arg
//
//    NOTE: this merge operation presumes
//       cct_addr_data(CCT_A) == cct_addr_data(CCT_B)
//

// vi3: Added by vi3
static cct_node_t*
walkset_l_merge(cct_node_t* cct, cct_op_merge_t fn, cct_op_arg_t arg, size_t level)
{
  // if node is NULL, the return NULL
  if (! cct) return NULL;
  // if left should be disconnected
  if (! walkset_l_merge(cct->left, fn, arg, level))
    cct->left = NULL;
  // if right should be disconnected
  if(! walkset_l_merge(cct->right, fn, arg, level))
    cct->right = NULL;
  // fn is going to decide if cct should be disconnected from parent or not
  return fn(cct, arg, level);
}

void
hpcrun_cct_walkset_merge(cct_node_t* cct, cct_op_merge_t fn, cct_op_arg_t arg)
{
  if(! cct->children) return;
  // should children be disconnected
  if(! walkset_l_merge(cct->children, fn, arg, 0))
    cct->children = NULL;
}




//
// Helpers & datatypes for cct_merge operation
//
//static void merge_or_join(cct_node_t* n, cct_op_arg_t a, size_t l);
static cct_node_t* merge_or_join(cct_node_t* n, cct_op_arg_t a, size_t l);

static cct_node_t* cct_child_find_cache(cct_node_t* cct, cct_addr_t* addr);
static void cct_disjoint_union_cached(cct_node_t* target, cct_node_t* src);

typedef struct {
  cct_node_t* targ;
  merge_op_t fn;
  merge_op_arg_t arg;
} mjarg_t;

// always returns NULL which indicated that node should be disconnected from old tree
static void
attach_to_a(cct_node_t* node, cct_op_arg_t arg, size_t l)
{
  cct_node_t* targ = (cct_node_t*) arg;
  node->parent = targ;
}

//
// The merging operation main code
//

#include "../utilities/ip-normalized.h"

void
hpcrun_cct_merge(cct_node_t* cct_a, cct_node_t* cct_b,
		 merge_op_t merge, merge_op_arg_t arg)
{
  if (hpcrun_cct_is_leaf (cct_a) && hpcrun_cct_is_leaf(cct_b)) {
    // nothing to clean, because cct_b is leaf
    merge(cct_a, cct_b, arg);
  }
  if (! cct_a->children){
      // FIXME: vi3 bug because cct_b->children has the same addr as cct_a
    cct_a->children = cct_b->children;
    // whole cct->children splay tree is used as kids of cct_a,
    // enough to disconnect children from cct_b (that's why hpcrun_cct_walkset is called)
    hpcrun_cct_walkset(cct_b, attach_to_a, (cct_op_arg_t) cct_a);
    cct_b->children = NULL;
  }
  else {
    mjarg_t local = (mjarg_t) {.targ = cct_a, .fn = merge, .arg = arg};
    hpcrun_cct_walkset_merge(cct_b, merge_or_join, (cct_op_arg_t) &local);
  }
}

//
// merge helper functions (forward declared above)
//
// if function cct_disjoint_union_cached is called, that means n should be removed from cct_b tree,
// which is indicated by NULL as a return value
// if hpcrun_cct_merge is called, that means that node n should stay in the cct_b tree
// which is indicated by n as a return value (not NULL value)
static cct_node_t*
merge_or_join(cct_node_t* n, cct_op_arg_t a, size_t l)
{
  mjarg_t* the_arg = (mjarg_t*) a;
  cct_node_t* targ = the_arg->targ;
  cct_node_t* tmp = NULL;
  if ((tmp = cct_child_find_cache(targ, hpcrun_cct_addr(n)))){
    // when merge, n should stay in the same tree, because the whole tree is going to to freelist
    // that is the reason why return value is not NULL
    hpcrun_cct_merge(tmp, n, the_arg->fn, the_arg->arg);
    return n;
  }
  else{
    // disjoint has to happen, which means that node n is going to change tree
    // if it has left and right siblings, they are going to bee added to freelist
    // and the return value is NULL (indicates that n is goint to be disconnected from previous cct_b tree)

    // add left to freelist, if needed
    hpcrun_cct_node_free(n->left);
    // add right to freelist, if needed
    hpcrun_cct_node_free(n->right);

    cct_disjoint_union_cached(targ, n);
    return NULL;
  }

}

//
// Differs from the main accessor by setting the splay cache as a side
// effect
//
static cct_node_t*
cct_child_find_cache(cct_node_t* cct, cct_addr_t* addr)
{
  return hpcrun_cct_find_addr(cct, addr);
}

//
// This procedure assumes that cct_child_find_cache has been
// called, and that no other intervening splay operations have been called
//
static void
cct_disjoint_union_cached(cct_node_t* target, cct_node_t* src)
{

  if ( ! target) {
    if ( src) EMSG("WARNING: cct disjoin union called w null target!!");
    return;
  }

  cct_addr_t* addr = hpcrun_cct_addr(src);
  cct_node_t* found    = splay(target->children, addr);  // FIXME: vi3: is it possible that splay returns something which address is not equal to addre
    //
    // !! SPECIAL CASE for cct splay !!
    // !! The splay tree (represented by the root) is the data structure for the set
    // !! of children of the parent. Consequently, when the splay operation changes the root,
    // !! the parent's children pointer must point to the NEW root node
    // !! NOT the old (pre-splay) root node
    //
  if (!found) {
    target->children = src;
    src->parent = target;
    return;
  }

  if (cct_addr_lt(addr, &(found->addr))){
    src->left = found->left;
    src->right = found;
    found->left = NULL;
  }
  else { // addr > addr of found
    src->left = found;
    src->right = found->right;
    found->right = NULL;
  }
  target->children = src;
  src->parent = target;
}


// FIXME: only temporary function, until hpcrun_merge is repaired
void
cct_remove_my_subtree(cct_node_t* cct){
  cct->children = NULL;
//  printf("CHILDREN: %p\tLEFT: %p\tRIGHT: %p\n", cct->children, cct->left, cct->right);
}


// FIXME: is this proper place for handling memory leaks caused by cct_node_t
// frelist manipulation

__thread cct_node_t* cct_node_freelist_head = NULL;

// vi3: functions used for manipulation of freelist of trees
void
add_node_to_freelist(cct_node_t* cct){
  // parent is used as a next pointer
  if(cct){
    cct->parent = cct_node_freelist_head;
    cct_node_freelist_head = cct;
  }
}

// vi3: remove root of first tree in the freelist
cct_node_t*
remove_node_from_freelist(){
  cct_node_t* first_root = cct_node_freelist_head;
  if(!first_root){
    return NULL;
  }
  // new head is free_root's next (parent pointer is used for now)
  cct_node_freelist_head = first_root->parent;

  cct_node_t* children = first_root->children;
  cct_node_t* left = first_root->left;
  cct_node_t* right = first_root->right;

  add_node_to_freelist(children);
  add_node_to_freelist(left);
  add_node_to_freelist(right);

  return first_root;

  // FIXME: seg fault happened once and i cannot reproduce it anymore
}


// allocating and free cct_node_t
cct_node_t*
hpcrun_cct_node_alloc(){
  cct_node_t* cct_new = remove_node_from_freelist();
  return cct_new ? cct_new : (cct_node_t*)hpcrun_malloc(sizeof(cct_node_t));
}


void
hpcrun_cct_node_free(cct_node_t *cct){
  add_node_to_freelist(cct);
}


// FIXME vi3: disccuss about hpcrun_merge



cct_node_t*
hpcrun_cct_copy_just_addr(cct_node_t *cct)
{
  return cct ? cct_node_create(&cct->addr, NULL): NULL;
}

void
hpcrun_cct_set_children(cct_node_t* cct, cct_node_t* children)
{
  if(!cct)
    return;
  cct->children = children;
}

void
hpcrun_cct_set_parent(cct_node_t* cct, cct_node_t* parent)
{
  if(!cct)
    return;
  cct->parent = parent;
}
