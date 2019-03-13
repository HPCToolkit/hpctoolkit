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
// Copyright ((c)) 2002-2019, Rice University
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



//******************************************************************************
//
// File: cskiplist_abstract.c
//   $HeadURL$
//
// Purpose:
//   Implement the API for a concurrent skip list as specified in
// cskiplist_abstract.h
//
//******************************************************************************

//******************************************************************************
// global includes
//******************************************************************************


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "randomizer.h"
#include "cskiplist.h"

//******************************************************************************
// macros
//******************************************************************************

#ifndef NULL
#define NULL 0
#endif

#define CSKL_DEBUG 0

#define NO_LAYER -1

// number of bytes in a skip list node with L levels
#define SIZEOF_CSKLNODE_T(L) (sizeof(csklnode_t) + sizeof(void*) * L)

#define NUM_NODES 10


//******************************************************************************
// implementation types
//******************************************************************************

#if OPAQUE_TYPE
// opaque type not supported by gcc 4.4.*
#include "cskiplist_defs.h"
#endif


typedef enum {
  cskiplist_find_early_exit,
  cskiplist_find_full
} cskiplist_find_type;


static csklnode_t *GF_cskl_nodes = NULL; // global free csklnode list
static mcs_lock_t GFCN_lock;  // lock for GF_cskl_nodes
static __thread  csklnode_t *_lf_cskl_nodes = NULL;  // thread local free csklnode list


//******************************************************************************
// private operations
//******************************************************************************

/*
 * allocate a node of a specified height.
 */
static csklnode_t *
csklnode_alloc_node(int height, mem_alloc m_alloc)
{
  csklnode_t* node = (csklnode_t*)m_alloc(SIZEOF_CSKLNODE_T(height));
  node->height = height;
  node->fully_linked = false;
  node->marked = false;
  mcs_init(&(node->lock));
  return node;
}

/*
 * add a bunch of nodes to _lf_cskl_nodes
 */
static void
csklnode_add_nodes_to_lfl(
	int maxheight,
	mem_alloc m_alloc)
{
  for (int i = 0; i < NUM_NODES; i++) {
	int my_height  = random_level(maxheight);
	csklnode_t* node = csklnode_alloc_node(my_height, m_alloc);
	node->nexts[0] = _lf_cskl_nodes;
	_lf_cskl_nodes = node;
  }
}


/*
 * remove and return the head of _lf_cskl_nodes
 * pre-condtion: _lf_cskl_nodes != NULL
 */
static csklnode_t*
csklnode_alloc_from_lfl(
	void* val)
{
  csklnode_t *ans = _lf_cskl_nodes;
  _lf_cskl_nodes = _lf_cskl_nodes->nexts[0];
  ans->nexts[0] = NULL;
  ans->val = val;
  return ans;
}

/*
 * return node to lfl
 */
static void
csklnode_free_to_lfl(csklnode_t *node)
{
  node->nexts[0] = _lf_cskl_nodes;
  _lf_cskl_nodes = node;
}


/*
 * trylock GF_cskl_nodes;
 * if GF_cskl_nodes != NULL, transfer a bunch of nodes to _lf_cskl_nodes,
 * otherwise add a few nodes to _lf_cskl_nodes.
 */
static void
csklnode_populate_lfl(
	int maxheight,
	mem_alloc m_alloc)
{
  mcs_node_t me;
  bool acquired = mcs_trylock(&GFCN_lock, &me);
  if (acquired) {
	if (GF_cskl_nodes) {
	  // transfer a bunch of nodes the global free list to the local free list
	  int n = 0;
	  while (GF_cskl_nodes && n < NUM_NODES) {
		csklnode_t *head = GF_cskl_nodes;
		GF_cskl_nodes = GF_cskl_nodes->nexts[0];
		head->nexts[0] = _lf_cskl_nodes;
		_lf_cskl_nodes = head;
		n++;
	  }
	}
	mcs_unlock(&GFCN_lock, &me);
  }
  if (!_lf_cskl_nodes) {
	csklnode_add_nodes_to_lfl(maxheight, m_alloc);
  }
}

static csklnode_t*
csklnode_malloc(
	void* val,
	int maxheight,
	mem_alloc m_alloc)
{
  if (!_lf_cskl_nodes) {
	csklnode_populate_lfl(maxheight, m_alloc);
  }

#if CSKL_DEBUG
  assert(_lf_cskl_nodes);
#endif

  return csklnode_alloc_from_lfl(val);
}

