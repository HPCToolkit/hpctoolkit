// -*-Mode: C++;-*- // technically C99
// $Id$

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

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include "cct.h"
#include "general.h"
#include "csproflib_private.h"
#include "mem.h"
#include "list.h"
#include "metrics.h"

#include "hpcfile_cstreelib.h"

//*************************** Forward Declarations **************************

/* cstree callbacks (CB) */
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x,
			    lush_assoc_info_t as_info, void* ip,
			    lush_lip_t* lip,
			    void* sp);

static int
csprof_cct_node__link(csprof_cct_node_t *, csprof_cct_node_t *);

//*************************** Forward Declarations **************************

// FIXME: tallent: when code is merged into hpctoolkit tree, this
// should come from src/lib/support/Logic.hpp (where a C version can
// easily be created).
static bool implies(bool p, bool q) { return (!p || q); }


//***************************************************************************
//
//***************************************************************************

/* balanced red black trees for finding children */

/* for efficiency's sake, we store the color information in the low-order
   bits of the data pointer of a node.  these macros let us manipulate
   said color. */
#define RED 1
#define BLACK 0
#define MASK 3L

#define COLOR(node) (((unsigned long)((node)->data)) & MASK)
#define SET_COLOR(node, c)					\
  {								\
    csprof_cct_node_t *p = (node)->data;			\
    unsigned long pl = (unsigned long) p;			\
								\
    (node)->data = (csprof_cct_node_t *)((pl & ~MASK) | (c));	\
  }
#define REDDEN(node) SET_COLOR((node), RED)
#define BLACKEN(node) SET_COLOR((node), BLACK)
#define IS_RED(node) (COLOR((node)))

#define DATA(node) ((csprof_cct_node_t *)((unsigned long)((node)->data) & ~MASK))
#define SET_DATA(node, d)			\
  {						\
    unsigned long c = COLOR(node);		\
						\
    node->data = d;				\
    SET_COLOR(node, c);				\
  }

/* fills one cache line.  we think this is a good thing */
struct rbtree_node
{
  struct rbtree_node *parent;
  struct rbtree_node *children[2];
  csprof_cct_node_t *data;
};

#define RIGHT_INDEX 0
#define LEFT_INDEX 1
#define LEFT(node) ((node)->children[LEFT_INDEX])
#define RIGHT(node) ((node)->children[RIGHT_INDEX])

static struct rbtree_node *rbtree_node_create(csprof_cct_node_t *,
                                              struct rbtree_node *);
static struct rbtree_node *rbtree_search(struct rbtree *, void *, void *);
static void rbtree_insert(struct rbtree *, csprof_cct_node_t *);


static struct rbtree_node *
rbtree_node_create(csprof_cct_node_t *x, struct rbtree_node *parent)
{
  struct rbtree_node *node = csprof_malloc(sizeof(struct rbtree_node));

  node->parent = parent;
  LEFT(node) = NULL;
  RIGHT(node) = NULL;
  node->data = x;
  REDDEN(node);

  return node;
}
    
static struct rbtree_node *
rbtree_search(struct rbtree *tree, void *ip, void *sp)
{
  struct rbtree_node *curr = tree->root;

  while(curr != NULL) {
    void *cip = DATA(curr)->ip;
    int val = (cip == ip);

    if(val) {
      return curr;
    }
    else {
      int val = (ip < cip);

      curr = curr->children[val];
    }
  }

  return NULL;
}

/* Rotate the right child of parent upwards */
static void
rbtree_left_rotate(struct rbtree *root, struct rbtree_node *parent)
{
  struct rbtree_node *curr;

  curr = RIGHT(parent);

  if (curr != NULL) {
    RIGHT(parent) = LEFT(curr);
    if (LEFT(curr) != NULL) {
      LEFT(curr)->parent = parent;
    }

    curr->parent = parent->parent;
    if (parent->parent == NULL) {
      root->root = curr;
    }
    else {
      int val = (parent == LEFT(parent->parent));

      parent->parent->children[val] = curr;
    }
    LEFT(curr) = parent;
    parent->parent = curr;
  }
}

