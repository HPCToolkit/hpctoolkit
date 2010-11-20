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
// Copyright ((c)) 2002-2010, Rice University 
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
#include <unwind/common/unw-datatypes.h>
#include <trampoline/common/trampoline.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/hpcrun_stats.h>
#include <lush/lush-backtrace.h>

//***************************** Local Macros ********************************

//*************************** Forward Declarations **************************

/* cstree callbacks (CB) */
static cct_node_t*
cct_node_find_child(cct_node_t* x,
		    lush_assoc_info_t as_info, ip_normalized_t ip_norm,
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
help_hpcrun_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context,
		      int metricId, uint64_t metricIncr,
		      int skipInner);


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
		ip_normalized_t ip_norm,
		lush_lip_t* lip)
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
  node->ip_norm = ip_norm;
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
  // NOTE: cct nodes here are in NON-freeable memory (to be passed to
  // other threads)
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


#if 0
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
#endif


bool
hpcrun_empty_cct(hpcrun_cct_t* cct)
{
  bool rv = (cct->num_nodes == 1);
  if (rv) {
    TMSG(CCT, "cct %p is empty", cct);
  }
  return rv;
}


#if 0
static bool
no_metric_samples(hpcrun_cct_t* cct)
{
  return (cct->num_nodes == 1) &&
    ((cct->tree_root->persistent_id == 0) || (all_metrics_0(cct->tree_root)));
}
#endif


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


// cct_node_link: adds a node to the head of the list of parent's children
// 
static int
cct_node_link(cct_node_t* x, cct_node_t* parent)
{
  /* Sanity check */
  if (x->parent != NULL) { return HPCRUN_ERR; } /* can only have one parent */

  assert(parent); // no adding children to NULL parents !

  x->next_sibling = parent->children;
  parent->children = x;
  x->parent = parent;

  return HPCRUN_OK;
}


