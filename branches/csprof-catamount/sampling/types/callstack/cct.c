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
static void  
csprof_cct__get_data_CB(csprof_cct_t* x, csprof_cct_node_t* node, 
			hpcfile_cstree_nodedata_t* d);
static void* 
csprof_cct__get_first_child_CB(csprof_cct_t* x, csprof_cct_node_t* node);
static void*
csprof_cct__get_sibling_CB(csprof_cct_t* x, csprof_cct_node_t* node);

#ifdef CSPROF_TRAMPOLINE_BACKEND
static csprof_cct_node_t *
csprof_cct_node__find_child(csprof_cct_node_t*, void*, void*);
#else
static csprof_cct_node_t *
csprof_cct_node__find_child(csprof_cct_node_t*, void*);
#endif

static int
csprof_cct_node__link(csprof_cct_node_t *, csprof_cct_node_t *);

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
#define SET_COLOR(node, c) \
{ \
  csprof_cct_node_t *p = (node)->data; \
  unsigned long pl = (unsigned long) p; \
  \
  (node)->data = (csprof_cct_node_t *)((pl & ~MASK) | (c)); \
}
#define REDDEN(node) SET_COLOR((node), RED)
#define BLACKEN(node) SET_COLOR((node), BLACK)
#define IS_RED(node) (COLOR((node)))

#define DATA(node) ((csprof_cct_node_t *)((unsigned long)((node)->data) & ~MASK))
#define SET_DATA(node, d) \
{ \
  unsigned long c = COLOR(node); \
  \
  node->data = d; \
  SET_COLOR(node, c); \
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
#ifdef CSPROF_TRAMPOLINE_BACKEND
    x->sp = sp;
#else
    x->sp = NULL;
#endif

    /* initial circular list of siblings includes only self */
    x->next_sibling = NULL;
  
    /* link to parent and siblings if any */
    csprof_cct_node__link(x, parent); 
    return CSPROF_OK;
}

static csprof_cct_node_t *
csprof_cct_node__create(void *ip, void *sp)
{
    size_t node_size = sizeof(csprof_cct_node_t)
	+ sizeof(size_t)*(csprof_get_max_metrics() - 1);
    csprof_cct_node_t *node = csprof_malloc(node_size);

    memset(node, 0, node_size);

    node->ip = ip;
#ifdef CSPROF_TRAMPOLINE_BACKEND
    node->sp = sp;
#else
    node->sp = NULL;
#endif
    node->next_sibling = NULL;

    return node;
}

static void
csprof_cct_node__parent_insert(csprof_cct_node_t *x, csprof_cct_node_t *parent)
{
    csprof_cct_node__link(x, parent);
    rbtree_insert(&parent->tree_children, x);
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

/* csprof_cct_node__find_child: finds the child of 'x' with
   instruction pointer equal to 'ip'.

   we have different versions because the trampoline-based backends
   need to worry about stack pointers and such.  this should be FIXME. */

#ifdef CSPROF_TRAMPOLINE_BACKEND
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x, void* ip, void* sp)
{
#if 1
    struct rbtree_node *node = rbtree_search(&x->tree_children, ip, sp);

    return node ? DATA(node) : NULL;
#else
  csprof_cct_node_t* c, *first;

  first = c = csprof_cct_node__first_child(x);
  if (c) {
    do {
        void *cip = c->ip;
        if(cip == ip) { return c; }
	
      c = csprof_cct_node__next_sibling(c);
    } while (c != NULL);
  }

  return NULL;
#endif
}
#else
static csprof_cct_node_t*
csprof_cct_node__find_child(csprof_cct_node_t* x, void* ip)
{
    csprof_cct_node_t* c, *first;

    first = c = csprof_cct_node__first_child(x);
    if (c) {
        do {
            if (c->ip == ip) { return c; }

            c = csprof_cct_node__next_sibling(c);
        } while (c != NULL);
    }

    return NULL;
}
#endif


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

csprof_cct_node_t *
csprof_cct_insert_backtrace(csprof_cct_t *x, void *treenode, int metric_id,
			    csprof_frame_t *start, csprof_frame_t *end,
			    size_t sample_count)
{
    csprof_cct_node_t *tn = (csprof_cct_node_t *)treenode;

    MSG(1,"Insert backtrace w x=%lp,tn=%lp,strt=%lp,end=%lp",x,treenode,start,end);
    if(csprof_cct__isempty(x)) {
      MSG(1,"x is empty");
        x->tree_root = csprof_cct_node__create(start->ip, start->sp);

        tn = x->tree_root;
        x->num_nodes = 1;

        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "--start ip %#lx | sp %#lx",
                   start->ip, start->sp);
        start--;

#ifdef CSPROF_TRAMPOLINE_BACKEND
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "root ip %#lx | sp %#lx",
                   tn->ip, tn->sp);
#else
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "root ip %#lx",
                   tn->ip);
#endif
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "nxt start ip %#lx | sp %#lx",
                   start->ip, start->sp);
    }

    if(tn == NULL) {
        tn = x->tree_root;

#ifdef CSPROF_TRAMPOLINE_BACKEND
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "(NULL) root ip %#lx | sp %#lx",
                   tn->ip, tn->sp);