/* Rotate the left child of parent upwards */
static void
rbtree_right_rotate(struct rbtree *root, struct rbtree_node *parent)
{
  struct rbtree_node *curr;

  curr = LEFT(parent);

  if (curr != NULL) {
    LEFT(parent) = RIGHT(curr);
    if (RIGHT(curr) != NULL) {
      RIGHT(curr)->parent = parent;
    }
    curr->parent = parent->parent;
    if (parent->parent == NULL) {
      root->root = curr;
    }
    else {
      int val = (parent != RIGHT(parent->parent));

      parent->parent->children[val] = curr;
    }
    RIGHT(curr) = parent;
    parent->parent = curr;
  }
}

static void
rbtree_insert(struct rbtree *tree, csprof_cct_node_t *node)
{
  struct rbtree_node *curr = tree->root;
  struct rbtree_node *parent, *gparent, *prev = NULL, *x;
  void *ip = node->ip;
  int val;

  /* handle an empty tree */
  if(curr == NULL) {
    tree->root = rbtree_node_create(node, NULL);
    return;
  }

  /* like 'rbtree_search', except that we need a little extra information */
  while(curr != NULL) {
    void *cip = DATA(curr)->ip;
    val = (cip == ip);

    if(val) {
      DIE("Did something wrong before inserting!", __FILE__, __LINE__);
    }
    else {
      prev = curr;
      val = (ip < cip);

      curr = curr->children[val];
    }
  }

  /* didn't contain key */

  if(val) {
    /* came from the left */
    x = curr = LEFT(prev) = rbtree_node_create(node, prev);
  }
  else {
    /* came from the right */
    x = curr = RIGHT(prev) = rbtree_node_create(node, prev);
  }

  /* rebalance */
  while((parent = curr->parent) != NULL
	&& (gparent = parent->parent) != NULL
	&& (IS_RED(curr->parent))) {
    if(parent == LEFT(gparent)) {
      if(RIGHT(gparent) != NULL && IS_RED(RIGHT(gparent))) {
	BLACKEN(parent);
	BLACKEN(RIGHT(gparent));
	REDDEN(gparent);
	curr = gparent;
      }
      else {
	if(curr == RIGHT(parent)) {
	  curr = parent;
	  rbtree_left_rotate(tree, curr);
	  parent = curr->parent;
	}
	BLACKEN(parent);
	if((gparent = parent->parent) != NULL) {
	  REDDEN(gparent);
	  rbtree_right_rotate(tree, gparent);
	}
      }
    }
    /* mirror case */
    else {
      if(LEFT(gparent) != NULL && IS_RED(LEFT(gparent))) {
	BLACKEN(parent);
	BLACKEN(LEFT(gparent));
	REDDEN(gparent);
	curr = gparent;
      }
      else {
	if(curr == LEFT(parent)) {
	  curr = parent;
	  rbtree_right_rotate(tree, curr);
	  parent = curr->parent;
	}
	BLACKEN(parent);
	if((gparent = parent->parent) != NULL) {
	  REDDEN(gparent);
	  rbtree_left_rotate(tree, gparent);
	}
      }
    }
  }

  if(curr->parent == NULL) {
    tree->root = curr;
  }
  BLACKEN(tree->root);
}


//***************************************************************************
//
//***************************************************************************

/* various internal functions for maintaining vdegree trees */

/* csprof_cct_node__init: initializes empty node and links
   it to its parent and siblings (if any) */
static int
csprof_cct_node__init(csprof_cct_node_t* x, csprof_cct_node_t* parent,
		      void *ip, void *sp)
{
  memset(x, 0, sizeof(*x));

  x->ip = ip;
  x->sp = sp;

  /* initial circular list of siblings includes only self */
  x->next_sibling = NULL;
  
  /* link to parent and siblings if any */
  csprof_cct_node__link(x, parent); 
  return CSPROF_OK;
}

