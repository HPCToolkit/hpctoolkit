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
//   csprof_cct.c
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

//*************************** User Include Files ****************************

#include "cct.h"
#include "csprof-malloc.h"
#include "csproflib_private.h"
#include "list.h"
#include "metrics.h"

#include "hpcrun_return_codes.h"

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/lush/lush-support.h>

//*************************** Forward Declarations **************************

/* cstree callbacks (CB) */
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x,
			    lush_assoc_info_t as_info, void* ip,
			    lush_lip_t* lip);

static int
csprof_cct_node__link(csprof_cct_node_t *, csprof_cct_node_t *);

//*************************** Forward Declarations **************************

// FIXME: tallent: when code is merged into hpctoolkit tree, this
// should come from src/lib/support/Logic.hpp (where a C version can
// easily be created).
static inline bool implies(bool p, bool q) { return (!p || q); }

//***************************************************************************
//

//***************************************************************************

//
// tallent (via fagan):
// FIXME:
//  The linear lookup of node children should be improved.
//  A full on red-black tree data structure for the children is probably overkill.
//  Just putting the most recently used child at the front of the list would be sufficient
//

static uint32_t 
new_persistent_id()
{
  // by default, all persistent ids are even; odd ids signify that we need 
  // to retain them as call path ids associated with a trace.
  static long global_persistent_id = 2;
  uint32_t myid = (int) fetch_and_add(&global_persistent_id, 2); 
  return myid;
}

static csprof_cct_node_t *
csprof_cct_node__create(lush_assoc_info_t as_info, 
			void* ip,
			lush_lip_t* lip,
			hpcrun_cct_t *x)
{

  size_t sz = (sizeof(csprof_cct_node_t)
	       + sizeof(cct_metric_data_t)*(csprof_get_max_metrics() - 1));
  csprof_cct_node_t *node;

  // FIXME: when multiple epochs really work, this will always be freeable.
  if (ENABLED(FREEABLE)) {
    node = csprof_malloc_freeable(sz);
  } else {
    node = csprof_malloc(sz);
  }

  memset(node, 0, sz);

  node->as_info = as_info; // LUSH
  node->ip = ip;
  node->lip = lip;         // LUSH

  node->persistent_id = new_persistent_id();

  node->next_sibling = NULL;

  return node;
}

csprof_cct_node_t*
hpcrun_copy_btrace(csprof_cct_node_t* n)
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
  csprof_cct_node_t* rv = (csprof_cct_node_t*) csprof_malloc(sizeof(csprof_cct_node_t));

  memcpy(rv, n, sizeof(csprof_cct_node_t));
  rv->parent   = NULL;
  rv->children = NULL;

  csprof_cct_node_t* prev = rv;
  for(n = n->parent; n; n = n->parent){
    TMSG(CCT_CTXT, "ctxt node(%p) ==> parent(%p) :: child(%p)", n, n->parent, n->children);
    csprof_cct_node_t* cpy = (csprof_cct_node_t*) csprof_malloc(sizeof(csprof_cct_node_t));
    memcpy(cpy, n, sizeof(csprof_cct_node_t));
    cpy->parent   = NULL;
    cpy->children = NULL;

    prev->parent  = cpy;
    prev          = cpy;
  }
  return rv;
}