#else
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "(NULL) root ip %#lx",
                   tn->ip);
#endif
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "start ip %#lx | sp %#lx",
                   start->ip, start->sp);

        /* we don't want the tree root calling itself */
#ifdef CSPROF_TRAMPOLINE_BACKEND
        if(start->ip == tn->ip && start->sp == tn->sp) {
            start--;
        }
#else
	if(start->ip == tn->ip) {
          MSG(1,"start ip == tn ip = %lx",tn->ip);
	  start--;
	}
#endif
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "start ip %#lx | sp %#lx",
                   start->ip, start->sp);
    }

    while(1) {
        if(start < end) {
            /* done */
            break;
        }
        else {
            /* find child */
#ifdef CSPROF_TRAMPOLINE_BACKEND
            csprof_cct_node_t *c =
                csprof_cct_node__find_child(tn, start->ip, start->sp);
#else
            MSG(1,"finding child in tree w ip = %lx",start->ip);
	    csprof_cct_node_t *c =
                csprof_cct_node__find_child(tn, start->ip);
#endif

            if(c) {
                /* child exists; recur */
              MSG(1,"found child");
                tn = c;
                start--;
            }
            else {
                /* no such child; insert new tail */
              MSG(1,"No child found, inserting new tail");
                while(start >= end) {
                  MSG(1,"create node w ip = %lx",start->ip);
                    c = csprof_cct_node__create(start->ip, start->sp);
                    csprof_cct_node__parent_insert(c, tn);
                    x->num_nodes++;

                    tn = c;
                    start--;
                }
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
			     num_nodes, epoch_id,
			     (hpcfile_cstree_cb__write_context_fn_t)
			       lush_cct_ctxt__write,
			     (hpcfile_cstree_cb__get_data_fn_t)
			       csprof_cct__get_data_CB,
			     (hpcfile_cstree_cb__get_first_child_fn_t)
			       csprof_cct__get_first_child_CB,
			     (hpcfile_cstree_cb__get_sibling_fn_t)
			       csprof_cct__get_sibling_CB);


  // splice creation context out of tree

  return (ret == HPCFILE_OK) ? CSPROF_OK : CSPROF_ERR;
}




/* cstree callbacks (CB) */
static void  
csprof_cct__get_data_CB(csprof_cct_t* x, csprof_cct_node_t* node,
			hpcfile_cstree_nodedata_t* d)
{
    d->ip = (hpcfile_vma_t)node->ip;
#ifdef CSPROF_TRAMPOLINE_BACKEND
    d->sp = (hpcfile_uint_t)node->sp;
#else
    d->sp = (hpcfile_uint_t)NULL;
#endif
    memcpy(d->metrics, node->metrics, d->num_metrics * sizeof(size_t));
}

static void* 
csprof_cct__get_first_child_CB(csprof_cct_t* x, csprof_cct_node_t* node)
{
  return csprof_cct_node__first_child(node);
}

static void*
csprof_cct__get_sibling_CB(csprof_cct_t* x, csprof_cct_node_t* node)
{
  return csprof_cct_node__next_sibling(node);
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
    return ret;
  }


  // General case
  ret = lush_cct_ctxt__write_gbl(fs, cct_ctxt->parent, id_root, 
				 nodes_written, tmp_node);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  
  unsigned int lcl_written = 0;
  unsigned int id_lcl_root = *nodes_written;
  ret = lush_cct_ctxt__write_lcl(fs, cct_ctxt->context, id_lcl_root, 
				 &lcl_written, tmp_node);
  if (ret != CSPROF_OK) { return CSPROF_ERR; }
  nodes_written += lcl_written;
  
  return ret;
}


// Post order write of 'node'
static int
lush_cct_ctxt__write_lcl(FILE* fs, csprof_cct_node_t* node,
			 unsigned int id_root, unsigned int* nodes_written,
			 hpcfile_cstree_node_t* tmp_node)
{
  int ret;

  // Base case
  if (!node) {
    return CSPROF_OK;
  }
  

  // General case
  lush_cct_ctxt__write_lcl(fs, node->parent, id_root,
			   nodes_written, tmp_node);
  
  // write a node
  unsigned int id_parent = *nodes_written;
  unsigned int id        = id_parent + 1;
  
  tmp_node->id = id;
  tmp_node->id_parent = id_parent;
  csprof_cct__get_data_CB(NULL, node, &(tmp_node->data));
  
  ret = hpcfile_cstree_node__fwrite(tmp_node, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR;
  }
  
  (*nodes_written)++;
  
  return ret;
}


#if 0
csprof_cct_node_t* 
xxx_copy_list(FILE* fs, csprof_cct_node_t* node, &new_tail)
{
  if (!node) {
    *new_tail = NULL;
    return NULL;
  }

  csprof_cct_node_t* cp_head = xxx_copy(node);
  csprof_cct_node_t* cp_tail = cp_head;

  for (csprof_cct_node_t* x = node->parent; (x); x = x->parent) {
    csprof_cct_node_t* tmp = xxx_copy(x);
    cp_tail->parent = tmp;
    cp_tail = tmp;
  }
  
  *new_tail = cp_tail;
  return cp_head;
}
#endif