static csprof_cct_node_t *
csprof_cct_node__create(lush_assoc_info_t as_info, 
			void* ip,
			lush_lip_t* lip,
			void* sp)
{
  size_t sz = (sizeof(csprof_cct_node_t)
	       + sizeof(size_t)*(csprof_get_max_metrics() - 1));
  csprof_cct_node_t *node = csprof_malloc(sz);

  memset(node, 0, sz);

  node->as_info = as_info; // LUSH
  node->ip = ip;
  node->lip = lip;         // LUSH
  node->sp = sp;

  node->next_sibling = NULL;

  return node;
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

static int
csprof_cct_node__fini(csprof_cct_node_t* x)
{
  return CSPROF_OK;
}

/* csprof_cct_node__link: links a node to a parent and at
   the end of the circular doubly-linked list of its siblings (if any) */
static int
csprof_cct_node__link(csprof_cct_node_t* x, csprof_cct_node_t* parent)
{
  /* Sanity check */
  if (x->parent != NULL) { return CSPROF_ERR; } /* can only have one parent */
  
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
  return CSPROF_OK;
}


// csprof_cct_node__find_child: finds the child of 'x' with
// instruction pointer equal to 'ip'.
//
// tallent: I slightly sanitied the different versions, but FIXME
//
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x,
			    lush_assoc_info_t as_info, void* ip,
			    lush_lip_t* lip,
			    void* sp)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
  // FIXME:LUSH: match assoc and lip
  struct rbtree_node *node = rbtree_search(&x->tree_children, ip, sp);

  return node ? DATA(node) : NULL;
#else
  csprof_cct_node_t* c, *first;
  lush_assoc_t as = lush_assoc_info__get_assoc(as_info);
      
  first = c = csprof_cct_node__first_child(x);
  if (c) {
    do {
      // LUSH
      lush_assoc_t c_as = lush_assoc_info__get_assoc(c->as_info);
      if (c->ip == ip 
	  && c->lip == lip 
	  && lush_assoc_class_eq(c_as, as) 
	  && implies(lush_assoc_info_is_root_note(as_info), 
		     lush_assoc_info_is_root_note(c->as_info))) {
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
csprof_cct__init(csprof_cct_t* x)
{
  memset(x, 0, sizeof(*x));

#ifndef CSPROF_TRAMPOLINE_BACKEND
  {
    unsigned int l;

    /* initialize cached arrays */
    x->cache_len = l = CSPROF_BACKTRACE_CACHE_INIT_SZ;
    x->cache_bt    = csprof_malloc(sizeof(void*) * l);
    x->cache_nodes = csprof_malloc(sizeof(csprof_cct_node_t*) * l);
  }
#endif


  return CSPROF_OK;
}

int
csprof_cct__fini(csprof_cct_t *x)
{
  return CSPROF_OK;
}


// See usage in header.
csprof_cct_node_t*
csprof_cct_insert_backtrace(csprof_cct_t *x, void *treenode, int metric_id,
			    csprof_frame_t *path_beg, csprof_frame_t *path_end,
			    size_t sample_count)
{
#define csprof_MY_ADVANCE_PATH_FRAME(x)   (x)--
#define csprof_MY_IS_PATH_FRAME_AT_END(x) ((x) < path_end)

  MSG(1,"Insert backtrace w x=%lp,tn=%lp,strt=%lp,end=%lp", x, treenode,
      path_beg, path_end);

  csprof_frame_t* frm = path_beg; // current frame 
  csprof_cct_node_t *tn = (csprof_cct_node_t *)treenode;


  if (csprof_cct__isempty(x)) {
    // introduce placeholder root node to prevent forests
    x->tree_root = csprof_cct_node__create(frm->as_info, 0, 0, 0);
    x->num_nodes = 1;
  }

  if (csprof_cct__isempty(x)) {
    // LUSH:FIXME: introduce bogus root to handle possible forests?

    x->tree_root = csprof_cct_node__create(frm->as_info, frm->ip, 
					   frm->lip, frm->sp);
    tn = x->tree_root;
    x->num_nodes = 1;

    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "beg ip %#lx | sp %#lx",
	       frm->ip, frm->sp);
    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "root ip %#lx | sp %#lx",
	       tn->ip, tn->sp);

    csprof_MY_ADVANCE_PATH_FRAME(frm);

    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "nxt beg ip %#lx | sp %#lx",
	       frm->ip, frm->sp);
  }

  if (tn == NULL) {
    tn = x->tree_root;

    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "(NULL) root ip %#lx | sp %#lx",
	       tn->ip, tn->sp);
    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "beg ip %#lx | sp %#lx",
	       frm->ip, frm->sp);

    // tallent: what is this for?
    /* we don't want the tree root calling itself */
    if (frm->ip == tn->ip 
#ifdef CSPROF_TRAMPOLINE_BACKEND
	&& frm->sp == tn->sp
#endif
	) {
      MSG(1,"beg ip == tn ip = %lx",tn->ip);
      csprof_MY_ADVANCE_PATH_FRAME(frm);
    }

    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "beg ip %#lx | sp %#lx",
	       frm->ip, frm->sp);
  }

  while (1) {
    if (csprof_MY_IS_PATH_FRAME_AT_END(frm)) {
      break;
    }

    // Attempt to find a child 'c' corresponding to 'frm'
    MSG(1,"finding child in tree w ip = %lx", frm->ip);

    csprof_cct_node_t *c;
    c = csprof_cct_node__find_child(tn, frm->as_info, frm->ip, frm->lip, 
				    frm->sp);
    if (c) {
      // child exists; recur
      MSG(1,"found child");
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
      MSG(1,"No child found, inserting new tail");
      
      while (!csprof_MY_IS_PATH_FRAME_AT_END(frm)) {
	MSG(1,"create node w ip = %lx",frm->ip);
	c = csprof_cct_node__create(frm->as_info, frm->ip, frm->lip, frm->sp);
	csprof_cct_node__parent_insert(c, tn);
	x->num_nodes++;
	
	tn = c;
	csprof_MY_ADVANCE_PATH_FRAME(frm);
      }
    }
  }

  DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION,
	     "Inserted in %#lx for count %d", x, x->num_nodes);
  tn->metrics[metric_id] += sample_count;
  return tn;
}


