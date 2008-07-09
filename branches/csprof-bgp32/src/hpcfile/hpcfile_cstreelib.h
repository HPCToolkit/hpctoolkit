// -*-Mode: C-*-
// $Id: hpcfile_cstreelib.h 1163 2008-03-07 01:44:02Z eraxxon $

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//    hpcfile_cstreelib.h
//
// Purpose:
//    Types and functions for reading/writing a call stack tree
//    from/to a binary file.  These functions encapsulate the details
//    of reading and writing a call stack tree and therefore allow
//    users to easily switch between different tree implementations.
//    The user must provide certain callback functions for navigating
//    the tree (during writing) or for constructing the tree (during
//    reading).
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_lean_hpcfile_cstreelib_h
#define prof_lean_hpcfile_cstreelib_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "hpcfile_general.h"
#include "hpcfile_cstree.h"

//*************************** Forward Declarations **************************

#define HPCFILE_CSTREE_ID_ROOT 1


#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
// High-level types and functions for reading/writing a call stack tree
// from/to a binary file.
//
// Users will most likely want to use the hpcfile_open_XXX() and
// hpcfile_close() functions to open and close the file streams that
// these functions use.
//
//***************************************************************************

//***************************************************************************
// hpcfile_cstree_read()
//***************************************************************************

typedef void* 
  (*hpcfile_cstree_cb__create_node_fn_t)(void*, 
					 hpcfile_cstree_nodedata_t*);
typedef void (*hpcfile_cstree_cb__link_parent_fn_t)(void*, void*, void*);

// hpcfile_cstree_read: Given an empty (not non-NULL!) tree 'tree',
// reads tree nodes from the file stream 'fs' and constructs the tree
// using the user supplied callback functions.  See documentation
// below for interfaces for callback interfaces.
//
// The tree data is thoroughly checked for errors. Returns HPCFILE_OK
// upon success; HPCFILE_ERR on error.
int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    int num_metrics,
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn);

// ---------------------------------------------------------
// callback helpers for hpcfile_cstree_read().
// ---------------------------------------------------------

#if 0 /* describe callbacks */

// Allocates space for a new tree node 'node' for the tree 'tree' and
// returns a pointer to 'node'.  The node should be initialized with
// data from 'data', but it should not be linked with any other node
// of the tree.  Note: The first call to this function creates the
// root node of the call stack tree; the user should record a pointer
// to this node (e.g. in 'tree') for subsequent access to the call
// stack tree.
void* hpcfile_cstree_cb__create_node(void* tree, 
				     hpcfile_cstree_nodedata_t* data);

// Given two tree nodes 'node' and 'parent' allocated with the above
// function and with 'parent' already linked into the tree, links
// 'node' into 'tree' such that 'parent' is the parent of 'node'.
// Note that while nodes have only one parent node, several nodes may
// have the same parent, indicating that the parent has multiple
// children.
void  hpcfile_cstree_cb__link_parent(void* tree, void* node, void* parent);

#endif /* describe callbacks */

//***************************************************************************
// hpcfile_cstree_fprint()
//***************************************************************************

// hpcfile_cstree_fprint: Given an input file stream 'infs',
// reads tree data from the infs and writes it to 'outfs' as text for
// human inspection.  This text output is not designed for parsing and
// any formatting is subject to change.  Returns HPCFILE_OK upon
// success; HPCFILE_ERR on error.
int
hpcfile_cstree_fprint(FILE* infs, int num_metrics, FILE* outfs);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcfile_cstreelib_h */

