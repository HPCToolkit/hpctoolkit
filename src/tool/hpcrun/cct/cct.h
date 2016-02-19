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
// Copyright ((c)) 2002-2016, Rice University
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
#include <ucontext.h>

#include <assert.h>

//*************************** User Include Files ****************************

#include <hpcrun/metrics.h>

#include <hpcrun/unwind/common/backtrace.h>

#include <hpcrun/utilities/ip-normalized.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include "cct_addr.h"

//
// Readability Macros (to facilitate coding initialization operations)
//
#define CCT_ROOT HPCRUN_FMT_LMId_NULL, HPCRUN_FMT_LMIp_NULL
#define PARTIAL_ROOT HPCRUN_FMT_LMId_NULL, HPCRUN_FMT_LMIp_Flag1
#define ADDR_I(L)     NON_LUSH_ADDR_INI(L)
#define ADDR(L)      (cct_addr_t) NON_LUSH_ADDR_INI(L)
#define ADDR2_I(id, ip) NON_LUSH_ADDR_INI(id, ip)
#define ADDR2(id, ip) (cct_addr_t) ADDR2_I(id, ip)

//***************************************************************************
// Calling context tree node (abstract data type)
//***************************************************************************


#define IS_PARTIAL_ROOT(addr) \
	(addr->ip_norm.lm_id == HPCRUN_FMT_LMId_NULL) && \
	(addr->ip_norm.lm_ip == HPCRUN_FMT_LMIp_Flag1)

typedef struct cct_node_t cct_node_t;
//
// In order to associate data with a given calling context,
// cct nodes need an id type (abstract)
//

typedef cct_node_t* cct_node_id_t;

//
// Interface procedures
//

//
// Constructors
//
extern cct_node_t* hpcrun_cct_new(void);
extern cct_node_t* hpcrun_cct_new_partial(void);
extern cct_node_t* hpcrun_cct_new_special(void* addr);
extern cct_node_t* hpcrun_cct_top_new(uint16_t lmid, uintptr_t lmip);
// 
// Accessor functions
// 

extern cct_node_t* hpcrun_cct_parent(cct_node_t* node);
extern int32_t hpcrun_cct_persistent_id(cct_node_t* node);
extern cct_addr_t* hpcrun_cct_addr(cct_node_t* node);
extern bool hpcrun_cct_is_leaf(cct_node_t* node);
extern cct_node_t* hpcrun_cct_insert_path_return_leaf(cct_node_t *root, cct_node_t *path);
extern void hpcrun_cct_delete_self(cct_node_t *node);
//
// NOTE: having no children is not exactly the same as being a leaf
//       A leaf represents a full path. There might be full paths
//       that are a prefix of other full paths. So, a "leaf" can have children
//
extern bool hpcrun_cct_no_children(cct_node_t* node);
extern bool hpcrun_cct_is_root(cct_node_t* node);

//
// Mutator functions: modify a given cct
//

//
// Fundamental mutation operation: insert a given addr into the
// set of children of a given cct node. Return the cct_node corresponding
// to the inserted addr [NB: if the addr is already in the node children, then
// the already present node is returned. Otherwise, a new node is created, linked in,
// and returned]
//
extern cct_node_t* hpcrun_cct_insert_addr(cct_node_t* cct, cct_addr_t* addr);

//
// 2nd fundamental mutator: mark a node as "terminal". That is,
//   it is the last node of a path
//
extern void hpcrun_cct_terminate_path(cct_node_t* node);
//
// Special purpose mutator:
// This operation is somewhat akin to concatenation.
// An already constructed cct ('src') is inserted as a
// child of the 'target' cct. The addr field of the src
// cct is ASSUMED TO BE DIFFERENT FROM ANY ADDR IN target's
// child set. [Otherwise something recursive has to happen]
//
//
extern cct_node_t* hpcrun_cct_insert_node(cct_node_t* target, cct_node_t* src);

extern void hpcrun_cct_insert_path(cct_node_t ** root, cct_node_t* path);

// special mutator to support tracing
extern void hpcrun_cct_persistent_id_trace_mutate(cct_node_t* x);

// Walking functions section:
//
//     typedefs for walking functions
typedef void* cct_op_arg_t;
typedef void (*cct_op_t)(cct_node_t* cct, cct_op_arg_t arg, size_t level);

//
//     general walking functions: (client may select starting level)
//       visits every node in the cct, calling op(node, arg, level)
//       level has property that node at level n ==> children at level n+1
//     there are 2 different walking strategies:
//       1) walk the children, then walk the node
//       2) walk the node, then walk the children
//     there is no implied children ordering
//

//
// visting order: children first, then node
//
extern void hpcrun_cct_walk_child_1st_w_level(cct_node_t* cct,
					      cct_op_t op,
					      cct_op_arg_t arg, size_t level);

//
// visting order: node first, then children
//
extern void hpcrun_cct_walk_node_1st_w_level(cct_node_t* cct,
					     cct_op_t op,
					     cct_op_arg_t arg, size_t level);
//
// Top level walking routines:
//  frequently, a walk will start with level = 0
//  the static inline routines below implement this utility
//

static inline
void hpcrun_cct_walk_child_1st(cct_node_t* cct,
			       cct_op_t op, cct_op_arg_t arg)
{
  hpcrun_cct_walk_child_1st_w_level(cct, op, arg, 0);
}

static inline
void hpcrun_cct_walk_node_1st(cct_node_t* cct,
			      cct_op_t op, cct_op_arg_t arg)
{
  hpcrun_cct_walk_node_1st_w_level(cct, op, arg, 0);
}

//
// Special routine to walk a path represented by a cct node.
// The actual path represented by a node is list reversal of the nodes
//  linked by the parent link. So walking a path involves touching the
// path nodes in list reverse order
//
extern void hpcrun_walk_path(cct_node_t* node, cct_op_t op, cct_op_arg_t arg);

//
// utility walker for cct sets (part of the substructure of a cct)
//
extern void hpcrun_cct_walkset(cct_node_t* cct, cct_op_t fn, cct_op_arg_t arg);
//
// Writing operation
//
int hpcrun_cct_fwrite(cct_node_t* cct, FILE* fs, epoch_flags_t flags);
//
// Utilities
//
extern size_t hpcrun_cct_num_nodes(cct_node_t* cct);
//
// look up addr in the set of cct's children
// return the found node or NULL
//
extern cct_node_t* hpcrun_cct_find_addr(cct_node_t* cct, cct_addr_t* addr);
//
// Merging operation: Given 2 ccts : CCT_A, CCT_B,
//    merge means add all paths in CCT_B that are NOT in CCT_A
//    to CCT_A. For paths that are common, perform the merge operation on
//    each common node, using auxiliary arg merge_arg
//
//    NOTE: this merge operation presumes
//       cct_addr_data(CCT_A) == cct_addr_data(CCT_B)
//
typedef void* merge_op_arg_t;
typedef void (*merge_op_t)(cct_node_t* a, cct_node_t*b, merge_op_arg_t arg);

extern void hpcrun_cct_merge(cct_node_t* cct_a, cct_node_t* cct_b,
			     merge_op_t merge, merge_op_arg_t arg);

#endif // cct_h
