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
// Copyright ((c)) 2002-2015, Rice University
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
// File:
//   $HeadURL$
//
// Purpose:
//   Implement an API for a concurrent skip list.
//
//******************************************************************************

//******************************************************************************
// global includes 
//******************************************************************************

#include <stdlib.h>
#include <values.h>



//******************************************************************************
// local includes 
//******************************************************************************

#include "bits.h"
#include "cskiplist.h"
#include "urand.h"



//******************************************************************************
// macros 
//******************************************************************************

#ifndef NULL
#define NULL 0
#endif

#define NO_LAYER -1

// number of bytes in a skip list node with L levels
#define SIZEOF_NODE_T(L) (sizeof(node_t) + sizeof(void*) * L)



//******************************************************************************
// types 
//******************************************************************************

typedef enum {
  cskiplist_find_early_exit,
  cskiplist_find_full
} cskiplist_find_type;



//******************************************************************************
// private operations 
//******************************************************************************

// generate a random level for a skip list node in the interval 
// [1 .. max_height]. these random numbers are distributed such that the 
// probability for each height h is half that of height h - 1, for h in 
// [2 .. max_height]. 
static int
random_level(int max_height)
{
  // generate random numbers with the required distribution by finding the 
  // position of the first one in a random number. if the first one bit is
  // in position > max_height, we can wrap it back to a value in 
  // [0 .. max_height-1] with a mod without disturbing the integrity of the
  // distribution.

  // a random number with the top bit set. knowing that SOME bit is set avoids
  // the need to handle the special case where no bit is set.
  unsigned int random = (unsigned int) (urand() | (1 << (INTBITS - 1)));

  // the following statement will count the trailing zeros in random.
  int first_one_position = __builtin_ctz(random);

  if (first_one_position >= max_height) {
    // wrapping a value >= maxheight with a mod operation preserves the
    // integrity of the desired distribution.
    first_one_position %= max_height; 
  }

  // post-condition: first_one_pos is a random level [0 .. max_height-1]
  // with the desired distribution, assuming that urand() provides a uniform 
  // random distribution.

  // return a random level [1 .. max_height] with the desired distribution.
  return first_one_position + 1; 
}


static inline node_t *
cskiplist_node_new(int value, int height)
{
  node_t *node = calloc(1, SIZEOF_NODE_T(height));

  node->value = value;
  node->height = height;
  node->fully_linked = 0;
  node->marked = 0;
  mcs_init(&(node->lock));

  return node;
}


static inline bool 
cskiplist_node_is_removable(node_t *candidate, int layer)
{ 
  return (candidate->fully_linked && 
          candidate->height - 1 == layer && 
          !candidate->marked);
}


static int 
cskiplist_find_helper
(cskiplist_t *l,         // the skiplist
 int value,              // the value we are seeking 
 node_t *preds[],        // return value: nodes < value
 node_t *succs[],        // return value: nodes >= value
 cskiplist_find_type ft) // is info needed at all layers?
{ 
  int     max_height = l->max_height;
  node_t *pred        = l->left_sentinel;

  int found_layer = NO_LAYER;
  for (int layer = max_height-1; layer >= 0; layer--) {
    node_t *current = pred->nexts[layer]; 

    // look along a layer until we find a value that is not to the left
    // of our value of iterest
    while (value > current->value) {
      pred = current; 
      current = pred->nexts[layer]; 
    }

    if (found_layer == NO_LAYER && // if we haven't already found a match
        value == current->value) { // and the current value is a match
      found_layer = layer;         // remember the layer where we matched
    }

    preds[layer] = pred;     // remember the 'layer' node < value
    succs[layer] = current;  // remember the 'layer' node >= value 

    // some clients of find don't need results for layers below found_layer
    if (found_layer != NO_LAYER && ft == cskiplist_find_early_exit) break;
  }

  return found_layer; 
}


static void inline
unlock_preds
(node_t *preds[],
 mcs_node_t mcs_nodes[],
 int highestLocked)
{
  // unlock each of my predecessors, as necessary
  node_t *prevPred = NULL;
  for (int layer = 0; layer <= highestLocked; layer++) {
    node_t *pred = preds[layer];
    if (pred != prevPred) {
      mcs_unlock(&pred->lock, &mcs_nodes[layer]);
    }
    prevPred = pred;
  }

}