static bool
all_metrics_0(csprof_cct_node_t* node)
{
  cct_metric_data_t* metrics = &(node->metrics[0]);
  int num_metrics            = csprof_num_recorded_metrics();
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


lush_cct_ctxt_t * 
copy_thr_ctxt(lush_cct_ctxt_t* thr_ctxt)
{
  // MEMORY PROBLEM: if thr_ctxt is reclaimed 
  // FIXME: if reclamation of a thread context is possible, we would need
  //        a deep copy here.
  return thr_ctxt;
}


static void
csprof_cct_node__parent_insert(csprof_cct_node_t *x, csprof_cct_node_t *parent)
{
  csprof_cct_node__link(x, parent);
#ifdef CSPROF_TRAMPOLINE_BACKEND
  // FIXME:LUSH: for lush, must match assoc and lip
  rbtree_insert(&parent->tree_children, x);
#endif
}

/* csprof_cct_node__link: links a node to a parent and at
   the end of the circular doubly-linked list of its siblings (if any) */
static int
csprof_cct_node__link(csprof_cct_node_t* x, csprof_cct_node_t* parent)
{
  /* Sanity check */
  if (x->parent != NULL) { return HPCRUN_ERR; } /* can only have one parent */
  
  if (parent != NULL) {
    /* Children are maintained as a doubly linked ring.  A new node
       is linked at the end of the ring (as a predecessor of
       "parent->children") which points to first child in the ring */
    csprof_cct_node_t *first_sibling = parent->children;
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


// csprof_cct_node__find_child: finds the child of 'x' with
// instruction pointer equal to 'ip'.
//
// tallent: I slightly sanitized the different versions, but FIXME
//
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x,
			    lush_assoc_info_t as_info, void* ip,
			    lush_lip_t* lip)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
  // FIXME:LUSH: match assoc and lip
  struct rbtree_node *node = rbtree_search(&x->tree_children, ip);

  return node ? DATA(node) : NULL;
#else
  csprof_cct_node_t* c, *first;
  lush_assoc_t as = lush_assoc_info__get_assoc(as_info);
      
  first = c = csprof_cct_node__first_child(x);
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

      c = csprof_cct_node__next_sibling(c);
    } while (c != NULL);
  }

  return NULL;
#endif
}



//***************************************************************************
//
//***************************************************************************

/* building and deleting trees */

int
csprof_cct__init(hpcrun_cct_t* x, lush_cct_ctxt_t* ctxt)
{
  TMSG(CCT_TYPE,"--Init");
  memset(x, 0, sizeof(*x));

#ifndef CSPROF_TRAMPOLINE_BACKEND
  {
    unsigned int l;

    /* initialize cached arrays */
    x->cache_len = l = CSPROF_BACKTRACE_CACHE_INIT_SZ;
    TMSG(CCT,"cct__init allocate cache_bt");
    x->cache_bt    = csprof_malloc(sizeof(void*) * l);
    TMSG(CCT,"cct__init allocate cache_nodes");
    x->cache_nodes = csprof_malloc(sizeof(csprof_cct_node_t*) * l);
  }
#endif

  // introduce bogus root to handle possible forests
  x->tree_root = csprof_cct_node__create(lush_assoc_info_NULL, 0, NULL, x);

  x->tree_root->parent = (ctxt)? ctxt->context : NULL;
  x->num_nodes = 1;

  return HPCRUN_OK;
}

int
csprof_cct__fini(hpcrun_cct_t *x)
{
  TMSG(CCT_TYPE,"--Fini");
  return HPCRUN_OK;
}


// find a child with the specified pc. if none exists, create one.
csprof_cct_node_t*
csprof_cct_get_child(hpcrun_cct_t *cct, csprof_cct_node_t *parent, csprof_frame_t *frm)
{
  csprof_cct_node_t *c = csprof_cct_node__find_child(parent, frm->as_info, frm->ip, frm->lip);  

  if (!c) {
    c = csprof_cct_node__create(frm->as_info, frm->ip, frm->lip, cct);
    csprof_cct_node__parent_insert(c, parent);
    cct->num_nodes++;
  }


  return c;
} 