static inline bool
cskiplist_node_is_removable(csklnode_t *candidate, int layer)
{
  return (candidate->fully_linked &&
	  candidate->height - 1 == layer &&
	  !candidate->marked);
}

/**
 * from Herlihy, 2006.
 * pre-conditions:
 * preds[] and succs[] are arrays of csklnode_t* of length cskl->max_height.
 *
 * post-conditions:
 *   if val is in cskl, return the first layer where val is found,
 *     otherwise return NO_LAYER.
 *
 *   preds[i]->val < val <= succs[i] for i = low..cskl->max_height-1, where
 *     low = (ft == cskiplist_find_full)? 0: found_layer, in other words,
 *   preds[i] is the node that precedes the found node at level i
 *   succs[i] is the node that follows the found node at level i.
 *
 *   if val is found,
 *     val == succs[cskiplist_find_helper(compare, val, preds, succs, ft)]
 */
static int
cskiplist_find_helper
(val_cmp compare,   // comparator used for searching
	int max_height,		// number of list levels
	csklnode_t *pred,	// full node < val
	void* val,            // the value we are seeking
	csklnode_t *preds[],    // return value: nodes < val
	csklnode_t *succs[],    // return value: nodes >= val
	cskiplist_find_type ft) // is info needed at all layers?
{
  int found_layer = NO_LAYER;
  for (int layer = max_height-1; layer >= 0; layer--) {
	csklnode_t *current = pred->nexts[layer];

	// look along a layer until we find a value that is not to the left
	// of our value of interest
	while (compare(current->val, val) < 0) { //  i.e. current->val < val
	  pred = current; // invariant pred->val < val is maintained
	  current = pred->nexts[layer];
	}
	// loop exit condition: val <= curr->val
	// loop invariant: pred->val < val

	// remember the 'layer' node < val:
	preds[layer] = pred; // preds[layer]->val < val, by the loop invariant
	// remember the 'layer' node >= val:
	succs[layer] = current; // val <= succs[layer]->val, by the loop exit condition

	if (found_layer == NO_LAYER &&    // if we haven't already found a match
		compare(current->val, val) == 0) { // and the current val is a match
	  found_layer = layer;            // remember the layer where we matched
	  // found_layer is the first layer from the top where val is found.
	  // some clients of find don't need results for layers below found_layer
	  if (ft == cskiplist_find_early_exit)
	    break;
	}
  }

  return found_layer;
}

static csklnode_t *
cskiplist_find(val_cmp compare, cskiplist_t *cskl, void* value)
{
  // Acquire lock before reading:
  pfq_rwlock_read_lock(&cskl->lock);

  int     max_height  = cskl->max_height;
  csklnode_t *preds[max_height];
  csklnode_t *succs[max_height];
  int layer = cskiplist_find_helper(compare, max_height, cskl->left_sentinel, value, preds,
	  succs, cskiplist_find_early_exit);
  csklnode_t *node = NULL;
  if (layer != NO_LAYER) {
	node = succs[layer];
	if (!node->fully_linked || node->marked)
	  node = NULL;
  }

  // Release lock after reading:
  pfq_rwlock_read_unlock(&cskl->lock);

  return node;
}

static void inline
unlock_preds
(csklnode_t *preds[],
	mcs_node_t mcs_nodes[],
	int highestLocked)
{
  // unlock each of my predecessors, as necessary
  csklnode_t *prevPred = NULL;
  for (int layer = 0; layer <= highestLocked; layer++) {
	csklnode_t *pred = preds[layer];
	if (pred != prevPred) {
	  mcs_unlock(&pred->lock, &mcs_nodes[layer]);
	}
	prevPred = pred;
  }
}

static void
cskl_links_tostr(int max_height, char str[], int max_cskl_str_len)
{
  int str_len;
  str[0] = '\0';
  for (int i = 0; i < max_height; i++) {
	str_len = strlen(str);
    strncat(str, " |", max_cskl_str_len - str_len - 1);
  }
  str_len = strlen(str);
  strncat(str, "\n", max_cskl_str_len - str_len - 1);
}

