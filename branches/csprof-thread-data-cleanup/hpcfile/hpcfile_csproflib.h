// -*-Mode: C-*-
// $Id$

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

#ifndef prof_lean_hpcfile_csproflib_h
#define prof_lean_hpcfile_csproflib_h

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
hpcfile_csprof_read(FILE* fs, 
		    hpcfile_csprof_data_t* data, epoch_table_t* epochtbl,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn);

//***************************************************************************
// hpcfile_csprof_fprint()
//***************************************************************************

// hpcfile_csprof_fprint: Given an output file stream 'outfs',
// reads profile data from the input file stream 'infs' and writes it to
// 'outfs' as text for human inspection.  This text output is not
// designed for parsing and any formatting is subject to change.
// Returns HPCFILE_OK upon success; HPCFILE_ERR on error.
//
// Note: Any corresponding call stack tree is *not* converted by this
// function.  Users must also call hpcfile_cstree_fprint().
int
hpcfile_csprof_fprint(FILE* infs, FILE* outfs, hpcfile_csprof_data_t* data);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcfile_csproflib_h */
