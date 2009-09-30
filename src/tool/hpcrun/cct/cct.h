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
//   $HeadURL$
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
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <unwind/common/unwind_cursor.h>
#include <hpcrun/metrics.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/lush/lush-support.h>


//*************************** Forward Declarations **************************


//***************************************************************************
// Calling context tree node
//***************************************************************************

// --------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------

// tallent: was 'size_t'.  If this should change the memcpy in
// hpcfile_cstree_write_node_hlp should be modified.
typedef hpcrun_metricVal_t cct_metric_data_t;

static inline void
cct_metric_data_increment(int metric_id,
			  cct_metric_data_t* x, 
			  cct_metric_data_t incr)
{
  metric_tbl_t* mdata = hpcrun_get_metric_data();
  metric_desc_t* minfo = &(mdata->lst[metric_id]);
  
  if (hpcrun_metricFlags_isFlag(minfo->flags, HPCRUN_MetricFlag_Real)) {
    x->r += incr.r;
  }
  else {
    x->i += incr.i;
  }
}


typedef struct csprof_cct_node_t {

  // ---------------------------------------------------------
  // a persistent node id is assigned for each node. this id
  // is used both to reassemble a tree when reading it from 
  // a file as well as to identify call paths. a call path
  // can simply be represented by the node id of the deepest
  // node in the path.
  // ---------------------------------------------------------
  int32_t persistent_id;

  lush_assoc_info_t as_info;

  // physical instruction pointer: more accurately, this is an
  // 'operation pointer'.  The operation in the instruction packet is
  // represented by adding 0, 1, or 2 to the instruction pointer for
  // the first, second and third operation, respectively.
  void* ip;

  // logical instruction pointer
  lush_lip_t* lip;

  // ---------------------------------------------------------
  // tree structure
  // ---------------------------------------------------------

  // parent node and the beginning of the child list
  struct csprof_cct_node_t* parent;
  struct csprof_cct_node_t* children;

  // singly linked list of siblings
  struct csprof_cct_node_t* next_sibling;

  // ---------------------------------------------------------
  // metrics (N.B.: MUST APPEAR AT END! cf. csprof_cct_node__create)
  // ---------------------------------------------------------
  
  cct_metric_data_t metrics[1]; // variable-sized array

} csprof_cct_node_t;

//
// frame_t values are stored in the backtrace buffer
//

typedef struct hpcrun_frame_t {
  unw_cursor_t cursor;       // hold a copy of the cursor for this frame
  lush_assoc_info_t as_info;
  void* ip;
  void* ra_loc;
  lush_lip_t* lip;
} hpcrun_frame_t;


// returns the number of ancestors walking up the tree
unsigned int
csprof_cct_node__ancestor_count(csprof_cct_node_t* x);

// functions for inspecting links to other nodes

#define csprof_cct_node__parent(/* csprof_cct_node_t* */ x)       \
  (x)->parent
#define csprof_cct_node__next_sibling(/* csprof_cct_node_t* */ x) \
  (x)->next_sibling
#define csprof_cct_node__prev_sibling(/* csprof_cct_node_t* */ x) \
  (x)->prev_sibling
#define csprof_cct_node__first_child(/* csprof_cct_node_t* */ x)  \
  (x)->children
#define csprof_cct_node__last_child(/* csprof_cct_node_t* */ x)   \
  ((x)->children ? (x)->children->prev_sibling : NULL)


//***************************************************************************
// LUSH: thread creation context
//***************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

// Represents the creation creation of a given calling context tree as
// a linked list.
//   get-ctxt(ctxt): [ctxt.context, get-ctxt(ctxt.parent)]
typedef struct lush_cct_ctxt_s {
  
  // the leaf node of the creation context
  csprof_cct_node_t* context;
  
  struct lush_cct_ctxt_s* parent; // a list of lush_cct_ctxt_t

} lush_cct_ctxt_t;


unsigned int
lush_cct_ctxt__length(lush_cct_ctxt_t* cct_ctxt);

int
lush_cct_ctxt__write(FILE* fs, epoch_flags_t flags, lush_cct_ctxt_t* cct_ctxt);


//***************************************************************************
// Calling context tree
//***************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef struct csprof_cct_t {

  csprof_cct_node_t* tree_root;
  unsigned long num_nodes;

} hpcrun_cct_t;


void hpcrun_cct_make_root(hpcrun_cct_t* x, lush_cct_ctxt_t* ctxt);
int csprof_cct__init(hpcrun_cct_t* x, lush_cct_ctxt_t* ctxt);
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
csprof_cct_node_t*
csprof_cct_insert_backtrace(hpcrun_cct_t *x, csprof_cct_node_t* treenode, int metric_id,
			    hpcrun_frame_t *path_beg, hpcrun_frame_t *path_end,
			    cct_metric_data_t sample_count);

csprof_cct_node_t *csprof_cct_get_child(hpcrun_cct_t *cct, 
					csprof_cct_node_t *parent, 
					hpcrun_frame_t *frm);

int hpcrun_cct_fwrite(FILE* fs, epoch_flags_t flags, hpcrun_cct_t* x, lush_cct_ctxt_t* x_ctxt);

#define csprof_cct__isempty(/* hpcrun_cct_t* */x) ((x)->tree_root == NULL)


void
csprof_cct_print_path_to_root(hpcrun_cct_t *tree, csprof_cct_node_t* node);

extern csprof_cct_node_t* hpcrun_copy_btrace(csprof_cct_node_t* n);
extern lush_cct_ctxt_t* copy_thr_ctxt(lush_cct_ctxt_t* thr_ctxt);
extern bool hpcrun_empty_cct(hpcrun_cct_t* cct);


//***************************************************************************

#endif /* csprof_cct_h */
