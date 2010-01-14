// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include <alloca.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <alloca.h>
#include <ucontext.h>

//*************************** User Include Files ****************************

#include "cct.h"
#include <memory/hpcrun-malloc.h>
#include <hpcrun/metrics.h>

#include <hpcrun/hpcrun_return_codes.h>

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <unwind/common/backtrace.h>
#include <trampoline/common/trampoline.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/hpcrun_stats.h>
#include <lush/lush-backtrace.h>

//***************************** Local Macros ********************************

//*************************** Forward Declarations **************************

/* cstree callbacks (CB) */
static cct_node_t*
cct_node_find_child(cct_node_t* x,
		    lush_assoc_info_t as_info, void* ip,
		    lush_lip_t* lip);

static int
cct_node_link(cct_node_t *, cct_node_t *);

//*************************** Forward Declarations **************************

// FIXME: tallent: when code is merged into hpctoolkit tree, this
// should come from src/lib/support/Logic.hpp (where a C version can
// easily be created).
static inline bool
implies(bool p, bool q) { return (!p || q); }

static cct_node_t*
_hpcrun_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context,
		      int metricId, uint64_t metricIncr,
		      int skipInner);

static cct_node_t*
_hpcrun_bt2cct(hpcrun_cct_t *cct, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       bt_mut_fn bt_fn, bt_fn_arg bt_arg);

//***************************************************************************
//
//***************************************************************************

// FIXME:tallent: The linear lookup of node children should be
// improved.  A full on red-black tree data structure for the children
// is probably overkill.  Just putting the most recently used child at
// the front of the list would be sufficient

static uint32_t 
new_persistent_id()
{
  // by default, all persistent ids are even; odd ids signify that we need 
  // to retain them as call path ids associated with a trace.
  static long global_persistent_id = 2;
  uint32_t myid = (int) fetch_and_add(&global_persistent_id, 2); 
  return myid;
}


static cct_node_t*
cct_node_create(lush_assoc_info_t as_info, 
		void* ip,
		lush_lip_t* lip,
		hpcrun_cct_t *x)
{
  size_t sz = (sizeof(cct_node_t)
	       + sizeof(cct_metric_data_t)*hpcrun_get_num_metrics());
  cct_node_t *node;

  // FIXME: when multiple epochs really work, this will always be freeable.
  // WARN ME (krentel) if/when we really use freeable memory.
  if (ENABLED(FREEABLE)) {
    node = hpcrun_malloc_freeable(sz);
  } else {
    node = hpcrun_malloc(sz);
  }

  memset(node, 0, sz);

  node->as_info = as_info; // LUSH
  node->ip = ip;
  node->lip = lip;         // LUSH

  node->persistent_id = new_persistent_id();

  node->next_sibling = NULL;

  return node;
}


cct_node_t*
hpcrun_copy_btrace(cct_node_t* n)
{
  return n;

  // FIXME: this routine is broken. it doesn't consider metric data for nodes

  if (! n) {
    TMSG(CCT_CTXT, "incoming cct path = %p", n);
    return NULL;
  }

  //
  // NOTE: cct nodes here are in NON-freeable memory (to be passed to other threads)
  //
  cct_node_t* rv = (cct_node_t*) hpcrun_malloc(sizeof(cct_node_t));

  memcpy(rv, n, sizeof(cct_node_t));
  rv->parent   = NULL;
  rv->children = NULL;

  cct_node_t* prev = rv;
  for(n = n->parent; n; n = n->parent){
    TMSG(CCT_CTXT, "ctxt node(%p) ==> parent(%p) :: child(%p)", n, n->parent, n->children);
    cct_node_t* cpy = (cct_node_t*) hpcrun_malloc(sizeof(cct_node_t));
    memcpy(cpy, n, sizeof(cct_node_t));
    cpy->parent   = NULL;
    cpy->children = NULL;

    prev->parent  = cpy;
    prev          = cpy;
  }
  return rv;
}


