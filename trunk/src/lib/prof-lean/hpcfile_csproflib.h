// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    hpcfile_csproflib.h
//
// Purpose:
//    Types and functions for reading/writing a call stack profile
//    from/to a binary file. 
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCFILE_CSPROFLIB_H
#define HPCFILE_CSPROFLIB_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "hpcfile_general.h"
#include "hpcfile_csprof.h"
#include "hpcfile_cstreelib.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
// High-level types and functions for reading/writing a call stack profile
// from/to a binary file.
//
// Basic format of HPC_CSPROF: see implementation file for more details
//   HPC_CSPROF header
//   List of data chunks describing profile metrics, etc.
//   HPC_CSTREE data
//
// Users will most likely want to use the hpcfile_open_XXX() and
// hpcfile_close() functions to open and close the file streams that
// these functions use.
//
//***************************************************************************

//***************************************************************************
// hpcfile_csprof_write()
//***************************************************************************

// hpcfile_csprof_write: Writes the call stack profile data in 'data'
// to file stream 'fs'.  The user should supply and manage all data
// contained in 'data'.  Returns HPCFILE_OK upon success; HPCFILE_ERR
// on error.
//
// Note: Any corresponding call stack tree is *not* written by this
// function.  Users must also call hpcfile_cstree_write().
int
hpcfile_csprof_write(FILE* fs, hpcfile_csprof_data_t* data);

//***************************************************************************
// hpcfile_csprof_read()
//***************************************************************************

// hpcfile_csprof_read: Reads call stack profile data from the file
// stream 'fs' into 'data'.  Uses callback functions to manage any
// memory allocation.  Note that the *user* is responsible for freeing
// any memory allocated for pointers in 'data' (characer strings,
// etc.).  Returns HPCFILE_OK upon success; HPCFILE_ERR on error.
//
// Note: Any corresponding call stack tree is *not* read by this
// function.  Users must also call hpcfile_cstree_read().
int
hpcfile_csprof_read(FILE* fs, hpcfile_csprof_data_t* data,  
                    epoch_table_t* epochtbl,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn);

//***************************************************************************
// hpcfile_csprof_convert_to_txt()
//***************************************************************************

// hpcfile_csprof_convert_to_txt: Given an output file stream 'outfs',
// reads profile data from the input file stream 'infs' and writes it to
// 'outfs' as text for human inspection.  This text output is not
// designed for parsing and any formatting is subject to change.
// Returns HPCFILE_OK upon success; HPCFILE_ERR on error.
//
// Note: Any corresponding call stack tree is *not* converted by this
// function.  Users must also call hpcfile_cstree_convert_to_txt().
int
hpcfile_csprof_convert_to_txt(FILE* infs, FILE* outfs);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

