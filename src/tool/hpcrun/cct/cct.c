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
// Copyright ((c)) 2002-2016, Rice University
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include <memory/hpcrun-malloc.h>
#include <hpcrun/metrics.h>
#include <messages/messages.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <hpcrun/hpcrun_return_codes.h>

#include "cct.h"
#include "cct_addr.h"
#include "cct2metrics.h"

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
  cct_node_t* node;
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
  static long global_persistent_id = 12;
  uint32_t myid = (int) fetch_and_add(&global_persistent_id, 2); 
  return myid;
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
    node = hpcrun_malloc(sz);
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
static void
l_count(cct_node_t* n, cct_op_arg_t arg, size_t level)
{
  size_t* cnt = (size_t*) arg;
  (*cnt)++;
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
  hpcfmt_uint_t num_metrics;
  FILE* fs;
  epoch_flags_t flags;
  hpcrun_fmt_cct_node_t* tmp_node;
} write_arg_t;


static void
lwrite(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  write_arg_t* my_arg = (write_arg_t*) arg;
  hpcrun_fmt_cct_node_t* tmp = my_arg->tmp_node;
  cct_node_t* parent = hpcrun_cct_parent(node);
  epoch_flags_t flags = my_arg->flags;
  cct_addr_t* addr    = hpcrun_cct_addr(node);

  tmp->id = hpcrun_cct_persistent_id(node);
  tmp->id_parent = parent ? hpcrun_cct_persistent_id(parent) : 0;

  // if no children, chg sign of id when written out
  if (hpcrun_cct_no_children(node)) {
    tmp->id = - tmp->id;
  }
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

  tmp->num_metrics = my_arg->num_metrics;
  hpcrun_metric_set_dense_copy(tmp->metrics, hpcrun_get_metric_set(node),
			       my_arg->num_metrics);
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


//
// ********** Mutator functions: modify a given cct
//

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

// special mutator to support tracing

void
hpcrun_cct_persistent_id_trace_mutate(cct_node_t* x)
{
  x->persistent_id |= HPCRUN_FMT_RetainIdFlag;
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


//
// Writing operation
//
int
hpcrun_cct_fwrite(cct_node_t* cct, FILE* fs, epoch_flags_t flags)
{
  if (!fs) return HPCRUN_ERR;

  hpcfmt_int8_fwrite((uint64_t) hpcrun_cct_num_nodes(cct), fs);
  TMSG(DATA_WRITE, "num cct nodes = %d", hpcrun_cct_num_nodes(cct));

  hpcfmt_uint_t num_metrics = hpcrun_get_num_metrics();
  TMSG(DATA_WRITE, "num metrics in a cct node = %d", num_metrics);
  
  hpcrun_fmt_cct_node_t tmp_node;

  write_arg_t write_arg = {
    .num_metrics = num_metrics,
    .fs          = fs,
    .flags       = flags,
    .tmp_node    = &tmp_node,
  };
  
  hpcrun_metricVal_t metrics[num_metrics];
  tmp_node.metrics = &(metrics[0]);

  hpcrun_cct_walk_node_1st(cct, lwrite, &write_arg);

  return HPCRUN_OK;
}

//
// Utilities
//
size_t
hpcrun_cct_num_nodes(cct_node_t* cct)
{
  size_t n = 0;
  hpcrun_cct_walk_node_1st(cct, l_count, &n);
  return n;
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

//
// Helpers & datatypes for cct_merge operation
//
static void merge_or_join(cct_node_t* n, cct_op_arg_t a, size_t l);
static cct_node_t* cct_child_find_cache(cct_node_t* cct, cct_addr_t* addr);
static void cct_disjoint_union_cached(cct_node_t* target, cct_node_t* src);

typedef struct {
  cct_node_t* targ;
  merge_op_t fn;
  merge_op_arg_t arg;
} mjarg_t;

static void
attach_to_a(cct_node_t* node, cct_op_arg_t arg, size_t l)
{
  cct_node_t* targ = (cct_node_t*) arg;
  node->parent = targ;
}

//
// The merging operation main code
//

void
hpcrun_cct_merge(cct_node_t* cct_a, cct_node_t* cct_b,
		 merge_op_t merge, merge_op_arg_t arg)
{
  if (hpcrun_cct_is_leaf (cct_a) && hpcrun_cct_is_leaf(cct_b)) {
    merge(cct_a, cct_b, arg);
  }
  if (! cct_a->children){
    cct_a->children = cct_b->children;
    hpcrun_cct_walkset(cct_b, attach_to_a, (cct_op_arg_t) cct_a);
  }
  else {
    mjarg_t local = (mjarg_t) {.targ = cct_a, .fn = merge, .arg = arg};
    hpcrun_cct_walkset(cct_b, merge_or_join, (cct_op_arg_t) &local);
  }
}

//
// merge helper functions (forward declared above)
//
static void
merge_or_join(cct_node_t* n, cct_op_arg_t a, size_t l)
{
  mjarg_t* the_arg = (mjarg_t*) a;
  cct_node_t* targ = the_arg->targ;
  cct_node_t* tmp = NULL;
  if ((tmp = cct_child_find_cache(targ, hpcrun_cct_addr(n))))
    hpcrun_cct_merge(tmp, n, the_arg->fn, the_arg->arg);
  else
    cct_disjoint_union_cached(targ, n);
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
  cct_node_t* found    = splay(target->children, addr);
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