static bool
all_metrics_0(cct_node_t* node)
{
  cct_metric_data_t* metrics = &(node->metrics[0]);
  int num_metrics            = hpcrun_get_num_metrics();
  bool rv = true;
  for (int i=0; i < num_metrics; i++){
    if (metrics[i].bits) {
      rv = false;
      break;
    }
  }
  return rv;
}


bool
hpcrun_empty_cct(hpcrun_cct_t* cct)
{
  bool rv = (cct->num_nodes == 1);
  if (rv) {
    TMSG(CCT, "cct %p is empty", cct);
  }
  return rv;
}


bool
no_metric_samples(hpcrun_cct_t* cct)
{
  return (cct->num_nodes == 1) &&
    ((cct->tree_root->persistent_id == 0) || (all_metrics_0(cct->tree_root)));
}


cct_ctxt_t* 
copy_thr_ctxt(cct_ctxt_t* thr_ctxt)
{
  // MEMORY PROBLEM: if thr_ctxt is reclaimed 
  // FIXME: if reclamation of a thread context is possible, we would need
  //        a deep copy here.
  return thr_ctxt;
}


static void
cct_node_parent_insert(cct_node_t *x, cct_node_t *parent)
{
  cct_node_link(x, parent);
}


// cct_node_link: links a node to a parent and at
// the end of the circular doubly-linked list of its siblings (if any)
static int
cct_node_link(cct_node_t* x, cct_node_t* parent)
{
  /* Sanity check */
  if (x->parent != NULL) { return HPCRUN_ERR; } /* can only have one parent */
  
  if (parent != NULL) {
    /* Children are maintained as a doubly linked ring.  A new node
       is linked at the end of the ring (as a predecessor of
       "parent->children") which points to first child in the ring */
    cct_node_t *first_sibling = parent->children;
    if (first_sibling) {
      x->next_sibling = parent->children;
      parent->children = x;
    } else {
      /* create a single element ring. */
      parent->children = x;
    }

    x->parent = parent;
  }
  return HPCRUN_OK;
}


// cct_node_find_child: finds the child of 'x' with
// instruction pointer equal to 'ip'.
//
// tallent: I slightly sanitized the different versions, but FIXME
//
static cct_node_t*
cct_node_find_child(cct_node_t* x,
			    lush_assoc_info_t as_info, void* ip,
			    lush_lip_t* lip)
{
  cct_node_t* c, *first;
  lush_assoc_t as = lush_assoc_info__get_assoc(as_info);
      
  first = c = cct_node_firstChild(x);
  if (c) {
    do {
      // LUSH
      // FIXME: abstract this and the test in CSProfNode::findDynChild
      lush_assoc_t c_as = lush_assoc_info__get_assoc(c->as_info);
      if (c->ip == ip 
	  && lush_lip_eq(c->lip, lip)
	  && lush_assoc_class_eq(c_as, as) 
	  && lush_assoc_info__path_len_eq(c->as_info, as_info)) {
	return c;
      }

      c = cct_node_nextSibling(c);
    } while (c != NULL);
  }

  return NULL;
}

//***************************************************************************
// Metric support functions
//***************************************************************************

int
hpcrun_cct_node_length(cct_node_t* node)
{
  int len = 0;
  for (cct_node_t* p = node; p; p = cct_node_parent(p)) {
    len++;
  }
  return len;
}

void
hpcrun_cct_node_update_metric(cct_node_t* node, int metric_id, metric_bin_fn fn, cct_metric_data_t datum)
{
  node->metrics[metric_id] = fn(node->metrics[metric_id], datum);
}

cct_metric_data_t
hpcrun_cct_node_get_metric(cct_node_t* node, int metric_id)
{
  return node->metrics[metric_id];
}

//***************************************************************************
//  Interface functions
//***************************************************************************

/* building and deleting trees */

void
hpcrun_cct_make_root(hpcrun_cct_t* x, cct_ctxt_t* ctxt)
{
  // introduce bogus root to handle possible forests
  x->tree_root = cct_node_create(lush_assoc_info_NULL, 0, NULL, x);

  x->tree_root->parent = (ctxt)? ctxt->context : NULL;
  x->num_nodes = 1;
}