//******************************************************************************
// interface operations 
//******************************************************************************

cskiplist_t *
cskiplist_new(int max_height)
{
  cskiplist_t *l = calloc(1, sizeof(cskiplist_t));
  l->max_height = max_height;

  // create sentinel nodes
  node_t *left = l->left_sentinel   = cskiplist_node_new(MININT, max_height);
  node_t *right = l->right_sentinel = cskiplist_node_new(MAXINT, max_height);

  // hook sentinel nodes in empty list
  for(int i = 0; i < max_height; left->nexts[i++] = right);
  
  return l;
}


node_t *
cskiplist_find(cskiplist_t *l, int value) 
{
  int     max_height  = l->max_height;
  node_t *preds[max_height]; 
  node_t *succs[max_height]; 

  int layer = cskiplist_find_helper(l, value, preds, succs,
            cskiplist_find_early_exit); 

  if (layer != NO_LAYER) {
    node_t *node = succs[layer];
    if (node->fully_linked && !node->marked) return node;
  }

  return NULL;
}


bool
cskiplist_insert(cskiplist_t *l, int value) 
{
  int max_height = l->max_height;
  int my_height  = random_level(l->max_height); 

  node_t *preds[max_height]; 
  node_t *succs[max_height]; 
  mcs_node_t mcs_nodes[max_height]; 

  for (;;) {
    int found_layer= cskiplist_find_helper(l, value, preds, succs, 
                                           cskiplist_find_full); 

    if (found_layer != NO_LAYER) {
      node_t *node = succs[found_layer]; 
      if (!node->marked) {
        while (!node->fully_linked);  
        return false;
      }
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
    int highestLocked = -1; 
    node_t *pred, *succ; 
    node_t *prevPred = NULL; 
    bool valid = true;
    for (int layer = 0; valid && (layer < my_height); layer++) {
      pred=preds[layer]; 
      succ=succs[layer];
      if (pred != prevPred) {
        mcs_lock(&pred->lock, &mcs_nodes[layer]); 
        highestLocked = layer; 
        prevPred = pred;
      }
      valid = !pred->marked && !succ->marked && pred->nexts[layer] == succ;
    }
    if (!valid) {
      // unlock each of my predecessors, as necessary
      unlock_preds(preds, mcs_nodes, highestLocked);
      continue;
    }

    // allocate my node
    node_t *node = cskiplist_node_new(value, my_height); 

    // link it in at levels [0 .. my_height-1]
    for (int layer = 0; layer < my_height; layer++) {
      node->nexts[layer] = succs[layer]; 
      preds[layer]->nexts[layer] = node;
    }

    // mark the node as usable
    node->fully_linked = true;

    // unlock each of my predecessors, as necessary
    unlock_preds(preds, mcs_nodes, highestLocked);

    return true; 
  }
}


bool 
cskiplist_delete(cskiplist_t *l, int v) 
{
  int max_height = l->max_height;
  bool node_is_marked = false;
  node_t     *node   = NULL;

  for (;;) {
    node_t     *preds[max_height]; 
    node_t     *succs[max_height]; 
    mcs_node_t  mcs_nodes[max_height]; 
    int         height = -1;

    //--------------------------------------------------------------------------
    // Look for a node matching v
    //--------------------------------------------------------------------------
    int layer = cskiplist_find_helper(l, v, preds, succs, cskiplist_find_full); 

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
      node_t *pred, *succ;
      node_t *prevPred = NULL; 
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

#define AFTER_HIGH(succs, layer, found) (layer <= hlayer ? succs[layer]->nexts[layer] : succs[layer])

bool 
cskiplist_delete_bulk_unsynchronized(cskiplist_t *l, int low, int high) 
{
  int max_height = l->max_height;
  bool removed_something = false;

  node_t     *lpreds[max_height]; 
  node_t     *other[max_height]; 
  node_t     *hsuccs[max_height]; 

  //----------------------------------------------------------------------------
  // Look for a node matching low
  //----------------------------------------------------------------------------
  int llayer = cskiplist_find_helper(l, low, lpreds, other, 
                                     cskiplist_find_full); 

  //----------------------------------------------------------------------------
  // Look for a node matching low
  //----------------------------------------------------------------------------
  int hlayer = cskiplist_find_helper(l, high, other, hsuccs, 
                                     cskiplist_find_full); 

  bool hfound = hlayer != NO_LAYER;


  //----------------------------------------------------------------------------
  // Delete all of the nodes between the two.
  //----------------------------------------------------------------------------
  node_t *end = AFTER_HIGH(hsuccs, 0, hlayer); 
  for (node_t *node = lpreds[0]->nexts[0]; node != end;) {
    node_t *next = node->nexts[0]; // remember the pointer before trashing node

    // delete node
    free(node);

    node = next;
    removed_something = true;
  }

  //
  //----------------------------------------------------------------------------
  // Complete the deletion: Splice out node at all layers from 0..max_height-1
  //----------------------------------------------------------------------------
  for (int layer = max_height - 1; layer >= 0; layer--) {
    // Splice out node at layer.
    lpreds[layer]->nexts[layer] = AFTER_HIGH(hsuccs, layer, hlayer);
  }

  return removed_something;
}




//******************************************************************************
// unit tests 
//******************************************************************************

#define UNIT_TEST__cskiplist_c__random_level 0
#if UNIT_TEST__cskiplist_c__random_level

#include <stdio.h>
#include <assert.h>

// test that the random levels for skip list node heights have the proper 
// distribution between 1 .. max_height, where the probability of 2^h is
// half that of 2^(h-1), for h in [2 .. max_height]
int main()
{
  int bins[15];
  
  for (int i = 0; i < 15; i++) bins[i] = 0;

  for (int i = 0; i < 10000 * 1024 ; i++) {
    int j = random_level(10);
    assert(1 <= j && j <= 15);
    bins[j-1]++;
  } 
  for (int i = 0; i < 15;i++) 
    printf("bin[%d] = %d\n", i, bins[i]);
}

#endif


#define UNIT_TEST__cskiplist_c_all 0

#if UNIT_TEST__cskiplist_c_all 
				   

static void
cskiplist_space_print(int max_height)
{
  for (int i = 0; i < max_height; i++) {
    printf(" |");
  }
  printf("\n");
}


static void
cskiplist_node_print(node_t *node, int max_height)
{
  printf(" +");
  for (int i = 1; i < node->height; i++) {
    printf("-+");
  }
  for (int i = node->height; i < max_height; i++) {
    printf(" |");
  }
  printf("  %+d\n", node->value);
}


static void
cskiplist_print(cskiplist_t *l)
{
  node_t *node = l->left_sentinel;
  printf("Concurrent Skip List %p:\n", l);
  cskiplist_node_print(node, l->max_height);
  for (node = node=node->nexts[0]; node; node = node->nexts[0]) {
    cskiplist_space_print(l->max_height);
    cskiplist_node_print(node, l->max_height);
  }
  printf("\n");
}


cskiplist_t *
cskiplist_init(int len, int reversed)
{
  cskiplist_t *l = cskiplist_new(6);
  for (int i = 0; i < len; i++) {
    cskiplist_insert(l, reversed ? len - i : i);
  }
  return l;
}


void 
cskiplist_test(int len, int reversed)
{
  cskiplist_t *l = cskiplist_init(len, reversed);
  cskiplist_print(l);
}


void
find_test(cskiplist_t *l, int val, int lmax)
{
  node_t *found = cskiplist_find(l,  val);
  
  printf("Find %d in skiplist of %d: %s\n", val, lmax,
          found != NULL ? "found" : "not found");
}


int main()
{
  for (int i = 0; i < 33; i++) cskiplist_test(i, 0);
  for (int i = 0; i < 33; i++) cskiplist_test(i, 1);

  printf("Input to delete test 0..34\n");
  cskiplist_t *l = cskiplist_init(34, 0);

  printf("Deleting 12\n");
  cskiplist_delete(l, 12);
  cskiplist_print(l);

  printf("Deleting 19\n");
  cskiplist_delete(l, 19);
  cskiplist_print(l);
  

  printf("Deleting 9-27\n");
  cskiplist_delete_bulk_unsynchronized(l, 9, 27);
  cskiplist_print(l);
  
  for (int i = -10; i < 43; i++) find_test(l, i, 34);

  return 0;
}

#endif