static bool
cskl_del_bulk_unsynch(val_cmp cmpfn, cskiplist_t *cskl, void *lo, void *hi, mem_free m_free)
{
  int max_height = cskl->max_height;

  csklnode_t* lpreds[max_height];
  csklnode_t* other[max_height];
  csklnode_t* hsuccs[max_height];
  csklnode_t** succs;

  // Acquire lock before writing:
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&cskl->lock, &me);

  //----------------------------------------------------------------------------
  // Look for a node matching low
  //----------------------------------------------------------------------------
  int hlayer = cskiplist_find_helper(cmpfn, max_height, cskl->left_sentinel, lo, lpreds,
				     other, cskiplist_find_full);
  csklnode_t *first = lpreds[0]->nexts[0];
  if (lo == hi)
    succs = lpreds;
  else {
    //----------------------------------------------------------------------------
    // Look for a node matching high
    //----------------------------------------------------------------------------
    succs = &hsuccs[0];
    hlayer = cskiplist_find_helper(cmpfn, max_height, lpreds[max_height-1],
				   hi, succs, other, cskiplist_find_full);

    //
    //----------------------------------------------------------------------------
    // Complete the deletion: Splice out node at all layers from 0..max_height-1
    //----------------------------------------------------------------------------
    int layer = max_height - 1;
    while (layer > hlayer) {
      // Splice out nodes at layer.
      lpreds[layer]->nexts[layer] = succs[layer]->nexts[layer];
      layer--;
    }
  }
  
  while (hlayer >= 0) {
	// Splice out nodes at layer, including found node.
	lpreds[hlayer]->nexts[hlayer] = succs[hlayer]->nexts[hlayer]->nexts[hlayer];
	hlayer--;
  }
  csklnode_t *last = lpreds[0]->nexts[0];

  //----------------------------------------------------------------------------
  // Delete all of the nodes between first and last.
  //----------------------------------------------------------------------------

  for (csklnode_t *node = first; node != last;) {
	csklnode_t *next = node->nexts[0]; // remember the pointer before trashing node
	m_free(node); // delete node
	node = next;
  }

  // Release lock after writing:
  pfq_rwlock_write_unlock(&cskl->lock, &me);

  return first != last;
}

//******************************************************************************
// interface operations
//******************************************************************************

void
cskl_init()
{
#if CSKL_DEBUG
  printf("DXN_DBG: cskl_init  mcs_init(&GFCN_lock)\n");
#endif
  mcs_init(&GFCN_lock);
}

/*
 * only free non null csklnode_t*
 * pre-condition: anode is of type csklnpde_t*
 */
void
cskl_free(void* anode)
{
  if (!anode) return;
  // add node to the front of the global free csklnode list.
  mcs_node_t me;
  csklnode_t *node = (csklnode_t*)anode;
  for (int i = 0; i < node->height; i++) {
	node->nexts[i] = NULL;
  }
  node->fully_linked = false;
  node->marked = false;
  mcs_lock(&GFCN_lock, &me);
  node->nexts[0] = GF_cskl_nodes;
  GF_cskl_nodes = node;
  mcs_unlock(&GFCN_lock, &me);
}


cskiplist_t*
cskl_new(
	void* lsentinel,
	void* rsentinel,
	int max_height,
	val_cmp compare,
	val_cmp inrange,
	mem_alloc m_alloc)
{
  cskiplist_t* cskl = (cskiplist_t*)m_alloc(sizeof(cskiplist_t));
  cskl->max_height = max_height;
  cskl->compare = compare;
  cskl->inrange = inrange;
  pfq_rwlock_init(&cskl->lock);

  // create sentinel nodes
  csklnode_t *left = cskl->left_sentinel   = csklnode_alloc_node(max_height, m_alloc);
  csklnode_t *right = cskl->right_sentinel = csklnode_alloc_node(0, m_alloc);
  left->val = lsentinel;
  right->val = rsentinel;
  // hook sentinel nodes in empty list
  for(int i = 0; i < max_height; i++)
    left->nexts[i] = right;

  return cskl;
}

void*
cskl_cmp_find(cskiplist_t *cskl, void *value)
{
  csklnode_t *found = cskiplist_find(cskl->compare, cskl, value);
  return found? found->val: NULL;  // found->val == value
}