// cct_node_find_child: finds the child of 'x' with
// instruction pointer equal to 'ip'.
//
// tallent: I slightly sanitized the different versions, but FIXME
//
static cct_node_t*
cct_node_find_child(cct_node_t* x,
		    lush_assoc_info_t as_info, ip_normalized_t ip_norm,
		    lush_lip_t* lip)
{
  cct_node_t* c, *first;
  lush_assoc_t as = lush_assoc_info__get_assoc(as_info);
      
  first = c = cct_node_firstChild(x);
  if (c) {
    do {
      // FIXME: abstract this an Prof::CCT::ANode::findDynChild
      lush_assoc_t c_as = lush_assoc_info__get_assoc(c->as_info);
      if (ip_normalized_eq(&c->ip_norm, &ip_norm)
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
  x->tree_root = cct_node_create(lush_assoc_info_NULL, ip_normalized_NULL, NULL);

  x->tree_root->parent = (ctxt)? ctxt->context : NULL;
  x->num_nodes = 1;
}


static const ip_normalized_t ip_normalized_PARTIAL = { .lm_id = HPCRUN_FMT_LMId_NULL,
						       .lm_ip = HPCRUN_FMT_LMIp_Flag1};
void
hpcrun_cct_attach_partial_unw_root(hpcrun_cct_t* cct)
{
#ifndef NO_SPECIAL_UNW_ROOT
  TMSG(PARTIAL_UNW, "Attaching partial unw_root");
  cct_node_t* partial_unw_root = cct_node_create(lush_assoc_info_NULL,
						 ip_normalized_PARTIAL,
						 NULL);
  cct_node_t* parent = cct->tree_root;
  cct_node_parent_insert(partial_unw_root, parent);
  cct->num_nodes++;
  cct->partial_unw_root = partial_unw_root;
#else
  TMSG(PARTIAL_UNW, "FAKING partial unw root");
  cct->partial_unw_root = cct->tree_root;
#endif
}

int
hpcrun_cct_init(hpcrun_cct_t* x, cct_ctxt_t* ctxt)
{
  TMSG(CCT,"--Init");
  memset(x, 0, sizeof(*x));
 
  hpcrun_cct_make_root(x, ctxt);
  hpcrun_cct_attach_partial_unw_root(x);

  return HPCRUN_OK;
}


int
hpcrun_cct_fini(hpcrun_cct_t *x)
{
  TMSG(CCT,"--Fini");
  return HPCRUN_OK;
}


// find a child with the specified pc. if none exists, create one.
cct_node_t*
hpcrun_cct_get_child(hpcrun_cct_t *cct, cct_node_t* parent, frame_t *frm)
{
  cct_node_t *c = cct_node_find_child(parent, frm->as_info, frm->ip_norm,
				      frm->lip);

  if (!c) {
    c = cct_node_create(frm->as_info, frm->ip_norm, frm->lip);
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

  if (ENABLED(THREAD_CTXT) && ENABLED(IN_THREAD_CTXT)) {
    TMSG(THREAD_CTXT, "Inserting context backtrace, cct root = %d, cct root parent = %d",
	 hpcrun_get_persistent_id(cct->tree_root),
	 hpcrun_get_persistent_id(cct->tree_root->parent));
  }
  if (tn == NULL) {
    tn = cct->tree_root;
    
    TMSG(CCT, "Starting tree node = (NULL), so begin search @ cct root");
    if (ip_normalized_eq(&frm->ip_norm, &tn->ip_norm)) {
      TMSG(CCT,"beg ip == tn ip ==> lm_id = %d and lm_ip = %p", 
	   tn->ip_norm.lm_id, tn->ip_norm.lm_ip);
      MY_advancePathFrame(frm);
    }
    
    
    TMSG(CCT, "beg ip ==> lm_id = %d and lm_ip = %p", frm->ip_norm.lm_id,
	 frm->ip_norm.lm_ip);
  }
	

  while (true) {
    if (MY_isPathFrameAtEnd(frm)) {
      break;
    }

    // Attempt to find a child 'c' corresponding to 'frm'
    TMSG(CCT,"looking for child in tree w ip ==> lm_id = %d and lm_ip = %p", 
	 frm->ip_norm.lm_id, frm->ip_norm.lm_ip);

    cct_node_t* c = cct_node_find_child(tn, frm->as_info, frm->ip_norm,
					frm->lip);
    if (c) {
      // 'c' represents 'frm'
      TMSG(CCT,"found child @ node %p", c);
      tn = c;

      // N.B. If 'frm' is a 1-to-1 bichord and 'c' is not (i.e., 'c'
      // is M-to-1 or 1-to-M), then update the association of 'c' to
      // reflect that 'c' is now a proxy for two bichord types (1-to-1
      // and M-to-1 or 1-to-M)
      lush_assoc_t as_frm = lush_assoc_info__get_assoc(frm->as_info);
      lush_assoc_t as_c   = lush_assoc_info__get_assoc(c->as_info);
      if (as_frm == LUSH_ASSOC_1_to_1 && as_c != LUSH_ASSOC_1_to_1) {
	// INVARIANT: c->as_info should be either M-to-1 or 1-to-M
	lush_assoc_info__set_assoc(c->as_info, LUSH_ASSOC_1_to_1);
      }
      MY_advancePathFrame(frm);
    }
    else {
      // no such child; insert new tail
      TMSG(CCT,"No child found, inserting new tail");
      
      while (!MY_isPathFrameAtEnd(frm)) {
	c = cct_node_create(frm->as_info, frm->ip_norm, frm->lip);
	TMSG(CCT, "create node %p w ip ==> lm_id = %d and offst = %p", c,
	     frm->ip_norm.lm_id, frm->ip_norm.lm_ip);
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
    n = help_hpcrun_backtrace2cct(cct, context, metricId, metricIncr, skipInner);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}


#if 0 // TODO: tallent: Use Mike's improved code; retire prior routines

static cct_node_t*
help_hpcrun_bt2cct(hpcrun_cct_t *cct, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       bt_mut_fn bt_fn, bt_fn_arg bt_arg);

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
    n = help_hpcrun_bt2cct(cct, context, metricId, metricIncr, bt_fn, arg);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

#endif

//***************************************************************************
// Private Operations
//***************************************************************************

static int
hpcrun_cct_fwrite_hlp(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree, 
		      cct_node_t* root, cct_ctxt_t* tree_ctxt,
		      hpcfmt_uint_t num_metrics, hpcfmt_uint_t num_nodes);


// hpcrun_cct_fwrite: Write the tree 'x' to the file stream
//   'fs'. Returns HPCRUN_OK upon success; HPCRUN_ERR on error
int 
hpcrun_cct_fwrite(FILE* fs, epoch_flags_t flags,
		  hpcrun_cct_t* x, cct_ctxt_t* x_ctxt)
{
  int ret;
  if (!fs) { return HPCRUN_ERR; }

  // Collect stats on the creation context
  unsigned int x_ctxt_len = cct_ctxt_length(x_ctxt);
  hpcfmt_uint_t num_nodes = x_ctxt_len + x->num_nodes;

  ret = hpcrun_cct_fwrite_hlp(fs, flags, x, x->tree_root, x_ctxt,
			      hpcrun_get_num_metrics(), num_nodes);


  // splice creation context out of tree

  return (ret == HPCFMT_OK) ? HPCRUN_OK : HPCRUN_ERR;
}


//***************************************************************************
// hpcrun_cct_fwrite_hlp()
//***************************************************************************

static int
hpcrun_cctNode_fwrite(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree,
		      cct_node_t* node, hpcrun_fmt_cct_node_t* tmp_node,
		      int lvl_to_skip, int32_t parent_id);

static int
hpcrun_cctNode_fwrite_hlp(bool is_ctxt_node, FILE* fs, epoch_flags_t flags,
			  cct_node_t* node, hpcrun_fmt_cct_node_t* tmp_node, 
			  int32_t parent_id, int32_t my_id);

static int
hpcrun_cct_countSkippedNodes(hpcrun_cct_t* tree, cct_node_t* node, 
			     int lvl_to_skip);


static int cct_count_nodes(cct_node_t* n);

static int
cct_count_sibs(cct_node_t* node)
{
  int rv = 0;
  for (cct_node_t* n = node->children; n; n = n->next_sibling) {
    rv += cct_count_nodes(n);
  }
  return rv;
}

static int
cct_count_nodes(cct_node_t* node)
{
  if (! node) {
    return 0;
  }
  return 1 + cct_count_sibs(node);
}


// hpcrun_cct_fwrite_hlp: Writes all nodes of the tree 'tree' and its
// context 'tree_ctxt' to file stream 'fs'; writing of 'tree' begins
// at 'root'.  The tree and its context should have 'num_nodes' nodes.
// Returns HPCFMT_OK upon success; HPCFMT_ERR on error.
static int
hpcrun_cct_fwrite_hlp(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree, 
		      cct_node_t* root, cct_ctxt_t* tree_ctxt,
		      hpcfmt_uint_t num_metrics, hpcfmt_uint_t num_nodes)
{
  int ret;
  int lvl_to_skip = 0;
  int32_t tree_parent_id = 0;

  // ------------------------------------------------------- 
  // If the CCT has a creation context, we suppress its outermost two
  // levels.  That is, we make the grandchildren of the CCT root kids
  // of the creation context.
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
    int skipped = hpcrun_cct_countSkippedNodes(tree, root, lvl_to_skip);
    num_nodes -= skipped;
    //
    // HACK: need to add back # nodes in the partial unwind, as they got skipped ...
    //
    num_nodes += cct_count_nodes(tree->partial_unw_root);
  }

  
  hpcfmt_int8_fwrite(num_nodes, fs);

  // -------------------------------------------------------
  // Write context
  // -------------------------------------------------------

  cct_ctxt_write(fs, flags, tree_ctxt);

  // -------------------------------------------------------
  // Prepare a "scratch" node for output
  // -------------------------------------------------------
  hpcrun_fmt_cct_node_t tmp_node;

  hpcrun_fmt_cct_node_init(&tmp_node);
  tmp_node.num_metrics = num_metrics;
  tmp_node.metrics = alloca(num_metrics * sizeof(hpcrun_metricVal_t));

  //
  // For nodes with non-null ctxt (== pthread cct), write out the partial unwinds
  //
  
  if (tree_ctxt && tree_ctxt->context) {
    hpcrun_cctNode_fwrite(fs, flags, tree,
			  tree->partial_unw_root, &tmp_node,
			  0 /* no skipping */, tree_parent_id);
  }
  
  // -------------------------------------------------------
  // Write each fully-unwound node, beginning with root, and skipping appropriate # of levels
  // -------------------------------------------------------

  ret = hpcrun_cctNode_fwrite(fs, flags, tree, root, &tmp_node,
			      lvl_to_skip, tree_parent_id);
  return ret;
}


static int
hpcrun_cctNode_fwrite(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* tree,
		      cct_node_t* node, hpcrun_fmt_cct_node_t* tmp_node,
		      int lvl_to_skip, int32_t parent_id)
{
  int ret;

  if (!node) { return HPCFMT_OK; }

  // ---------------------------------------------------------
  // Write this node
  // ---------------------------------------------------------
  int my_lvl_to_skip = lvl_to_skip;
  int32_t my_parent;
  int32_t my_id;

  if (lvl_to_skip >= 0) {
    my_parent = parent_id;
  }
  else {
    my_parent = node->parent->persistent_id;
  }

  cct_node_t* first, *c;
  first = c = cct_node_firstChild(node);

  my_id = node->persistent_id;

  // no children ==> node is a leaf ==> report negative persistent_id 
  //
  if (! first) {
    my_id = -my_id;
    TMSG(LEAF_NODE_WRITE, "changing sign of leaf node id %d", my_id);
  }

  if (lvl_to_skip <= 0) {
    ret = hpcrun_cctNode_fwrite_hlp(false, fs, flags, node, tmp_node,
				    my_parent, my_id);
    if (ret != HPCRUN_OK) {
      return HPCRUN_ERR;
    }
  }

  my_lvl_to_skip--;
  
  // ---------------------------------------------------------
  // Write children (handles either a circular or non-circular structure)
  // ---------------------------------------------------------
  while (c) {
    ret = hpcrun_cctNode_fwrite(fs, flags, tree, c, tmp_node,
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
hpcrun_cctNode_fwrite_hlp(bool is_ctxt_node, FILE* fs, epoch_flags_t flags,
			  cct_node_t* node, hpcrun_fmt_cct_node_t* tmp_node,
			  int32_t my_parent, int32_t my_id)
{
  if (is_ctxt_node) {
    TMSG(THREAD_CTXT, "%d", my_id);
  }
  int ret = HPCRUN_OK;
  

  // ---------------------------------------------------------
  // Write the node
  // ---------------------------------------------------------

  tmp_node->id = my_id;
  tmp_node->id_parent = my_parent;

  if (flags.fields.isLogicalUnwind) {
    TMSG(LUSH, "setting as_info for %d", tmp_node->id);
    tmp_node->as_info = node->as_info;
  }
  
  tmp_node->lm_id = node->ip_norm.lm_id; // FIXME:tallent

  // double casts to avoid warnings when pointer is < 64 bits 
  tmp_node->lm_ip = (hpcfmt_vma_t) (uintptr_t) node->ip_norm.lm_ip;

  if (flags.fields.isLogicalUnwind) {
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
hpcrun_cct_countSkippedNodes(hpcrun_cct_t* tree, cct_node_t* node, 
			     int lvl_to_skip)
{
  int numSkippedNodes = 0;

  if (!node) {
    return numSkippedNodes;
  }

  if (lvl_to_skip > 0) {
    numSkippedNodes++; // count self
    
    lvl_to_skip--;
    
    // ---------------------------------------------------------
    // count skipped children 
    // (handles either a circular or non-circular structure)
    // ---------------------------------------------------------
    int numSkippedKids = 0;
    cct_node_t* first = cct_node_firstChild(node);
    cct_node_t* c = first;
    while (c) {
      numSkippedKids += hpcrun_cct_countSkippedNodes(tree, c, lvl_to_skip);
      c = cct_node_nextSibling(c);
      if (c == first) { break; }
    }
    numSkippedNodes += numSkippedKids;
  }
  
  return numSkippedNodes;
}

cct_node_t*
hpcrun_cct_record_backtrace(hpcrun_cct_t* cct, bool partial,
			    frame_t* bt_beg, frame_t* bt_last, bool tramp_found,
			    int metricId, uint64_t metricIncr)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* cct_cursor = NULL;
  if (tramp_found) {
    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor = td->tramp_cct_node->parent;
  }
  if (partial) {
    cct_cursor = cct->partial_unw_root;
  }

  return hpcrun_cct_insert_backtrace(cct, cct_cursor, metricId,
				     bt_last, bt_beg,
				     (cct_metric_data_t){.i = metricIncr});

}

cct_node_t*
hpcrun_dbg_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context,
			 int metricId,
			 uint64_t metricIncr,
			 int skipInner)
{
  bool tramp_found;
  frame_t* bt_beg;
  frame_t* bt_last;
  thread_data_t* td = hpcrun_get_thread_data();

  if (! hpcrun_dbg_generate_graceful_backtrace(context,
					       &bt_beg, &bt_last, &tramp_found,
					       skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
    else {
      TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure");
      hpcrun_stats_num_samples_partial_inc();
    }
  }

  cct_node_t* n = hpcrun_cct_record_backtrace(cct, true,
					      bt_beg, bt_last, tramp_found,
					      metricId, metricIncr);

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}

static cct_node_t*
help_hpcrun_backtrace2cct(hpcrun_cct_t* cct, ucontext_t* context,
		      int metricId,
		      uint64_t metricIncr,
		      int skipInner)
{
  bool tramp_found;
  frame_t* bt_beg;
  frame_t* bt_last;
  thread_data_t* td = hpcrun_get_thread_data();

  bool partial_unw = false;
  if (! hpcrun_generate_backtrace(context,
				  &bt_beg, &bt_last, &tramp_found,
				  skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
    TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure");
    hpcrun_stats_num_samples_partial_inc();
    partial_unw = true;
  }

  cct_node_t* n = hpcrun_cct_record_backtrace(cct, partial_unw,
					      bt_beg, bt_last, tramp_found,
					      metricId, metricIncr);

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();

#define IGNORE_SKIP_INNER_UNTIL_IT_DOESNT_BREAK_TRAMPOLINE 1
#if IGNORE_SKIP_INNER_UNTIL_IT_DOESNT_BREAK_TRAMPOLINE
    td->tramp_frame = td->cached_bt;
#else
    // I don't understand why this isn't the correct way to handle skipping
    // of inner frames. Perhaps another change is needed in 
    // hpcrun_trampoline_insert or in the cursor representation (e.g. ra_loc).
    td->tramp_frame = bt_beg;
#endif
    hpcrun_trampoline_insert(n);
  }

  return n;
}


#if 0 // TODO: tallent: Use Mike's improved code; retire prior routines
static cct_node_t*
help_hpcrun_bt2cct(hpcrun_cct_t *cct, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       bt_mut_fn bt_fn, bt_fn_arg bt_arg)
{
  bool tramp_found;
  if (! hpcrun_gen_bt(context, &tramp_found, bt_fn, bt_arg)) {
    return NULL;
  }

  thread_data_t* td = hpcrun_get_thread_data();

  cct_node_t* cct_cursor = NULL;
  if (tramp_found) {
    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor   = td->tramp_cct_node->parent;
  }

  cct_node_t* n;
  n = hpcrun_cct_insert_bt(cct, cct_cursor, metricId,
			   &(td->bt),
			   (cct_metric_data_t){.i = metricIncr});

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}
#endif


int32_t
hpcrun_get_persistent_id(cct_node_t* node)
{
  return node ? node->persistent_id : -1;
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

  if (! cct_ctxt) {
    return 0;
  }

  for (cct_node_t* x = cct_ctxt->context; (x); x = x->parent) {
    len++;
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
  if (cct_ctxt) {
    TMSG(THREAD_CTXT, "Tracking down context for %d",
	 hpcrun_get_persistent_id(cct_ctxt->context));
  }

  int ret = HPCRUN_OK;
  
  // Base case
  if (!cct_ctxt) {
    return HPCRUN_OK;
  }

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  
  TMSG(THREAD_CTXT, "Context for node %d", hpcrun_get_persistent_id(cct_ctxt->context));
  // write this context
  ret = cct_ctxt_writeLcl(fs, flags, cct_ctxt->context, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  return ret;
}


void
cct_dump_path(cct_node_t* node)
{
  if (node) {
    cct_dump_path(node->parent);
    TMSG(THREAD_CTXT, "%d", node->persistent_id);
  }
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
  ret = hpcrun_cctNode_fwrite_hlp(true, fs, flags, node, tmp_node, parent_id,
				  node->persistent_id);

  if (ret != HPCRUN_OK) {
    return HPCRUN_ERR;
  }
  
  return ret;
}
