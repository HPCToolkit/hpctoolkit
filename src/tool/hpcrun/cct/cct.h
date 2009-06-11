// -*-Mode: C++;-*- // technically C99
// $Id$

// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
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
//   $Source$
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

//************************* System Include Files ****************************

#include <stdio.h>
#include <stddef.h>

//*************************** User Include Files ****************************

#ifdef CSPROF_TRAMPOLINE_BACKEND
#include "list.h"
#endif

#include <metrics.h>

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/lush/lush-support.h>


#define CSPROF_TREE_USES_DOUBLE_LINKING 0
#define CSPROF_TREE_USES_SORTED_CHILDREN 1

//*************************** Forward Declarations **************************


typedef hpcrun_metric_data_t cct_metric_data_t;

static inline void
cct_metric_data_increment(int metric_id,
			  cct_metric_data_t* x, 
			  cct_metric_data_t incr)
{
  metric_tbl_t *mdata = hpcrun_get_metric_data();
  metric_desc_t *minfo = &(mdata->lst[metric_id]);
  
  if (hpcfile_csprof_metric_is_flag(minfo->flags, HPCFILE_METRIC_FLAG_REAL)) {
    x->r += incr.r;
  }
  else {
    x->i += incr.i;
  }
}

//
// Define hpcrun_cct_node_t as a composition of
//  2 datatypes:
//      1) The data portion (this datatype definition is shared with hpcrun_fmt)
//      2) The tree / linking portion
//

typedef struct hpcrun_cct_node_t
{
  struct hpcrun_cct_node_t *parent;
  struct hpcrun_cct_node_t *sib;
  struct hpcrun_cct_node_t *child;

  // data portion ... has variable data so must be last
  hpcrun_fmt_cct_node_t data;

} hpcrun_cct_node_t;

#if !defined(CSPROF_LIST_BACKTRACE_CACHE)
typedef struct csprof_frame_s {
  lush_assoc_info_t as_info; // LUSH
  void *ip;
  lush_lip_t* lip; // LUSH
  void *sp;
} csprof_frame_t;
#endif


// returns the number of ancestors walking up the tree
unsigned int
csprof_cct_node__ancestor_count(hpcrun_cct_node_t* x);

// functions for inspecting links to other nodes

#define csprof_cct_node__parent(/* hpcrun_cct_node_t* */ x)       \
  (x)->parent
#define csprof_cct_node__next_sibling(/* hpcrun_cct_node_t* */ x) \
  (x)->sib
#define csprof_cct_node__prev_sibling(/* hpcrun_cct_node_t* */ x) \
  (x)->prev_sibling
#define csprof_cct_node__first_child(/* hpcrun_cct_node_t* */ x)  \
  (x)->child
#define csprof_cct_node__last_child(/* hpcrun_cct_node_t* */ x)   \
  ((x)->child ? (x)->children->prev_sibling : NULL)


//***************************************************************************
// LUSH: thread creation context
//***************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

// Represents the creation creation of a given calling context tree as
// a linked list.
//   get-ctxt(ctxt): [ctxt.context, get-ctxt(ctxt.parent)]
//
typedef struct pthread_cct_ctxt_s {
  
  // the leaf node of the creation context
  hpcrun_cct_node_t* context;
  
  struct pthread_cct_ctxt_s* parent; // a list of pthread_cct_ctxt_t

} pthread_cct_ctxt_t;


unsigned int
pthread_cct_ctxt__length(pthread_cct_ctxt_t* cct_ctxt);

int
pthread_cct_ctxt__write(FILE* fs, pthread_cct_ctxt_t* cct_ctxt,
		     unsigned int id_root, unsigned int* nodes_written);


//***************************************************************************
// Calling context tree
//***************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef struct hpcrun_cct_t {

  hpcrun_cct_node_t* tree_root;
  unsigned long num_nodes;

  // next_cpid is used for assigning unique persistent ids to CCT nodes
  // to represent call paths
  int next_cpid;

#ifndef CSPROF_TRAMPOLINE_BACKEND
  /* Two cached arrays: one contains a copy of the most recent
     backtrace (with the first element being the *top* of the call
     stack); the other contains corresponding node pointers into the
     tree (where the last element will point to the tree root).  We
     try to resize these arrays as little as possible.  For
     convenience, the beginning of the array serves as padding instead
     of the end (i.e. the arrays grow from high to smaller indices.) */
  void **cache_bt;
  hpcrun_cct_node_t **cache_nodes;
  unsigned int cache_top;     /* current top (smallest index) of arrays */
  unsigned int cache_len;     /* maximum size of the arrays */
#endif

} hpcrun_cct_t;


int csprof_cct__init(hpcrun_cct_t* x);
int csprof_cct__fini(hpcrun_cct_t *x);


// Given a call path of the following form, insert the path into the
// calling context tree and, if successful, return the leaf node
// representing the sample point (innermost frame).
//
//               (low VMAs)                       (high VMAs)
//   backtrace: [inner-frame......................outer-frame]
//              ^ path_end                        ^ path_beg
//              ^ bt_beg                                       ^ bt_end
//
hpcrun_cct_node_t*
csprof_cct_insert_backtrace(hpcrun_cct_t *x, void *treenode, int metric_id,
			    csprof_frame_t *path_beg, csprof_frame_t *path_end,
			    cct_metric_data_t sample_count);

hpcrun_cct_node_t *csprof_cct_get_child(hpcrun_cct_t *cct, 
					hpcrun_cct_node_t *parent, 
					csprof_frame_t *frm);

int hpcrun_cct_fwrite(hpcrun_cct_t* x, pthread_cct_ctxt_t* x_ctxt, FILE* fs);

#define csprof_cct__isempty(/* hpcrun_cct_t* */x) ((x)->tree_root == NULL)


void
csprof_cct_print_path_to_root(hpcrun_cct_t *tree, hpcrun_cct_node_t* node);


/* private interface */
int csprof_cct__write_txt_q(hpcrun_cct_t *);

int csprof_cct__write_txt_r(FILE *, hpcrun_cct_t *, hpcrun_cct_node_t *);


//***************************************************************************

#endif /* csprof_cct_h */