void*
cskl_inrange_find(cskiplist_t *cskl, void *value)
{
  csklnode_t *found = cskiplist_find(cskl->inrange, cskl, value);
  return found? found->val: NULL;  // found->val contains value
}


csklnode_t *
cskl_insert(cskiplist_t *cskl, void *value,
	mem_alloc m_alloc)
{
  int max_height = cskl->max_height;
  csklnode_t *preds[max_height];
  csklnode_t *succs[max_height];
  csklnode_t *node;

  // allocate my node
  csklnode_t *new_node = csklnode_malloc(value, max_height, m_alloc);

  for (node = NULL; node == NULL; pfq_rwlock_read_unlock(&cskl->lock)) {
	// Acquire lock before reading:
	pfq_rwlock_read_lock(&cskl->lock);

	int found_layer= cskiplist_find_helper(cskl->compare,
		max_height, cskl->left_sentinel, value, preds, succs,
		cskiplist_find_full);

	if (found_layer != NO_LAYER) {
	  node = succs[found_layer];
	  if (!node->marked) {
	  	csklnode_free_to_lfl(new_node);
		while (!node->fully_linked);
	  } else
	    // node is marked for deletion by some other thread, so this thread needs
	    // to try to insert again by going to the end of this for(;;) loop. As
	    // this thread tries to insert again, there may be another the thread
	    // that succeeds in inserting value before this thread gets a chance to.
	    node = NULL;
	  continue;
	}


	//--------------------------------------------------------------------------
	// Acquire a lock on node's predecessor at each layer.
	//
	// valid condition:
	//   no predecessor is marked
	//   no successor is marked
	//   each of my predecessors still points to the successor I recorded
	//
	// If the valid condition is not satisfied, unlock all of my predecessors
	// and retry.
	//--------------------------------------------------------------------------
	int my_height = new_node->height;
	mcs_node_t mcs_nodes[my_height];
	int highestLocked = -1;
	int layer;
	csklnode_t *prevPred = NULL;
	for (layer = 0; layer < my_height; layer++) {
	  csklnode_t *pred = preds[layer];
	  csklnode_t *succ = succs[layer];
	  if (pred != prevPred) {
		mcs_lock(&pred->lock, &mcs_nodes[layer]);
		highestLocked = layer;
		prevPred = pred;
	  }
	  if (pred->marked || succ->marked || pred->nexts[layer] != succ)
	    break;
	  // my value better be less than the node I am inserting before
	  assert(cskl->compare(value, pred->nexts[layer]->val) == -1); 
	}
	if (layer == my_height) {
	  // link new node in at levels [0 .. my_height-1]
	  node = new_node;
	  for (int layer = 0; layer < my_height; layer++) {
	    node->nexts[layer] = succs[layer];
	    preds[layer]->nexts[layer] = node;
	  }

	  // mark the node as usable
	  node->fully_linked = true;
	}

	// unlock each of my predecessors, as necessary
	unlock_preds(preds, mcs_nodes, highestLocked);
  }

  return node;
}

/*
 * TODO: this function is not used anywhere and leaks memory.
 */
