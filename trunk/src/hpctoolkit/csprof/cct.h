// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002-2007, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   csprof_cct.h
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

#ifndef csprof_cct_h
#define csprof_cct_h

/* system include files */

#include <stdio.h>
#include <stddef.h>

/* user include files */

#ifdef CSPROF_TRAMPOLINE_BACKEND
#include "list.h"
#endif

#define CSPROF_TREE_USES_DOUBLE_LINKING 0
#define CSPROF_TREE_USES_SORTED_CHILDREN 1

/* forward declarations */

struct rbtree_node;

struct rbtree
{
    struct rbtree_node *root;
};

/* try to order these so that the most-often referenced things fit into
   a single cache line... */
typedef struct csprof_cct_node_s {
    /* instruction pointer: more accurately, this is an 'operation
       pointer'.  The operation in the instruction packet is represented
       by adding 0, 1, or 2 to the instruction pointer for the first,
       second and third operation, respectively. */
    void *ip;

    /* parent node and the beginning of the child list */
    struct csprof_cct_node_s *parent, *children;

    /* tree index of children */
    struct rbtree tree_children;

    /* singly/doubly linked list of siblings */
    struct csprof_cct_node_s *next_sibling;

    void *sp;
    /* sp is only used if we are using trampolines (i.e.,
       CSPROF_TRAMPOLINE_BACKEND is defined); otherwise its value is
       set to NULL  */

    /* variable-sized array for recording metrics */
    size_t metrics[1];
} csprof_cct_node_t;

#if !defined(CSPROF_LIST_BACKTRACE_CACHE)
typedef struct csprof_frame_s {
    void *ip;
    void *sp;
} csprof_frame_t;
#endif


/* public interface */

// returns the number of ancestors walking up the tree
unsigned int
csprof_cct_node__ancestor_count(csprof_cct_node_t* x);

// functions for inspecting links to other nodes

#define csprof_cct_node__parent(x)       (x)->parent
#define csprof_cct_node__next_sibling(x) (x)->next_sibling
#define csprof_cct_node__prev_sibling(x) (x)->prev_sibling
#define csprof_cct_node__first_child(x)  (x)->children
#define csprof_cct_node__last_child(x)   \
  ((x)->children ? (x)->children->prev_sibling : NULL)

typedef struct csprof_cct_s {
    csprof_cct_node_t* tree_root;
    unsigned long num_nodes;

#ifndef CSPROF_TRAMPOLINE_BACKEND
    /* Two cached arrays: one contains a copy of the most recent
       backtrace (with the first element being the *top* of the call
       stack); the other contains corresponding node pointers into the
       tree (where the last element will point to the tree root).  We
       try to resize these arrays as little as possible.  For
       convenience, the beginning of the array serves as padding instead
       of the end (i.e. the arrays grow from high to smaller indices.) */
    void **cache_bt;
    csprof_cct_node_t **cache_nodes;
    unsigned int cache_top;     /* current top (smallest index) of arrays */
    unsigned int cache_len;     /* maximum size of the arrays */
#endif
} csprof_cct_t;


int csprof_cct__init(csprof_cct_t* x);

csprof_cct_node_t *
csprof_cct_insert_backtrace(csprof_cct_t *, void *, int metric_id,
			    csprof_frame_t *, csprof_frame_t *,
			    size_t);

int csprof_cct__write_txt(csprof_cct_t* x, FILE* fs);

int csprof_cct__write_bin(csprof_cct_t* x, unsigned int, FILE* fs);

/* First argument: 'csprof_cct_t *' */
#define csprof_cct__isempty(x) ((x)->tree_root == NULL)

/* private interface */
int csprof_cct__write_txt_q(csprof_cct_t *);

int csprof_cct__write_txt_r(csprof_cct_t *, csprof_cct_node_t *, FILE *);

#endif 