// See usage in header.
csprof_cct_node_t*
csprof_cct_insert_backtrace(hpcrun_cct_t *x, void *treenode, int metric_id,
			    csprof_frame_t *path_beg, csprof_frame_t *path_end,
			    cct_metric_data_t increment)
{
#define csprof_MY_ADVANCE_PATH_FRAME(x)   (x)--
#define csprof_MY_IS_PATH_FRAME_AT_END(x) ((x) < path_end)

  TMSG(CCT,"Insert backtrace w x=%lp,tn=%lp,strt=%lp,end=%lp", x, treenode,
      path_beg, path_end);

  csprof_frame_t* frm = path_beg; // current frame 
  csprof_cct_node_t *tn = (csprof_cct_node_t *)treenode;

  if ( !(path_beg >= path_end) ) {
    return NULL;
  }

  if (tn == NULL) {
    tn = x->tree_root;

    TMSG(CCT, "(NULL) root ip %#lx", tn->ip);
    TMSG(CCT, "beg ip %#lx", frm->ip);

    // tallent: what is this for?
    /* we don't want the tree root calling itself */
    if (frm->ip == tn->ip) {
      TMSG(CCT,"beg ip == tn ip = %lx", tn->ip);
      csprof_MY_ADVANCE_PATH_FRAME(frm);
    }

    TMSG(CCT, "beg ip %#lx", frm->ip);
  }

  while (1) {
    if (csprof_MY_IS_PATH_FRAME_AT_END(frm)) {
      break;
    }

    // Attempt to find a child 'c' corresponding to 'frm'
    TMSG(CCT,"finding child in tree w ip = %lx", frm->ip);

    csprof_cct_node_t *c;
    c = csprof_cct_node__find_child(tn, frm->as_info, frm->ip, frm->lip);
    if (c) {
      // child exists; recur
      TMSG(CCT,"found child");
      tn = c;

      // If as_frm is 1-to-1 and as_c is not, update the latter
      lush_assoc_t as_frm = lush_assoc_info__get_assoc(frm->as_info);
      lush_assoc_t as_c = lush_assoc_info__get_assoc(c->as_info);
      if (as_frm == LUSH_ASSOC_1_to_1 && as_c != LUSH_ASSOC_1_to_1) {
	// INVARIANT: c->as_info must be either M-to-1 or 1-to-M
	lush_assoc_info__set_assoc(c->as_info, LUSH_ASSOC_1_to_1);
      }
      csprof_MY_ADVANCE_PATH_FRAME(frm);
    }
    else {
      // no such child; insert new tail
      TMSG(CCT,"No child found, inserting new tail");
      
      while (!csprof_MY_IS_PATH_FRAME_AT_END(frm)) {
	TMSG(CCT,"create node w ip = %lx",frm->ip);
	c = csprof_cct_node__create(frm->as_info, frm->ip, frm->lip, x);
	csprof_cct_node__parent_insert(c, tn);
	x->num_nodes++;
	
	tn = c;
	csprof_MY_ADVANCE_PATH_FRAME(frm);
      }
    }
  }

  TMSG(CCT, "Inserted in %#lx for count %d", x, x->num_nodes);

  cct_metric_data_increment(metric_id, &tn->metrics[metric_id], increment);
  return tn;
}


//***************************************************************************
//
//***************************************************************************

/* writing trees to streams of various kinds */

static int
hpcfile_cstree_write(FILE* fs, hpcrun_cct_t* tree, 
		     csprof_cct_node_t* root, 
		     lush_cct_ctxt_t* tree_ctxt,
		     hpcfmt_uint_t num_metrics,
		     hpcfmt_uint_t num_nodes);


/* csprof_cct__write_bin: Write the tree 'x' to the file
   stream 'fs'.  The tree is written in HPC_CSTREE format.  Returns
   HPCRUN_OK upon success; HPCRUN_ERR on error. */

int 
csprof_cct__write_bin(FILE* fs, hpcrun_cct_t* x, lush_cct_ctxt_t* x_ctxt)
{
  int ret;
  if (!fs) { return HPCRUN_ERR; }

  // Collect stats on the creation context
  unsigned int x_ctxt_len = lush_cct_ctxt__length(x_ctxt);
  hpcfmt_uint_t num_nodes = x_ctxt_len + x->num_nodes;

  ret = hpcfile_cstree_write(fs, x, x->tree_root, x_ctxt,
			     csprof_num_recorded_metrics(),
			     num_nodes);


  // splice creation context out of tree

  return (ret == HPCFMT_OK) ? HPCRUN_OK : HPCRUN_ERR;
}

//***************************************************************************
// hpcfile_cstree_write()
//***************************************************************************

// tallent: I moved this here from hpcfile_cstree.c.  With LUSH, the
// cct writing is becoming more complicated.  Because, 1) to support
// LUSH the callback interface is no longer sufficient; 2) we want
// writing to be stremlined; and 3) there is no need for a CCT
// feedback loop (as with experiment databases), I no longer think
// that we benefit from an abstract tree writer.  I do think an
// abstract reader is useful and it can be maintain its simple
// interface given a reasonable format.
// -- PLEASE remove once we agree this is a good decision. --