//***************************************************************************
//
//***************************************************************************

/* writing trees to streams of various kinds */

static int
hpcfile_cstree_write(FILE* fs, csprof_cct_t* tree, void* root, void* tree_ctxt,
		     hpcfile_uint_t num_metrics,
		     hpcfile_uint_t num_nodes,
                     hpcfile_uint_t epoch);


/* csprof_cct__write_bin: Write the tree 'x' to the file
   stream 'fs'.  The tree is written in HPC_CSTREE format.  Returns
   CSPROF_OK upon success; CSPROF_ERR on error. */
int 
csprof_cct__write_bin(FILE* fs, unsigned int epoch_id,
		      csprof_cct_t* x, lush_cct_ctxt_t* x_ctxt)
{
  int ret;
  if (!fs) { return CSPROF_ERR; }

  // Collect stats on the creation context
  unsigned int x_ctxt_len = lush_cct_ctxt__length(x_ctxt);
  hpcfile_uint_t num_nodes = x_ctxt_len + x->num_nodes;

  ret = hpcfile_cstree_write(fs, x, x->tree_root, x_ctxt,
			     csprof_num_recorded_metrics(),
			     num_nodes, epoch_id);


  // splice creation context out of tree

  return (ret == HPCFILE_OK) ? CSPROF_OK : CSPROF_ERR;
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
hpcfile_cstree_write_node(FILE* fs, csprof_cct_t* tree, csprof_cct_node_t* node, 
			  hpcfile_cstree_node_t *tmp_node,
			  hpcfile_uint_t id_parent,
			  hpcfile_uint_t *id,
			  int levels_to_skip);

static int
hpcfile_cstree_count_nodes(csprof_cct_t* tree, csprof_cct_node_t* node, 
			   int levels_to_skip);

static int
node_child_count(csprof_cct_t* tree, csprof_cct_node_t* node);



// See HPC_CSTREE format details

// hpcfile_cstree_write: Writes all nodes of the tree 'tree' beginning
// at 'root' to file stream 'fs'.  The tree should have 'num_nodes'
// nodes.  The user must supply appropriate callback functions; see
// documentation below for their interfaces.  Returns HPCFILE_OK upon
// success; HPCFILE_ERR on error.
static int
hpcfile_cstree_write(FILE* fs, csprof_cct_t* tree, void* root, void* tree_ctxt,
		     hpcfile_uint_t num_metrics,
		     hpcfile_uint_t num_nodes,
                     hpcfile_uint_t epoch)
{
  int ret;
  int levels_to_skip = 0;

  if (tree_ctxt != 0) {
    levels_to_skip = 2;
#if 0
  } else {
    // this case needs to be coordinated with the context writer. it should skip the same.
    int children = node_child_count(tree, root);
    if (children == 1) levels_to_skip = 1;
#endif
  }
    

  if (!fs) { return HPCFILE_ERR; }

  // -------------------------------------------------------
  // When tree_ctxt is non-null, we will peel off the top
  // node in the tree to eliminate the call frame for
  // monitor_pthread_start_routine
  // -------------------------------------------------------
  
  // -------------------------------------------------------
  // Write header
  // -------------------------------------------------------
  hpcfile_cstree_hdr_t fhdr;

  hpcfile_cstree_hdr__init(&fhdr);
  fhdr.epoch = epoch;
  fhdr.num_nodes = num_nodes;
  if (num_nodes > 0 && levels_to_skip > 0) { // FIXME: better way...
    int skipped = hpcfile_cstree_count_nodes(tree, root, levels_to_skip);
    fhdr.num_nodes -= skipped;
  }
  ret = hpcfile_cstree_hdr__fwrite(&fhdr, fs); 
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }

  // -------------------------------------------------------
  // Write context
  // -------------------------------------------------------
  hpcfile_uint_t id_root = HPCFILE_CSTREE_ID_ROOT;
  unsigned int num_ctxt_nodes = 0;
  lush_cct_ctxt__write(fs, tree_ctxt, id_root, &num_ctxt_nodes);

  // -------------------------------------------------------
  // Write each node, beginning with root
  // -------------------------------------------------------
  hpcfile_uint_t id_ctxt = HPCFILE_CSTREE_ID_ROOT + num_ctxt_nodes - 1;
  hpcfile_uint_t id = id_ctxt + 1;
  hpcfile_cstree_node_t tmp_node;

  hpcfile_cstree_node__init(&tmp_node);
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = malloc(num_metrics * sizeof(hpcfile_uint_t));

  ret = hpcfile_cstree_write_node(fs, tree, root, &tmp_node, id_ctxt, &id,
				  levels_to_skip);
  free(tmp_node.data.metrics);
  
  return ret;
}


