// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
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

#ifndef HPCFILE_CSTREELIB_H
#define HPCFILE_CSTREELIB_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "hpcfile_general.h"
#include "hpcfile_cstree.h"

//*************************** Forward Declarations **************************

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
// hpcfile_cstree_write()
//***************************************************************************

typedef void  (*hpcfile_cstree_cb__get_data_fn_t)(void*, void*, 
						  hpcfile_cstree_nodedata_t*);
typedef void* (*hpcfile_cstree_cb__get_first_child_fn_t)(void*, void*);
typedef void* (*hpcfile_cstree_cb__get_sibling_fn_t)(void*, void*);


// hpcfile_cstree_write: Writes all nodes of the tree 'tree' beginning
// at 'root' to file stream 'fs'.  The tree should have 'num_nodes'
// nodes.  The user must supply appropriate callback functions; see
// documentation below for their interfaces.  Returns HPCFILE_OK upon
// success; HPCFILE_ERR on error.
int
hpcfile_cstree_write(FILE* fs, void* tree, void* root, 
		     hpcfile_uint_t num_nodes,
                     hpcfile_uint_t epoch,
		     hpcfile_cstree_cb__get_data_fn_t get_data_fn,
		     hpcfile_cstree_cb__get_first_child_fn_t get_first_child_fn,
		     hpcfile_cstree_cb__get_sibling_fn_t get_sibling_fn,
		     int num_metrics);
  
// ---------------------------------------------------------
// callback helpers for hpcfile_cstree_write().
// ---------------------------------------------------------

#if 0 /* describe callbacks */

// Sets fields of node data 'd' given the node 'node' of the tree 'tree'.
void hpcfile_cstree_cb__get_data(void* tree, void* node, 
				 hpcfile_cstree_nodedata_t* d);

// Returns a pointer to the first child of node 'node' of the tree
// 'tree'.  Returns NULL if no children exist.
void* hpcfile_cstree_cb__get_first_child(void* tree, void* node);

// Returns a pointer to the next sibling of 'node' of the tree 'tree'.
// If 'node' does not have a sibling, returns NULL.  (Or, to support
// circular structures, this function may return the same node as
// returned by first_child above.)
void* hpcfile_cstree_cb__get_sibling(void* tree, void* node);

#endif /* describe callbacks */

//***************************************************************************
// hpcfile_cstree_read()
//***************************************************************************

typedef void* 
(*hpcfile_cstree_cb__create_node_fn_t)(void*, 
				       hpcfile_cstree_nodedata_t*, 
				       int);
typedef void  (*hpcfile_cstree_cb__link_parent_fn_t)(void*, void*, void*);

// hpcfile_cstree_read: Given an empty (not non-NULL!) tree 'tree',
// reads tree nodes from the file stream 'fs' and constructs the tree
// using the user supplied callback functions.  See documentation
// below for interfaces for callback interfaces.
//
// The tree data is thoroughly checked for errors. Returns HPCFILE_OK
// upon success; HPCFILE_ERR on error.
int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn,
		    int num_metrics);

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
// hpcfile_cstree_convert_to_txt()
//***************************************************************************

// hpcfile_cstree_convert_to_txt: Given an output file stream 'outfs',
// reads tree data from the input file stream 'infs' and writes it to
// 'outfs' as text for human inspection.  This text output is not
// designed for parsing and any formatting is subject to change.
// Returns HPCFILE_OK upon success; HPCFILE_ERR on error.
int
hpcfile_cstree_convert_to_txt(FILE* infs, FILE* outfs, int num_metrics);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