bool
cskl_delete(cskiplist_t *cskl, void *value)
{
  int max_height = cskl->max_height;
  bool node_is_marked = false;
  csklnode_t     *node   = NULL;

  for (;;) {
	csklnode_t     *preds[max_height], *succs[max_height];
	int         height = -1;

	//--------------------------------------------------------------------------
	// Look for a node matching v
	//--------------------------------------------------------------------------
	int layer = cskiplist_find_helper(cskl->compare, max_height, cskl->left_sentinel, value,
		preds, succs, cskiplist_find_full);

	//--------------------------------------------------------------------------
	// A matching node is available to remove. We may have marked it earlier as
	// we began its removal.
	//--------------------------------------------------------------------------
	if (node_is_marked ||
		(layer != NO_LAYER &&
			cskiplist_node_is_removable(succs[layer], layer))) {

	  //------------------------------------------------------------------------
	  // if I haven't already marked this node for deletion, do so.
	  //------------------------------------------------------------------------
	  mcs_node_t  mcs_nodes[layer + 1];
	  if (!node_is_marked) {
		node = succs[layer];
		height = node->height;
		mcs_lock(&node->lock, &mcs_nodes[layer]);
		if (node->marked) {
		  mcs_unlock(&node->lock, &mcs_nodes[layer]);
		  return false;
		}
		node->marked = true;
		node_is_marked = true;
	  }
	  //------------------------------------------------------------------------
	  // Post-condition: Node is marked and locked.
	  //------------------------------------------------------------------------

	  //------------------------------------------------------------------------
	  // Acquire a lock on node's predecessor at each layer.
	  //
	  // valid condition:
	  //   no predecessor is marked
	  //   each of my predecessors still points to the successor I recorded (me)
	  //
	  // If the valid condition is not satisfied, unlock all of my predecessors
	  // and retry.
	  //------------------------------------------------------------------------
	  int highestLocked = -1;
	  csklnode_t *pred, *succ;
	  csklnode_t *prevPred = NULL;
	  bool valid = true;
	  for (int layer = 0; valid && (layer < height); layer++) {
		pred = preds[layer];
		succ = succs[layer];
		if (pred != prevPred) {
		  mcs_lock(&pred->lock, &mcs_nodes[layer]);
		  highestLocked = layer;
		  prevPred = pred;
		}
		valid = !pred->marked && pred->nexts[layer] == succ;
	  }

	  if (!valid) {
		unlock_preds(preds, mcs_nodes, highestLocked);
		continue;
	  }
	  //-----------------------------------------------------------------------
	  // Post-condition: All predecessors are locked for delete.
	  //-----------------------------------------------------------------------

	  //-----------------------------------------------------------------------
	  // Complete the deletion: Splice out node at each layer and release lock
	  // on node's predecessor at each layer.
	  //-----------------------------------------------------------------------
	  for (int layer = height - 1; layer >= 0; layer--) {
		// Splice out node at layer.
		preds[layer]->nexts[layer] = node->nexts[layer];
	  }

	  // Unlock self.
	  mcs_unlock(&node->lock, &mcs_nodes[layer]);

	  unlock_preds(preds, mcs_nodes, highestLocked);
	  return true;
	} else {
	  return false;
	}
  }
}

bool
cskl_cmp_del_bulk_unsynch(cskiplist_t *cskl, void *lo, void *hi, mem_free m_free)
{
  return cskl_del_bulk_unsynch(cskl->compare, cskl, lo, hi, m_free);
}

bool
cskl_inrange_del_bulk_unsynch(cskiplist_t *cskl, void *lo, void *hi, mem_free m_free)
{
  return cskl_del_bulk_unsynch(cskl->inrange, cskl, lo, hi, m_free);
}


void cskl_levels_tostr (int height, int max_height, char str[],
	int max_cskl_str_len)
{
  int str_len;
  str[0] = '\0';
  strcat(str, " +");
  for (int i = 1; i < height; i++) {
	str_len = strlen(str);
    strncat(str, "-+", max_cskl_str_len - str_len - 1);
  }
  for (int i = height; i < max_height; i++) {
	str_len = strlen(str);
    strncat(str, " |", max_cskl_str_len - str_len - 1);
  }
  str_len = strlen(str);
  strncat(str, "  ", max_cskl_str_len - str_len - 1);
}

void
cskl_append_node_str(char nodestr[], char str[], int max_cskl_str_len)
{
  int str_len = strlen(str);
  strncat(str, nodestr, max_cskl_str_len - str_len - 1);
  str_len = strlen(str);
  strncat(str, "\n", max_cskl_str_len - str_len - 1);
}

void
cskl_tostr(cskiplist_t *cskl, cskl_node_tostr node_tostr, char csklstr[], int max_cskl_str_len)
{
  // Acquire lock before reading the cskiplist to build its string representation:
  pfq_rwlock_read_lock(&cskl->lock);

  int max_height = cskl->max_height;
  csklnode_t *node = cskl->left_sentinel;
  csklnode_t *right = cskl->right_sentinel;

  int temp_len = max_cskl_str_len/2;
  char temp_buff[temp_len];
  int str_len = 0;

  for (; ; node = node->nexts[0]) {
    cskl_links_tostr(max_height, temp_buff, temp_len - 1);
    strncat(csklstr, temp_buff, max_cskl_str_len - str_len - 1);
    str_len = strlen(csklstr);
    cskl_levels_tostr(node->height, max_height, csklstr + str_len, max_cskl_str_len - str_len - 1);
    str_len = strlen(csklstr);
    node_tostr(node->val, node->height, max_height, temp_buff, temp_len - 1);  // abstract function
    strncat(csklstr, temp_buff, max_cskl_str_len - str_len - 1);
    str_len = strlen(csklstr);
    if (node == right) break;
  }
  strncat(csklstr, "\n", max_cskl_str_len - str_len - 1);

  // Release lock after building the string representation:
  pfq_rwlock_read_unlock(&cskl->lock);

}