static int
hpcfile_cstree_write_node(FILE* fs, hpcrun_cct_t* tree,
			  csprof_cct_node_t* node, 
			  hpcfile_cstree_node_t* tmp_node,
			  int lvl_to_skip, int32_t parent_id);

static int
hpcfile_cstree_write_node_hlp(FILE* fs, csprof_cct_node_t* node,
			      hpcfile_cstree_node_t* tmp_node, 
			      int32_t parent_id);

static int
hpcfile_cstree_count_nodes(hpcrun_cct_t* tree, csprof_cct_node_t* node, 
			   int lvl_to_skip);

#undef NODE_CHILD_COUNT // FIXME: this is now dead code. can it be expunged?

#ifdef NODE_CHILD_COUNT 
static int
node_child_count(hpcrun_cct_t* tree, csprof_cct_node_t* node);
#endif



// See HPC_CSTREE format details

// hpcfile_cstree_write: Writes all nodes of the tree 'tree' and its
// context 'tree_ctxt' to file stream 'fs'; writing of 'tree' begins
// at 'root'.  The tree and its context should have 'num_nodes' nodes.
// Returns HPCFMT_OK upon success; HPCFMT_ERR on error.
static int
hpcfile_cstree_write(FILE* fs, hpcrun_cct_t* tree, 
		     csprof_cct_node_t* root,
		     lush_cct_ctxt_t* tree_ctxt,
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

  lush_cct_ctxt__write(fs, tree_ctxt);

  // -------------------------------------------------------
  // Write each node, beginning with root
  // -------------------------------------------------------
  hpcfile_cstree_node_t tmp_node;

  hpcfile_cstree_node__init(&tmp_node);
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = alloca(num_metrics * sizeof(hpcfmt_uint_t));

  ret = hpcfile_cstree_write_node(fs, tree, root, &tmp_node, 
				  lvl_to_skip, tree_parent_id);
  return ret;
}


static int
hpcfile_cstree_write_node(FILE* fs, hpcrun_cct_t* tree,
			  csprof_cct_node_t* node,
			  hpcfile_cstree_node_t* tmp_node,
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

  if (lvl_to_skip <= 0) {
    ret = hpcfile_cstree_write_node_hlp(fs, node, tmp_node, my_parent);
    if (ret != HPCRUN_OK) { 
      return HPCRUN_ERR; 
    }
  }

  my_lvl_to_skip--;
  
  // ---------------------------------------------------------
  // Write children (handles either a circular or non-circular structure)
  // ---------------------------------------------------------
  csprof_cct_node_t* first, *c;
  first = c = csprof_cct_node__first_child(node);
  while (c) {
    ret = hpcfile_cstree_write_node(fs, tree, c, tmp_node, 
				    my_lvl_to_skip, my_parent);
    if (ret != HPCFMT_OK) {
      return HPCFMT_ERR;
    }

    c = csprof_cct_node__next_sibling(c);
    if (c == first) { break; }
  }
  
  return HPCFMT_OK;
}

static int
hpcfile_cstree_write_node_hlp(FILE* fs, csprof_cct_node_t* node,
			      hpcfile_cstree_node_t* tmp_node,
                              int32_t my_parent)
{
  int ret = HPCRUN_OK;


  // ---------------------------------------------------------
  // Write the node
  // ---------------------------------------------------------

  tmp_node->id = node->persistent_id;
  tmp_node->id_parent = my_parent;

  // lush data
  //
  tmp_node->data.as_info = node->as_info;
  lush_lip_init(&tmp_node->data.lip);
  if (node->lip) {
    memcpy(&tmp_node->data.lip, node->lip, sizeof(lush_lip_t));
  }

  // double casts to avoid warnings when pointer is < 64 bits 
  tmp_node->data.ip = (hpcfmt_vma_t) (unsigned long) node->ip;

#if defined(OLD_LIP)
  tmp_node->data.lip.id = id_lip;
  tmp_node->data.lip.ptr = node->lip;
#endif
  
  memcpy(tmp_node->data.metrics, node->metrics, 
	 tmp_node->data.num_metrics * sizeof(cct_metric_data_t));

  ret = hpcfile_cstree_node__fwrite(tmp_node, fs);
  if (ret != HPCFMT_OK) { 
    return HPCRUN_ERR; 
  }

  return ret;
}