static int
hpcfile_cstree_write_node(FILE* fs, csprof_cct_t* tree, csprof_cct_node_t* node, 
			  hpcfile_cstree_node_t *tmp_node,
			  hpcfile_uint_t id_parent,
			  hpcfile_uint_t *id,
			  int levels_to_skip)
{
  hpcfile_uint_t myid;
  csprof_cct_node_t* first, *c;
  int ret;

  if (!node) { return HPCFILE_OK; }

  // ---------------------------------------------------------
  // Write this node
  // ---------------------------------------------------------
  if (levels_to_skip > 0) {
    myid = id_parent;
    levels_to_skip--;
  }
  else {
    tmp_node->id = myid = *id;
    tmp_node->id_parent = id_parent;
    
    // tallent:FIXME: for now I have inlined what was the get_data_fn
    tmp_node->data.as_info = node->as_info.bits;
    tmp_node->data.ip = (hpcfile_vma_t)node->ip;
    tmp_node->data.lip = (hpcfile_vma_t)NULL; // node->lip; // LUSH:FIXME
    tmp_node->data.sp = (hpcfile_uint_t)node->sp;
    memcpy(tmp_node->data.metrics, node->metrics, 
	   tmp_node->data.num_metrics * sizeof(size_t));

    ret = hpcfile_cstree_node__fwrite(tmp_node, fs);
    if (ret != HPCFILE_OK) { 

      return HPCFILE_ERR; 
    }
    
    // Prepare next id -- assigned in pre-order
    (*id)++;
  }
  
  // ---------------------------------------------------------
  // Write children (handles either a circular or non-circular structure)
  // ---------------------------------------------------------
  first = c = csprof_cct_node__first_child(node);
  while (c) {
    ret = hpcfile_cstree_write_node(fs, tree, c, tmp_node, myid, id,
				    levels_to_skip);
    if (ret != HPCFILE_OK) {
      return HPCFILE_ERR;
    }

    c = csprof_cct_node__next_sibling(c);
    if (c == first) { break; }
  }
  
  return HPCFILE_OK;
}