static void
cskl_links_dump(csklnode_t *node, int max_height)
{
  int i;
  int height = node->height;

  printf("0x%016lx: ", (unsigned long) node);

  // links [0..height)
  for (i = 0; i < height; i++) {
    printf("0x%016lx ", (unsigned long) node->nexts[i]);
  } 

  // links not present [height..max_height)
  for (i = height; i < max_height; i++) {
    printf("------------------ ");
  }
}


#define NODE_STR_LEN 4096

void
cskl_dump(cskiplist_t *cskl, cskl_node_tostr node_tostr)
{
  pfq_rwlock_read_lock(&cskl->lock);

  int max_height = cskl->max_height;
  csklnode_t *node = cskl->left_sentinel;
  csklnode_t *right = cskl->right_sentinel;

  char nodestr[NODE_STR_LEN];

  for (; ; node = node->nexts[0]) {
    cskl_links_dump(node, max_height);
    node_tostr(node->val, node->height, max_height, nodestr, NODE_STR_LEN -1);
    printf("%s", nodestr);
    if (node == right) break;
  }
  printf("\n");

  pfq_rwlock_read_unlock(&cskl->lock);
}


static void
cskl_links_print(csklnode_t *node, int max_height)
{
  int i;
  int height = node->height;

  printf("0x%016lx: ", (unsigned long) node);

  printf(" +");
  // links [1..height)
  for (i = 1; i < height; i++) {
    printf("-+");
  } 

  // links not present [height..max_height)
  for (i = height; i < max_height; i++) {
    printf(" |");
  }
}


void
cskl_print(cskiplist_t *cskl, cskl_node_tostr node_tostr)
{
  pfq_rwlock_read_lock(&cskl->lock);

  int max_height = cskl->max_height;
  csklnode_t *node;

  for (node = cskl->left_sentinel; node; node = node->nexts[0]) {
    cskl_links_print(node, max_height);

    char nodestr[NODE_STR_LEN];
    node_tostr(node->val, node->height, max_height, nodestr, NODE_STR_LEN -1);
    printf("%s", nodestr);
  }
  printf("\n");

  pfq_rwlock_read_unlock(&cskl->lock);
}


int
cskl_check_node_dump
(
 val_cmp compare,            // compare node values 
 int max_height,             // number of list levels
 cskl_node_tostr node_tostr, // print node value
 csklnode_t *node
)
{
  int i;
  int correct = 1;
  int succIsGreater[node->height];
  for (i = 0; i < node->height; i++) {
    csklnode_t *succ = node->nexts[i];
    succIsGreater[i] = (compare(node->val, succ->val) == -1); 
    correct &= succIsGreater[i];
  }
  if (!correct) {
    for (i = 0; i < node->height; i++) {
      printf("%c", succIsGreater[i] ? '+' : 'X');
    }
    for (i = node->height; i < max_height; i++) {
      printf("-");
    }
    printf(" ");
    
    char nodestr[NODE_STR_LEN];
    cskl_links_dump(node, max_height);
    node_tostr(node->val, node->height, max_height, nodestr, NODE_STR_LEN -1);
    printf("%s", nodestr);
  }
  return correct;
}


void
cskl_check_dump
(
 cskiplist_t *cs,           // cskiplist instance
 cskl_node_tostr node_tostr // print node value
 )
{
  int max_height = cs->max_height;
  csklnode_t *node = cs->left_sentinel;

  bool correct = true;
  printf("***** BEGIN CHECK: skip list %p\n", cs);
  for (node = cs->left_sentinel; node != cs->right_sentinel; node = node->nexts[0]) {
    correct &= cskl_check_node_dump(cs->compare, max_height, node_tostr, node);
  }
  printf("***** END CHECK: skip list %p is %s\n", cs, correct ? "correct" : "incorrect");
}

