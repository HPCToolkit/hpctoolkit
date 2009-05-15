// -*-Mode: C;-*-
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
//    hpcfile_csprof.h
//
// Purpose:
//    Low-level types and functions for reading/writing a call stack
//    profile from/to a binary file.
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCFILE_CSPROF_H
#define HPCFILE_CSPROF_H

//************************* System Include Files ****************************

#include <stdbool.h>

//*************************** User Include Files ****************************

#include "hpcfmt.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
// Types and functions for reading/writing a call stack tree from/to a
// binary file.  
//
// Basic format of HPC_CSPROF: see implementation file for more details
//   HPC_CSPROF header
//   List of data chunks describing profile metrics, etc.
//   HPC_CSTREE data
//
// Data in the file is stored in little-endian format, the ordering of
// IA-64 instructions and the default mode of an IA-64 processor.
//
//***************************************************************************

#define HPCFILE_CSPROF_MAGIC_STR     "HPC_CSPROF"
#define HPCFILE_CSPROF_MAGIC_STR_LEN 10 /* exclude '\0' */

#define HPCFILE_CSPROF_VERSION     "01.01"
#define HPCFILE_CSPROF_VERSION_LEN 5 /* exclude '\0' */

#define HPCFILE_CSPROF_ENDIAN 'l'

// ---------------------------------------------------------
// hpcfile_csprof_id_t: file identification.
//
// The size is 16 bytes and valid for both big/little endian ordering.
// Although these files will probably not have very long lives on
// disk, hopefully this will not need to be changed.
// ---------------------------------------------------------
typedef struct hpcfile_csprof_id_s {
  
  char magic_str[HPCFILE_CSPROF_MAGIC_STR_LEN]; 
  char version[HPCFILE_CSPROF_VERSION_LEN];
  char endian;  // 'b' or 'l' (currently, redundant info)
  
} hpcfile_csprof_id_t;

int hpcfile_csprof_id__init(hpcfile_csprof_id_t* x);
int hpcfile_csprof_id__fini(hpcfile_csprof_id_t* x);

int hpcfile_csprof_id__fread(hpcfile_csprof_id_t* x, FILE* fs);
int hpcfile_csprof_id__fwrite(hpcfile_csprof_id_t* x, FILE* fs);
int hpcfile_csprof_id__fprint(hpcfile_csprof_id_t* x, FILE* fs);


// ---------------------------------------------------------
// hpcfile_csprof_hdr_t:
// ---------------------------------------------------------
typedef struct hpcfile_csprof_hdr_s {

  hpcfile_csprof_id_t fid;

  // data information
  uint64_t num_data; // number of data chucks following header
  
} hpcfile_csprof_hdr_t;

int hpcfile_csprof_hdr__init(hpcfile_csprof_hdr_t* x);
int hpcfile_csprof_hdr__fini(hpcfile_csprof_hdr_t* x);

int hpcfile_csprof_hdr__fread(hpcfile_csprof_hdr_t* x, FILE* fs);
int hpcfile_csprof_hdr__fwrite(hpcfile_csprof_hdr_t* x, FILE* fs);
int hpcfile_csprof_hdr__fprint(hpcfile_csprof_hdr_t* x, FILE* fs);


/* hpcfile_csprof_metric_flag_t */

typedef uint64_t hpcfile_csprof_metric_flag_t;

#define HPCFILE_METRIC_FLAG_NULL  0x0
#define HPCFILE_METRIC_FLAG_ASYNC (1 << 1)
#define HPCFILE_METRIC_FLAG_REAL  (1 << 2)


static inline bool
hpcfile_csprof_metric_is_flag(hpcfile_csprof_metric_flag_t flagbits, 
			      hpcfile_csprof_metric_flag_t f)
{ 
  return (flagbits & f); 
}


static inline void 
hpcfile_csprof_metric_set_flag(hpcfile_csprof_metric_flag_t* flagbits, 
			       hpcfile_csprof_metric_flag_t f)
{
  *flagbits = (*flagbits | f);
}


static inline void 
hpcfile_csprof_metric_unset_flag(hpcfile_csprof_metric_flag_t* flagbits, 
				 hpcfile_csprof_metric_flag_t f)
{
  *flagbits = (*flagbits & ~f);
}


/* hpcfile_csprof_metric_t */

typedef struct hpcfile_csprof_metric_s {
    char *metric_name;          /* name of the metric */
    hpcfile_csprof_metric_flag_t flags;  /* metric flags (async, etc.) */
    uint64_t sample_period;     /* sample period of the metric */
} hpcfile_csprof_metric_t;



int hpcfile_csprof_metric__init(hpcfile_csprof_metric_t *x);
int hpcfile_csprof_metric__fini(hpcfile_csprof_metric_t *x);

int hpcfile_csprof_metric__fread(hpcfile_csprof_metric_t *x, FILE *fs);
int hpcfile_csprof_metric__fwrite(hpcfile_csprof_metric_t *x, FILE *fs);
int hpcfile_csprof_metric__fprint(hpcfile_csprof_metric_t *x, FILE *fs);



// ---------------------------------------------------------
// FIXME: 
// ---------------------------------------------------------

typedef struct ldmodule_s {
  char *name;
  uint64_t vaddr;
  uint64_t  mapaddr;
} ldmodule_t; 

typedef struct epoch_entry_s { 
  uint32_t num_loadmodule;
  ldmodule_t *loadmodule;
} epoch_entry_t;
  
typedef struct epoch_table_s {
  uint32_t num_epoch;
  epoch_entry_t *epoch_modlist;
} epoch_table_t; 


// frees the data of 'x' but not x itself
void epoch_table__free_data(epoch_table_t* x, hpcfile_cb__free_fn_t free_fn);

// ---------------------------------------------------------
// hpcfile_csprof_data_t: used only for passing data; not actually
// part of the file format
// ---------------------------------------------------------

typedef struct hpcfile_csprof_data_s {
  char* target;               // name of profiling target
  uint32_t num_metrics;       // number of metrics recorded
  hpcfile_csprof_metric_t *metrics;
  uint32_t num_ccts;          // number of CCTs
  
} hpcfile_csprof_data_t;

int hpcfile_csprof_data__init(hpcfile_csprof_data_t* x);
int hpcfile_csprof_data__fini(hpcfile_csprof_data_t* x);

int hpcfile_csprof_data__fprint(hpcfile_csprof_data_t* x, FILE* fs);



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

#endif

