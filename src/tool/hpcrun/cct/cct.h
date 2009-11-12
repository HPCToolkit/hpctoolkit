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

#ifndef cct_h
#define cct_h

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

static inline void
cct_metric_data_increment(int metric_id,
			  cct_metric_data_t* x,
			  cct_metric_data_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  
  if (hpcrun_metricFlags_isFlag(minfo->flags, HPCRUN_MetricFlag_Real)) {
    x->r += incr.r;
  }
  else {
    x->i += incr.i;
  }
}


// FIXME: BT: frame_t really goes elsewhere, but backtrace needs cct_node_t
//            cct ops need frame_t.

// --------------------------------------------------------------------------
// frame_t: similar to cct_node_t, but specialized for the backtrace buffer
// --------------------------------------------------------------------------

typedef struct frame_t {
  unw_cursor_t cursor;       // hold a copy of the cursor for this frame
  lush_assoc_info_t as_info;
  void* ip;
  void* ra_loc;
  lush_lip_t* lip;
} frame_t;

// --------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------

typedef struct cct_node_t {

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
  struct cct_node_t* parent;
  struct cct_node_t* children;

  // singly linked list of siblings
  struct cct_node_t* next_sibling;

  // ---------------------------------------------------------
  // metrics (variable-sized array N.B.: MUST APPEAR AT END OF STRUCTURE!)
  // ---------------------------------------------------------
  
  cct_metric_data_t metrics[]; // variable-sized array

} cct_node_t;


static inline cct_node_t*
cct_node_parent(cct_node_t* x)
{
  return x->parent;
}


static inline cct_node_t*
cct_node_nextSibling(cct_node_t* x)
{
  return x->next_sibling;
}


static inline cct_node_t*
cct_node_firstChild(cct_node_t* x)
{
  return x->children;
}

//***************************************************************************
// thread creation context
//***************************************************************************

// Represents the creation creation of a given calling context tree as
// a linked list.
//   get-ctxt(ctxt): [ctxt.context, get-ctxt(ctxt.parent)]
typedef struct cct_ctxt_t {
  
  // the leaf node of the creation context
  cct_node_t* context;
  
  struct cct_ctxt_t* parent; // a list of cct_ctxt_t

} cct_ctxt_t;


unsigned int
cct_ctxt_length(cct_ctxt_t* cct_ctxt);

int
cct_ctxt_write(FILE* fs, epoch_flags_t flags, cct_ctxt_t* cct_ctxt);


//***************************************************************************
// Calling context tree
//***************************************************************************

typedef struct hpcrun_cct_t {

  cct_node_t* tree_root;
  unsigned long num_nodes;

} hpcrun_cct_t;


void
hpcrun_cct_make_root(hpcrun_cct_t* x, cct_ctxt_t* ctxt);

int
hpcrun_cct_init(hpcrun_cct_t* x, cct_ctxt_t* ctxt);

int
hpcrun_cct_fini(hpcrun_cct_t *x);


// Given a call path of the following form, insert the path into the
// calling context tree and, if successful, return the leaf node
// representing the sample point (innermost frame).
//
//               (low VMAs)                       (high VMAs)
//   backtrace: [inner-frame......................outer-frame]
//              ^ path_end                        ^ path_beg
//              ^ bt_beg                                       ^ bt_end
//
cct_node_t*
hpcrun_cct_insert_backtrace(hpcrun_cct_t* cct, cct_node_t* treenode,
			    int metric_id,
			    frame_t* path_beg, frame_t* path_end,
			    cct_metric_data_t sample_count);

cct_node_t*
hpcrun_cct_get_child(hpcrun_cct_t* cct, cct_node_t* parent, frame_t* frm);

int
hpcrun_cct_fwrite(FILE* fs, epoch_flags_t flags, 
		  hpcrun_cct_t* x, cct_ctxt_t* x_ctxt);

cct_node_t*
hpcrun_copy_btrace(cct_node_t* n);

cct_ctxt_t*
copy_thr_ctxt(cct_ctxt_t* thr_ctxt);

bool
hpcrun_empty_cct(hpcrun_cct_t* cct);

//***************************************************************************

#endif // cct_h
