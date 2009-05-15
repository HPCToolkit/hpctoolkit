// -*-Mode: C++;-*- // technically C99
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
//   $Source$
//
// Purpose:
//   Support functions for reading/writing a calling context tree
//   from/to a binary file.
//
//   These routines *must not* allocate dynamic memory; if such memory
//   is needed, callbacks to the user's allocator should be used.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_lean_hpcfile_cstree_h
#define prof_lean_hpcfile_cstree_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "hpcfmt.h"

#include "lush/lush-support.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
//***************************************************************************

#define HPCFILE_CSTREE_MAGIC_STR     "HPC_CSTREE"
#define HPCFILE_CSTREE_MAGIC_STR_LEN 10 /* exclude '\0' */

#define HPCFILE_CSTREE_VERSION     "01.00"
#define HPCFILE_CSTREE_VERSION_LEN 5 /* exclude '\0' */

#define HPCFILE_CSTREE_ENDIAN 'l' /* 'l' for little, 'b' for big */

// ---------------------------------------------------------
// hpcfile_cstree_id_t: file identification.
//
// The size is 16 bytes and valid for both big/little endian ordering.
// Although these files will probably not have very long lives on disk,
// hopefully this will not need to be changed.
// ---------------------------------------------------------
typedef struct hpcfile_cstree_id_s {
  
  char magic_str[HPCFILE_CSTREE_MAGIC_STR_LEN]; 
  char version[HPCFILE_CSTREE_VERSION_LEN];
  char endian;  // 'b' or 'l' (currently, redundant info)
  
} hpcfile_cstree_id_t;

int hpcfile_cstree_id__init(hpcfile_cstree_id_t* x);
int hpcfile_cstree_id__fini(hpcfile_cstree_id_t* x);

int hpcfile_cstree_id__fread(hpcfile_cstree_id_t* x, FILE* fs);
int hpcfile_cstree_id__fwrite(hpcfile_cstree_id_t* x, FILE* fs);
int hpcfile_cstree_id__fprint(hpcfile_cstree_id_t* x, FILE* fs, 
			      const char* pre);

// ---------------------------------------------------------
// hpcfile_cstree_hdr_t:
// ---------------------------------------------------------
typedef struct hpcfile_cstree_hdr_s {
  hpcfile_cstree_id_t fid;
  
  // data type sizes (currently, redundant info)
  uint32_t vma_sz;    // 8
  uint32_t uint_sz;   // 8
  
  // data information
  uint64_t num_nodes;         /* number of tree nodes */
  uint32_t epoch;             /* epoch index */
} hpcfile_cstree_hdr_t;


int
hpcfile_cstree_read_hdr(FILE* fs, hpcfile_cstree_hdr_t* hdr);

int hpcfile_cstree_hdr__init(hpcfile_cstree_hdr_t* x);
int hpcfile_cstree_hdr__fini(hpcfile_cstree_hdr_t* x);

int hpcfile_cstree_hdr__fread(hpcfile_cstree_hdr_t* x, FILE* fs);
int hpcfile_cstree_hdr__fwrite(hpcfile_cstree_hdr_t* x, FILE* fs);
int hpcfile_cstree_hdr__fprint(hpcfile_cstree_hdr_t* x, FILE* fs);

// ---------------------------------------------------------
// hpcfile_cstree_nodedata_t:
// ---------------------------------------------------------

#define HPCFILE_TAG__CSTREE_NODE 13 /* just because */
#define HPCFILE_TAG__CSTREE_LIP  77 /* feel free to change */


// tallent: was 'size_t'.  If this should change the memcpy in
// hpcfile_cstree_write_node_hlp should be modified.

typedef union hpcfile_metric_data_u {
  uint64_t bits; // for reading/writing

  uint64_t i; // integral data
  double   r; // real
  
} hpcfile_metric_data_t;

extern hpcfile_metric_data_t hpcfile_metric_data_ZERO;
  
static inline bool hpcfile_metric_data_iszero(hpcfile_metric_data_t x) {
  return (x.bits == hpcfile_metric_data_ZERO.bits);
}

typedef struct hpcfile_cstree_nodedata_s {

  lush_assoc_info_t as_info;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  hpcfile_vma_t ip;

  union {
    hpcfile_uint_t id;  // canonical lip id
    lush_lip_t*    ptr; // pointer
  } lip; 

  // 'sp': the stack pointer of this node
  // tallent: Why is this needed?
  hpcfile_uint_t sp;

  uint32_t cpid;

  hpcfile_uint_t num_metrics;
  hpcfile_metric_data_t* metrics;

} hpcfile_cstree_nodedata_t;

int hpcfile_cstree_nodedata__init(hpcfile_cstree_nodedata_t* x);
int hpcfile_cstree_nodedata__fini(hpcfile_cstree_nodedata_t* x);

int hpcfile_cstree_nodedata__fread(hpcfile_cstree_nodedata_t* x, FILE* fs);
int hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs);
int hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs,
				    const char* pre);

// ---------------------------------------------------------
// 
// ---------------------------------------------------------


int hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs);
int hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs);


int hpcfile_cstree_lip__fread(lush_lip_t* x, FILE* fs);
int hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs);
int hpcfile_cstree_lip__fprint(lush_lip_t* x, hpcfile_uint_t id, 
			       FILE* fs, const char* pre);

// ---------------------------------------------------------
// hpcfile_cstree_node_t: The root node -- the node without a parent -- is
// indicated by identical values for 'id' and 'id_parent'
// ---------------------------------------------------------
typedef struct hpcfile_cstree_node_s {

  hpcfile_cstree_nodedata_t data;

  hpcfile_uint_t id;        // persistent id of self
  hpcfile_uint_t id_parent; // persistent id of parent

} hpcfile_cstree_node_t;

int hpcfile_cstree_node__init(hpcfile_cstree_node_t* x);
int hpcfile_cstree_node__fini(hpcfile_cstree_node_t* x);

int hpcfile_cstree_node__fread(hpcfile_cstree_node_t* x, FILE* fs);
int hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs);
int hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* f, 
				const char* pres);


//***************************************************************************
// 
//***************************************************************************

#define HPCFILE_CSTREE_NODE_ID_NULL 0

#define HPCFILE_CSTREE_ID_ROOT 1

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

#endif /* prof_lean_hpcfile_cstree_h */