int
hpcrun_cct_init(hpcrun_cct_t* x, cct_ctxt_t* ctxt)
{
  TMSG(CCT_TYPE,"--Init");
  memset(x, 0, sizeof(*x));

  hpcrun_cct_make_root(x, ctxt);

  return HPCRUN_OK;
}


int
hpcrun_cct_fini(hpcrun_cct_t *x)
{
  TMSG(CCT_TYPE,"--Fini");
  return HPCRUN_OK;
}


// find a child with the specified pc. if none exists, create one.
cct_node_t*
hpcrun_cct_get_child(hpcrun_cct_t *cct, cct_node_t* parent, frame_t *frm)
{
  cct_node_t *c = cct_node_find_child(parent, frm->as_info, frm->ip, frm->lip);

  if (!c) {
    c = cct_node_create(frm->as_info, frm->ip, frm->lip, cct);
    cct_node_parent_insert(c, parent);
    cct->num_nodes++;
  }


  return c;
} 


// See usage in header.
cct_node_t*
hpcrun_cct_insert_backtrace(hpcrun_cct_t* cct, cct_node_t* treenode,
			    int metric_id,
			    frame_t* path_beg, frame_t* path_end,
			    cct_metric_data_t datum)
{
#define MY_advancePathFrame(x) (x)--
#define MY_isPathFrameAtEnd(x) ((x) < path_end)

  TMSG(CCT,"Insert backtrace w x=%p, tn=%p, strt=%p, end=%p", cct, treenode,
      path_beg, path_end);

  frame_t* frm   = path_beg; // current frame 
  cct_node_t* tn = treenode;

  if ( !(path_beg >= path_end) ) {
    EMSG("Attempted backtrace insertion where path_beg >= path_end!!!");
    return NULL;
  }

  if (tn == NULL) {
    tn = cct->tree_root;

    TMSG(CCT, "Starting tree node = (NULL), so begin search @ cct root");
    if (frm->ip == tn->ip) {
      TMSG(CCT,"beg ip == tn ip = %p", tn->ip);
      MY_advancePathFrame(frm);
    }

    TMSG(CCT, "beg ip %p", frm->ip);
  }

  while (true) {
    if (MY_isPathFrameAtEnd(frm)) {
      break;
    }

    // Attempt to find a child 'c' corresponding to 'frm'
    TMSG(CCT,"looking for child in tree w ip = %p", frm->ip);

    cct_node_t* c = cct_node_find_child(tn, frm->as_info, frm->ip, frm->lip);
    if (c) {
      // child exists; recur
      TMSG(CCT,"found child @ node %p", c);
      tn = c;

      // If as_frm is 1-to-1 and as_c is not, update the latter
      lush_assoc_t as_frm = lush_assoc_info__get_assoc(frm->as_info);
      lush_assoc_t as_c = lush_assoc_info__get_assoc(c->as_info);
      if (as_frm == LUSH_ASSOC_1_to_1 && as_c != LUSH_ASSOC_1_to_1) {
	// INVARIANT: c->as_info must be either M-to-1 or 1-to-M
	lush_assoc_info__set_assoc(c->as_info, LUSH_ASSOC_1_to_1);
      }
      MY_advancePathFrame(frm);
    }
    else {
      // no such child; insert new tail
      TMSG(CCT,"No child found, inserting new tail");
      
      while (!MY_isPathFrameAtEnd(frm)) {
	c = cct_node_create(frm->as_info, frm->ip, frm->lip, cct);
	TMSG(CCT, "create node %p w ip = %p", c, frm->ip);
	cct_node_parent_insert(c, tn);
	cct->num_nodes++;
	
	tn = c;
	MY_advancePathFrame(frm);
      }
    }
  }

  TMSG(CCT, "Total nodes after backtrace insertion = %d", cct->num_nodes);

  hpcrun_get_metric_proc(metric_id)(metric_id, &tn->metrics[metric_id], datum);
  return tn;

#undef MY_advancePathFrame
#undef MY_isPathFrameAtEnd
}

//
// Insert new backtrace in cct
//