static int
hpcfile_cstree_count_nodes(csprof_cct_t* tree, csprof_cct_node_t* node, 
			   int levels_to_skip)
{
  int skipped_subtree_count = 0;

  if (node) { 
    if (levels_to_skip-- > 0) {
      skipped_subtree_count++; // count self

      // ---------------------------------------------------------
      // count skipped children 
      // (handles either a circular or non-circular structure)
      // ---------------------------------------------------------
      int kids = 0;
      csprof_cct_node_t* first = csprof_cct_node__first_child(node);
      csprof_cct_node_t* c = first; 
      while (c) {
	kids += hpcfile_cstree_count_nodes(tree, c, levels_to_skip);
	c = csprof_cct_node__next_sibling(c);
	if (c == first) { break; }
      }
      skipped_subtree_count += kids; 
    }
  }
  return skipped_subtree_count; 
}


static int
node_child_count(csprof_cct_t* tree, csprof_cct_node_t* node)
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


//***************************************************************************
//
//***************************************************************************

static int
lush_cct_ctxt__write_gbl(FILE* fs, lush_cct_ctxt_t* cct_ctxt,
			 unsigned int id_root, unsigned int* nodes_written,
			 hpcfile_cstree_node_t* tmp_node);

static int
lush_cct_ctxt__write_lcl(FILE* fs, csprof_cct_node_t* node,
			 unsigned int id_root, unsigned int* nodes_written,
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
lush_cct_ctxt__write(FILE* fs, lush_cct_ctxt_t* cct_ctxt,
		     unsigned int id_root, unsigned int* nodes_written)
{
  // N.B.: assumes that calling malloc is acceptible!

  int ret;
  unsigned int num_metrics = csprof_num_recorded_metrics();

  hpcfile_cstree_node_t tmp_node;
  hpcfile_cstree_node__init(&tmp_node);
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = malloc(num_metrics * sizeof(hpcfile_uint_t));
    
  ret = lush_cct_ctxt__write_gbl(fs, cct_ctxt, id_root,
				 nodes_written, &tmp_node);

  free(tmp_node.data.metrics);

  return ret;
}


// Post order write of 'cct_ctxt'
static int
lush_cct_ctxt__write_gbl(FILE* fs, lush_cct_ctxt_t* cct_ctxt,
			 unsigned int id_root, unsigned int* nodes_written,
			 hpcfile_cstree_node_t* tmp_node)
{
  int ret = CSPROF_OK;
  
  // Base case
  if (!cct_ctxt) {
    return CSPROF_OK;
  }

  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = lush_cct_ctxt__write_gbl(fs, cct_ctxt->parent, id_root,
				 nodes_written, tmp_node);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  
  // write this context
  unsigned int lcl_written = 0;
  unsigned int id_lcl_root = id_root + (*nodes_written);
  ret = lush_cct_ctxt__write_lcl(fs, cct_ctxt->context, 
				 id_lcl_root, &lcl_written, tmp_node);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  (*nodes_written) += lcl_written;
  
  return ret;
}


// Post order write of 'node'
static int
lush_cct_ctxt__write_lcl(FILE* fs, csprof_cct_node_t* node,
			 unsigned int id_root, unsigned int* nodes_written,
			 hpcfile_cstree_node_t* tmp_node)
{
  int ret = CSPROF_OK;

  // Base case
  if (!node) {
    return CSPROF_OK;
  }
  
  // -------------------------------------------------------
  // General case (post order)
  // -------------------------------------------------------
  ret = lush_cct_ctxt__write_lcl(fs, node->parent, id_root,
				 nodes_written, tmp_node);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  
  // write this node
  tmp_node->id        = id_root + (*nodes_written);
  tmp_node->id_parent = tmp_node->id - 1;

  // tallent:FIXME: for now I have inlined what was the get_data_fn
  tmp_node->data.as_info = node->as_info.bits;
  tmp_node->data.ip = (hpcfile_vma_t)node->ip;
  tmp_node->data.lip = (hpcfile_vma_t)NULL; // node->lip; // LUSH:FIXME
  tmp_node->data.sp = (hpcfile_uint_t)node->sp;
  memcpy(tmp_node->data.metrics, node->metrics, 
	 tmp_node->data.num_metrics * sizeof(size_t));
  
  ret = hpcfile_cstree_node__fwrite(tmp_node, fs);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  (*nodes_written)++;
  
  return ret;
}