static int
hpcfile_cstree_count_nodes(hpcrun_cct_t* tree, csprof_cct_node_t* node, 
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
      csprof_cct_node_t* first = csprof_cct_node__first_child(node);
      csprof_cct_node_t* c = first; 
      while (c) {
	kids += hpcfile_cstree_count_nodes(tree, c, lvl_to_skip);
	c = csprof_cct_node__next_sibling(c);
	if (c == first) { break; }
      }
      skipped_subtree_count += kids; 
    }
  }
  return skipped_subtree_count; 
}


#ifdef NODE_CHILD_COUNT 
static int
node_child_count(hpcrun_cct_t* tree, csprof_cct_node_t* node)
{
  int children = 0;
  if (node) { 
    // ---------------------------------------------------------
    // count children 
    // (handles either a circular or non-circular structure)
    // ---------------------------------------------------------
    csprof_cct_node_t* first = csprof_cct_node__first_child(node);
    csprof_cct_node_t* c = first; 
    while (c) {
      children++;
      c = csprof_cct_node__next_sibling(c);
      if (c == first) { break; }
    }
  }
  return children; 
}
#endif


//***************************************************************************
//
//***************************************************************************

static int
lush_cct_ctxt__write_gbl(FILE* fs, lush_cct_ctxt_t* cct_ctxt,
			 hpcfile_cstree_node_t* tmp_node);

static int
lush_cct_ctxt__write_lcl(FILE* fs, csprof_cct_node_t* node,
			 hpcfile_cstree_node_t* tmp_node);


unsigned int
lush_cct_ctxt__length(lush_cct_ctxt_t* cct_ctxt)
{
  unsigned int len = 0;
  for (lush_cct_ctxt_t* ctxt = cct_ctxt; (ctxt); ctxt = ctxt->parent) {
    for (csprof_cct_node_t* x = ctxt->context; (x); x = x->parent) {
      len++;
    }
  }
  return len;
}


int
lush_cct_ctxt__write(FILE* fs, lush_cct_ctxt_t* cct_ctxt)
{
  // N.B.: assumes that calling malloc is acceptible!

  int ret;
  unsigned int num_metrics = csprof_num_recorded_metrics();

  hpcfile_cstree_node_t tmp_node;
  hpcfile_cstree_node__init(&tmp_node);
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = alloca(num_metrics * sizeof(hpcfmt_uint_t));
    
  ret = lush_cct_ctxt__write_gbl(fs, cct_ctxt, &tmp_node);

  return ret;
}


// Post order write of 'cct_ctxt'
static int
lush_cct_ctxt__write_gbl(FILE* fs, lush_cct_ctxt_t* cct_ctxt,
			 hpcfile_cstree_node_t* tmp_node)
{
  int ret = HPCRUN_OK;
  
  // Base case
  if (!cct_ctxt) {
    return HPCRUN_OK;
  }

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = lush_cct_ctxt__write_gbl(fs, cct_ctxt->parent, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  // write this context
  ret = lush_cct_ctxt__write_lcl(fs, cct_ctxt->context, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  return ret;
}


// Post order write of 'node'
static int
lush_cct_ctxt__write_lcl(FILE* fs, csprof_cct_node_t* node,
			 hpcfile_cstree_node_t* tmp_node)
{
  int ret = HPCRUN_OK;

  // Base case
  if (!node) {
    return HPCRUN_OK;
  }

  csprof_cct_node_t* parent = node->parent;
  int32_t parent_id = (parent ? parent->persistent_id : 0); 

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = lush_cct_ctxt__write_lcl(fs, parent, tmp_node);
  if (ret != HPCRUN_OK) { return HPCRUN_ERR; }
  
  // write this node
  ret = hpcfile_cstree_write_node_hlp(fs, node, tmp_node, parent_id);

  if (ret != HPCRUN_OK) {
    return HPCRUN_ERR; 
  }
  
  return ret;
}