cct_node_t*
hpcrun_cct_insert_bt(hpcrun_cct_t* cct, cct_node_t* node,
		     int metricId,
		     backtrace_t* bt,
		     cct_metric_data_t datum)
{
  return hpcrun_cct_insert_backtrace(cct, node, metricId, hpcrun_bt_last(bt), hpcrun_bt_beg(bt), datum);
}

//-----------------------------------------------------------------------------
// function: hpcrun_backtrace2cct
// purpose:
//     if successful, returns the leaf node representing the sample;
//     otherwise, returns NULL.
//-----------------------------------------------------------------------------

//
// TODO: one more flag:
//   backtrace needs to either:
//       IGNORE_TRAMPOLINE (usually, but not always called when isSync is true)
//       PLACE_TRAMPOLINE  (standard for normal async samples).
//             
cct_node_t*
hpcrun_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context, 
		     int metricId,
		     uint64_t metricIncr,
		     int skipInner, int isSync)
{
  cct_node_t* n = NULL;
  if (hpcrun_isLogicalUnwind()) {
    TMSG(LUSH,"lush backtrace2cct invoked");
    n = lush_backtrace2cct(cct, context, metricId, metricIncr, skipInner,
			   isSync);
  }
  else {
    TMSG(LUSH,"regular (NON-lush) backtrace2cct invoked");
    n = _hpcrun_backtrace2cct(cct, context, metricId, metricIncr, skipInner);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

//
// utility routine that does 3 things:
//   1) Generate a std backtrace
//   2) Modifies the backtrace according to a passed in function
//   3) enters the generated backtrace in the cct
//
cct_node_t*
hpcrun_bt2cct(hpcrun_cct_t *cct, ucontext_t* context,
	      int metricId, uint64_t metricIncr,
	      bt_mut_fn bt_fn, bt_fn_arg arg, int isSync)
{
  cct_node_t* n = NULL;
  if (hpcrun_isLogicalUnwind()) {
#ifdef LATER
    TMSG(LUSH,"lush backtrace2cct invoked");
    n = lush_backtrace2cct(cct, context, metricId, metricIncr, skipInner,
			   isSync);
#endif
  }
  else {
    TMSG(LUSH,"regular (NON-lush) bt2cct invoked");
    n = _hpcrun_bt2cct(cct, context, metricId, metricIncr, bt_fn, arg);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

//***************************************************************************
// Private Operations
//***************************************************************************

/* writing trees to streams of various kinds */

static int
hpcfile_cstree_write(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree, 
		     cct_node_t* root, 
		     cct_ctxt_t* tree_ctxt,
		     hpcfmt_uint_t num_metrics,
		     hpcfmt_uint_t num_nodes);

/* hpcrun_cct_fwrite: Write the tree 'x' to the file
   stream 'fs'.  The tree is written in HPC_CSTREE format.  Returns
   HPCRUN_OK upon success; HPCRUN_ERR on error. */

int 
hpcrun_cct_fwrite(FILE* fs, epoch_flags_t flags,
		  hpcrun_cct_t* x, cct_ctxt_t* x_ctxt)
{
  int ret;
  if (!fs) { return HPCRUN_ERR; }

  // Collect stats on the creation context
  unsigned int x_ctxt_len = cct_ctxt_length(x_ctxt);
  hpcfmt_uint_t num_nodes = x_ctxt_len + x->num_nodes;

  ret = hpcfile_cstree_write(fs, flags, x, x->tree_root, x_ctxt,
			     hpcrun_get_num_metrics(),
			     num_nodes);


  // splice creation context out of tree

  return (ret == HPCFMT_OK) ? HPCRUN_OK : HPCRUN_ERR;
}

//***************************************************************************
// hpcfile_cstree_write()
//***************************************************************************

static int
hpcfile_cstree_write_node(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree,
			  cct_node_t* node, 
			  hpcrun_fmt_cct_node_t* tmp_node,
			  int lvl_to_skip, int32_t parent_id);

static int
hpcfile_cstree_write_node_hlp(FILE* fs, epoch_flags_t flags, cct_node_t* node,
			      hpcrun_fmt_cct_node_t* tmp_node, 
			      int32_t parent_id);

static int
hpcfile_cstree_count_nodes(hpcrun_cct_t* tree, cct_node_t* node, 
			   int lvl_to_skip);

#undef NODE_CHILD_COUNT // FIXME: this is now dead code. can it be expunged?

#ifdef NODE_CHILD_COUNT 
static int
node_child_count(hpcrun_cct_t* tree, cct_node_t* node);
#endif



// See HPC_CSTREE format details

// hpcfile_cstree_write: Writes all nodes of the tree 'tree' and its
// context 'tree_ctxt' to file stream 'fs'; writing of 'tree' begins
// at 'root'.  The tree and its context should have 'num_nodes' nodes.
// Returns HPCFMT_OK upon success; HPCFMT_ERR on error.
static int
hpcfile_cstree_write(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree, 
		     cct_node_t* root,
		     cct_ctxt_t* tree_ctxt,
		     hpcfmt_uint_t num_metrics,
		     hpcfmt_uint_t num_nodes)
{
  int ret;
  int lvl_to_skip = 0;
  int32_t tree_parent_id = 0;

  // -------------------------------------------------------
  // the node has a creation context. in this case, we need 
  // to suppress the top two levels of the tree and splice 
  // the grandchildren of the tree root as kids of 
  // the node at the base of the context. 
  // -------------------------------------------------------
  if (tree_ctxt && tree_ctxt->context) {
    lvl_to_skip = 2;
    tree_parent_id = tree_ctxt->context->persistent_id;
  }
    
  if (!fs) { return HPCFMT_ERR; }

  // -------------------------------------------------------
  // When tree_ctxt is non-null, we will peel off the top
  // node in the tree to eliminate the call frame for
  // monitor_pthread_start_routine
  // -------------------------------------------------------

  // FIXME: if the creation context is non-NULL and the tree is empty, we
  // need to have a dummy root or a flag... so that the end of the
  // creation context is not interpreted as a leaf.
  
  // -------------------------------------------------------
  // Write num cct tree nodes
  // -------------------------------------------------------

  if (num_nodes > 0 && lvl_to_skip > 0) { // FIXME: better way...
    int skipped = hpcfile_cstree_count_nodes(tree, root, lvl_to_skip);
    num_nodes -= skipped;
  }
  hpcfmt_byte8_fwrite(num_nodes, fs);

  // -------------------------------------------------------
  // Write context
  // -------------------------------------------------------

  cct_ctxt_write(fs, flags, tree_ctxt);

  // -------------------------------------------------------
  // Write each node, beginning with root
  // -------------------------------------------------------
  hpcrun_fmt_cct_node_t tmp_node;

  hpcrun_fmt_cct_node_init(&tmp_node);
  tmp_node.num_metrics = num_metrics;
  tmp_node.metrics = alloca(num_metrics * sizeof(hpcrun_metricVal_t));

  ret = hpcfile_cstree_write_node(fs, flags, tree, root, &tmp_node,
				  lvl_to_skip, tree_parent_id);
  return ret;
}


static int
hpcfile_cstree_write_node(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree,
			  cct_node_t* node,
			  hpcrun_fmt_cct_node_t* tmp_node,
			  int lvl_to_skip, int32_t parent_id)
{
  int ret;

  if (!node) { return HPCFMT_OK; }

  // ---------------------------------------------------------
  // Write this node
  // ---------------------------------------------------------
  int my_lvl_to_skip = lvl_to_skip;
  int32_t my_parent;

  if (lvl_to_skip >= 0) {
    my_parent = parent_id;
  } else {
    my_parent = node->parent->persistent_id;
  }

  cct_node_t* first, *c;
  first = c = cct_node_firstChild(node);

  // no children ==> node is a leaf ==> persistent_id should be negative
  //
  if (! first) {
    node->persistent_id = -(node->persistent_id);
    TMSG(LEAF_NODE_WRITE, "changing sign of leaf node id %d", node->persistent_id);
  }

  if (lvl_to_skip <= 0) {
    ret = hpcfile_cstree_write_node_hlp(fs, flags, node, tmp_node, my_parent);
    if (ret != HPCRUN_OK) { 
      return HPCRUN_ERR; 
    }
  }

  my_lvl_to_skip--;
  
  // ---------------------------------------------------------
  // Write children (handles either a circular or non-circular structure)
  // ---------------------------------------------------------
  while (c) {
    ret = hpcfile_cstree_write_node(fs, flags, tree, c, tmp_node, 
				    my_lvl_to_skip, my_parent);
    if (ret != HPCFMT_OK) {
      return HPCFMT_ERR;
    }

    c = cct_node_nextSibling(c);
    if (c == first) { break; }
  }
  
  return HPCFMT_OK;
}


static int
hpcfile_cstree_write_node_hlp(FILE* fs, epoch_flags_t flags,
			      cct_node_t* node,
			      hpcrun_fmt_cct_node_t* tmp_node,
                              int32_t my_parent)
{
  int ret = HPCRUN_OK;


  // ---------------------------------------------------------
  // Write the node
  // ---------------------------------------------------------

  tmp_node->id = node->persistent_id;
  tmp_node->id_parent = my_parent;

  if (flags.flags.isLogicalUnwind) {
    TMSG(LUSH, "setting as_info for %d", tmp_node->id);
    tmp_node->as_info = node->as_info;
  }
  
  tmp_node->lm_id = 0; // FIXME:tallent

  // double casts to avoid warnings when pointer is < 64 bits 
  tmp_node->ip = (hpcfmt_vma_t) (unsigned long) node->ip;

  if (flags.flags.isLogicalUnwind) {
    TMSG(LUSH, "settiong lip for node %d", tmp_node->id);
    lush_lip_init(&tmp_node->lip);
    if (node->lip) {
      memcpy(&tmp_node->lip, node->lip, sizeof(lush_lip_t));
    }
  }

  memcpy(tmp_node->metrics, node->metrics, 
	 tmp_node->num_metrics * sizeof(cct_metric_data_t));

  ret = hpcrun_fmt_cct_node_fwrite(tmp_node, flags, fs);
  if (ret != HPCFMT_OK) { 
    return HPCRUN_ERR; 
  }

  return ret;
}


static int
hpcfile_cstree_count_nodes(hpcrun_cct_t* tree, cct_node_t* node, 
			   int lvl_to_skip)
{
  int skipped_subtree_count = 0;

  if (node) { 
    if (lvl_to_skip-- > 0) {
      skipped_subtree_count++; // count self

      // ---------------------------------------------------------
      // count skipped children 
      // (handles either a circular or non-circular structure)
      // ---------------------------------------------------------
      int kids = 0;
      cct_node_t* first = cct_node_firstChild(node);
      cct_node_t* c = first; 
      while (c) {
	kids += hpcfile_cstree_count_nodes(tree, c, lvl_to_skip);
	c = cct_node_nextSibling(c);
	if (c == first) { break; }
      }
      skipped_subtree_count += kids; 
    }
  }
  return skipped_subtree_count; 
}


#ifdef NODE_CHILD_COUNT 
static int
node_child_count(hpcrun_cct_t* tree, cct_node_t* node)
{
  int children = 0;
  if (node) { 
    // ---------------------------------------------------------
    // count children 
    // (handles either a circular or non-circular structure)
    // ---------------------------------------------------------
    cct_node_t* first = node_first_child(node);
    cct_node_t* c = first; 
    while (c) {
      children++;
      c = cct_node_nextSibling(c);
      if (c == first) { break; }
    }
  }
  return children; 
}
#endif


static cct_node_t*
_hpcrun_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context,
		      int metricId,
		      uint64_t metricIncr,
		      int skipInner)
{
  int  backtrace_trolled = 0;
  bool tramp_found       = false;

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();
  td->unwind   = td->btbuf; // innermost
  td->bufstk   = td->bufend;

  int unw_len = 0;
  while (true) {
    int ret;

    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    if (hpcrun_trampoline_interior(ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      unw_throw();
    }

    if (hpcrun_trampoline_at_entry(ip)) {
      if (unw_len == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	unw_throw();
      } else {
	// we have encountered a trampoline in the middle of an unwind.
	tramp_found = true;

	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    hpcrun_ensure_btbuf_avail();

    td->unwind->cursor = cursor;
    td->unwind->ip     = (void *) ip;
    td->unwind->ra_loc = NULL;
    frame_t* prev = td->unwind;
    td->unwind++;
    unw_len++;

    ret = unw_step(&cursor);
    backtrace_trolled = (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled){
    hpcrun_up_pmsg_count();
  }
  
  frame_t* bt_beg  = td->btbuf;      // innermost, inclusive
  frame_t* bt_last = td->unwind - 1; // outermost, inclusive
  frame_t* bt_end  = bt_last + 1;    // outermost, exclusive
  size_t new_frame_count = bt_end - bt_beg;

  cct_node_t* cct_cursor = NULL;

  if (tramp_found) {
    TMSG(BACKTRACE, "tramp stop: conjoining backtraces");
    //
    // join current backtrace fragment to previous trampoline-marked prefix
    // and make this new conjoined backtrace the cached-backtrace
    //
    frame_t* prefix = td->tramp_frame + 1; // skip top frame
    size_t old_frame_count = td->cached_bt_end - prefix;

    hpcrun_cached_bt_adjust_size(new_frame_count + old_frame_count);

    // put the old prefix in place
    memmove(td->cached_bt + new_frame_count, prefix, 
	   sizeof(frame_t) * old_frame_count);

    // put the new suffix in place
    memcpy(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    // update the length of the conjoined backtrace
    td->cached_bt_end = td->cached_bt + new_frame_count + old_frame_count;

    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor   = td->tramp_cct_node->parent;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p", i, fr->ip);
      }
      hpcrun_stats_num_samples_filtered_inc();
      return 0;
    }
  }

  //
  // FIXME: For the moment, ignore skipInner issues with trampolines.
  //        Eventually, this will need to be addressed
  //
  if (skipInner) {
    EMSG("WARNING: backtrace detects skipInner != 0 (skipInner = %d)", 
	 skipInner);
    bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
  }

  cct_node_t* n;
  n = hpcrun_cct_insert_backtrace(cct, cct_cursor, metricId,
				  bt_last, bt_beg,
				  (cct_metric_data_t){.i = metricIncr});

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}

static cct_node_t*
_hpcrun_bt2cct(hpcrun_cct_t *cct, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       bt_mut_fn bt_fn, bt_fn_arg bt_arg)
{
  int  backtrace_trolled = 0;
  bool tramp_found       = false;

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();

  backtrace_t* bt = &(td->bt);
  hpcrun_bt_reset(bt);

  while (true) {
    int ret;

    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    if (hpcrun_trampoline_interior(ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      unw_throw();
    }

    if (hpcrun_trampoline_at_entry(ip)) {
      if (hpcrun_bt_len(bt) == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	unw_throw();
      } else {
	// we have encountered a trampoline in the middle of an unwind.
	tramp_found = true;

	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    frame_t* prev = hpcrun_bt_push(bt,
				   &((frame_t){.cursor = cursor, .ip = (void*)ip, .ra_loc = NULL}));

    ret = unw_step(&cursor);
    backtrace_trolled = backtrace_trolled || (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled){
    hpcrun_up_pmsg_count();
  }
				    
  frame_t* bt_beg  = hpcrun_bt_beg(bt);

  size_t new_frame_count = hpcrun_bt_len(bt);

  cct_node_t* cct_cursor = NULL;

  if (tramp_found) {
    TMSG(BACKTRACE, "tramp stop: conjoining backtraces");
    //
    // join current backtrace fragment to previous trampoline-marked prefix
    // and make this new conjoined backtrace the cached-backtrace
    //
    frame_t* prefix = td->tramp_frame + 1; // skip top frame
    size_t old_frame_count = td->cached_bt_end - prefix;

    hpcrun_cached_bt_adjust_size(new_frame_count + old_frame_count);

    // put the old prefix in place
    memmove(td->cached_bt + new_frame_count, prefix, 
	   sizeof(frame_t) * old_frame_count);

    // put the new suffix in place
    memcpy(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    // update the length of the conjoined backtrace
    td->cached_bt_end = td->cached_bt + new_frame_count + old_frame_count;

    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor   = td->tramp_cct_node->parent;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p", i, fr->ip);
      }
      hpcrun_stats_num_samples_filtered_inc();
      return 0;
    }
  }

  //
  // mutate the backtrace according to the passed in mutator function
  //  (bt_fn == NULL means no mutation is necessary)
  //
  if (bt_fn) {
    TMSG(BT, "Mutation function called");
    TMSG(BT,"==== backtrace BEFORE mutation ========");
    hpcrun_dump_bt(bt);
    TMSG(BT,"-------------------------------");
    bt_fn(bt, bt_arg);
    TMSG(BT,"==== backtrace AFTER  mutation ========");
    hpcrun_dump_bt(bt);
    TMSG(BT,"-------------------------------");
  }

  cct_node_t* n;
  n = hpcrun_cct_insert_bt(cct, cct_cursor, metricId,
			   bt,
			   (cct_metric_data_t){.i = metricIncr});

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}

//***************************************************************************
//
//***************************************************************************

static int
cct_ctxt_writeGbl(FILE* fs, epoch_flags_t flags, cct_ctxt_t* cct_ctxt,
		   hpcrun_fmt_cct_node_t* tmp_node);

static int
cct_ctxt_writeLcl(FILE* fs, epoch_flags_t flags, cct_node_t* node,
		   hpcrun_fmt_cct_node_t* tmp_node);


unsigned int
cct_ctxt_length(cct_ctxt_t* cct_ctxt)
{
  unsigned int len = 0;
  for (cct_ctxt_t* ctxt = cct_ctxt; (ctxt); ctxt = ctxt->parent) {
    for (cct_node_t* x = ctxt->context; (x); x = x->parent) {
      len++;
    }
  }
  return len;
}


int
cct_ctxt_write(FILE* fs, epoch_flags_t flags, cct_ctxt_t* cct_ctxt)
{
  // N.B.: assumes that calling malloc is acceptible!

  int ret;
  unsigned int num_metrics = hpcrun_get_num_metrics();

  hpcrun_fmt_cct_node_t tmp_node;
  hpcrun_fmt_cct_node_init(&tmp_node);
  tmp_node.num_metrics = num_metrics;
  tmp_node.metrics = alloca(num_metrics * sizeof(hpcrun_metricVal_t));
    
  ret = cct_ctxt_writeGbl(fs, flags, cct_ctxt, &tmp_node);

  return ret;
}


// Post order write of 'cct_ctxt'
static int
cct_ctxt_writeGbl(FILE* fs, epoch_flags_t flags, cct_ctxt_t* cct_ctxt,
			 hpcrun_fmt_cct_node_t* tmp_node)
{
  int ret = HPCRUN_OK;
  
  // Base case
  if (!cct_ctxt) {
    return HPCRUN_OK;
  }

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = cct_ctxt_writeGbl(fs, flags, cct_ctxt->parent, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  // write this context
  ret = cct_ctxt_writeLcl(fs, flags, cct_ctxt->context, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  return ret;
}


// Post order write of 'node'
static int
cct_ctxt_writeLcl(FILE* fs, epoch_flags_t flags, cct_node_t* node,
		   hpcrun_fmt_cct_node_t* tmp_node)
{
  int ret = HPCRUN_OK;

  // Base case
  if (!node) {
    return HPCRUN_OK;
  }

  cct_node_t* parent = node->parent;
  int32_t parent_id = (parent ? parent->persistent_id : 0); 

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = cct_ctxt_writeLcl(fs, flags, parent, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  // write this node
  ret = hpcfile_cstree_write_node_hlp(fs, flags, node, tmp_node, parent_id);

  if (ret != HPCRUN_OK) {
    return HPCRUN_ERR; 
  }
  
  return ret;
}
