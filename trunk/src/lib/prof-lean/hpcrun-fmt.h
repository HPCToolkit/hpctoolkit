// -*-Mode: c;-*- // technically C99
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
//   Low-level types and functions for reading/writing a call path
//   profile as formatted data.
//
//   These routines *must not* allocate dynamic memory; if such memory
//   is needed, callbacks to the user's allocator should be used.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_lean_hpcrun_fmt_h
#define prof_lean_hpcrun_fmt_h

//************************* System Include Files ****************************

#include <stdbool.h>

//*************************** User Include Files ****************************

#include "hpcio.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"

#include "lush/lush-support.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
// Types. functions, and useful macros for reading/writing a call stack tree from/to a
// binary file.  
//
// See hpcrun-fmt.txt for more specifics on the format and interface
//
//***************************************************************************

//***************************************************************************
//
//  Macros for defining "LIST_OF" things
//
//***************************************************************************

// conventional name for LIST_OF stuff

#define LIST_OF(t) list_of_ ## t

// std representation for a LIST_OF things
#define REP_LIST_OF(t) \
struct LIST_OF(t) {    \
  uint32_t len;        \
  t *lst;              \
}

// Macro to typedef a LIST_OF some type

#define typedef_LIST_OF(t) \
typedef REP_LIST_OF(t) LIST_OF(t)

// macro to propagate the error code of another function

#define THROW_HPC_ERR(expr) if (HPCFILE_ERR == (expr)) { return HPCFILE_ERR; }

  // ****** name-value pairs (nvpair) ******

typedef struct nvpair_t {
  char *name;
  char *val;
} nvpair_t;

// Sentinel to mark end of a variadic nv pair list

//***************************************************************************
// 
// some type defs for various LIST_OF things (uses macros from this file)
//
//***************************************************************************

typedef_LIST_OF(nvpair_t);

char *hpcrun_fmt_nvpair_search(LIST_OF(nvpair_t) *lst, const char *name);

// *********  file hdr *************

#define MAGIC "HPCRUN____02.00l"


typedef struct hpcrun_fmt_hdr_t {
  char tag[sizeof(MAGIC)];
  LIST_OF(nvpair_t) nvps;
} hpcrun_fmt_hdr_t;

int hpcrun_fmt_hdr_fwrite(FILE *outfs, ...);
int hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t *hdr, FILE* infs, alloc_fn alloc);
int hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t *hdr, FILE* outf);

//*************************************************************************
//   epoch_hdr
//*************************************************************************

static const char EPOCH_TAG[] = "EPOCH___";

typedef struct hpcrun_fmt_epoch_hdr_t {
  char tag[sizeof(EPOCH_TAG)];
  uint64_t flags;
  uint32_t ra_distance;
  uint64_t granularity;
  LIST_OF(nvpair_t) nvps;
} hpcrun_fmt_epoch_hdr_t;

int hpcrun_fmt_epoch_hdr_fread(hpcrun_fmt_epoch_hdr_t *ehdr, FILE *fs, alloc_fn alloc);
int hpcrun_fmt_epoch_hdr_fwrite(FILE *out, uint64_t flags, uint32_t ra_distance, uint64_t granularity, ...);
int hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t *ehdr, FILE *out);

// ******** metric descriptor and metric table  **********

typedef struct metric_desc_t {
  char *name;
  uint64_t flags;
  uint64_t period;
} metric_desc_t;

typedef_LIST_OF(metric_desc_t);
typedef LIST_OF(metric_desc_t) hpcrun_fmt_metric_tbl_t;
typedef hpcrun_fmt_metric_tbl_t metric_tbl_t;

int hpcrun_fmt_metric_tbl_fwrite(metric_tbl_t *metric_tbl, FILE *out);
int hpcrun_fmt_metric_tbl_fread(metric_tbl_t *metric_tbl, FILE *in, alloc_fn alloc);
int hpcrun_fmt_metric_tbl_fprint(metric_tbl_t *metrics, FILE *out);
void hpcrun_fmt_metric_tbl_free(metric_tbl_t *metric_tbl, free_fn dealloc);

// ******** loadmap entry and loadmaps **********

typedef struct loadmap_entry_t {
  char *name;
  uint64_t vaddr;
  uint64_t mapaddr;
  uint32_t flags;
} loadmap_entry_t;

// This is an actual list of loadmap_entry_t elements

typedef struct loadmap_src_t {
  char *name;
  void *vaddr;
  void *mapaddr;
  size_t size;
  struct loadmap_src_t *next;
} loadmap_src_t;

typedef_LIST_OF(loadmap_entry_t);
typedef LIST_OF(loadmap_entry_t) loadmap_t;

int hpcrun_fmt_loadmap_fwrite(uint32_t num_modules, loadmap_src_t *src, FILE *out);
int hpcrun_fmt_loadmap_fread(loadmap_t *loadmap, FILE *in, alloc_fn alloc);
int hpcrun_fmt_loadmap_fprint(loadmap_t *loadmap, FILE *out);
void hpcrun_fmt_loadmap_free(loadmap_t *loadmap, free_fn dealloc);

// ******** cct nodes **********

typedef union hpcrun_metric_data_u {
  uint64_t bits; // for reading/writing

  uint64_t i; // integral data
  double   r; // real
  
} hpcrun_metric_data_t;

extern hpcrun_metric_data_t hpcrun_metric_data_ZERO;

typedef struct hpcrun_fmt_cct_node_t {
  lush_assoc_info_t as_info;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  void *ip;

  union {
    hpcfmt_uint_t id;  // canonical lip id
    lush_lip_t*    ptr; // pointer
  } lip; 

  // 'sp': the stack pointer of this node
  // tallent: Why is this needed?
  void *sp;

  uint32_t cpid;

#if defined(OLD_CCT)
  uint32_t node_id;
  uint32_t parent_id;
#endif //defined(OLD_CCT)

  uint32_t num_metrics;
  hpcrun_metric_data_t metrics[];

} hpcrun_fmt_cct_node_t;


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
  // char* target;               // name of profiling target
  uint32_t num_metrics;       // number of metrics recorded
  hpcfile_csprof_metric_t *metrics;
  // uint32_t num_ccts;          // number of CCTs
} hpcfile_csprof_data_t;

int hpcfile_csprof_data__init(hpcfile_csprof_data_t* x);
int hpcfile_csprof_data__fini(hpcfile_csprof_data_t* x);

int hpcfile_csprof_data__fprint(hpcfile_csprof_data_t* x, char *target, FILE* fs);



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
int
hpcfile_csprof_fprint(FILE* infs, FILE* outfs, char *target, hpcfile_csprof_data_t* data);


//***************************************************************************
//
//***************************************************************************

// tallent: OBSOLETE

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

  
static inline bool hpcrun_metric_data_iszero(hpcrun_metric_data_t x) {
  return (x.bits == hpcrun_metric_data_ZERO.bits);
}

typedef struct hpcfile_cstree_nodedata_s {

  lush_assoc_info_t as_info;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  hpcfmt_vma_t ip;

  union {
    hpcfmt_uint_t id;  // canonical lip id
    lush_lip_t*    ptr; // pointer
  } lip; 

  // 'sp': the stack pointer of this node
  // tallent: Why is this needed?
  hpcfmt_uint_t sp;

  uint32_t cpid;

  hpcfmt_uint_t num_metrics;
  hpcrun_metric_data_t* metrics;

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
int hpcfile_cstree_lip__fprint(lush_lip_t* x, hpcfmt_uint_t id, 
			       FILE* fs, const char* pre);

// ---------------------------------------------------------
// hpcfile_cstree_node_t: The root node -- the node without a parent -- is
// indicated by identical values for 'id' and 'id_parent'
// ---------------------------------------------------------
typedef struct hpcfile_cstree_node_s {

  hpcfile_cstree_nodedata_t data;

  hpcfmt_uint_t id;        // persistent id of self
  hpcfmt_uint_t id_parent; // persistent id of parent

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

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcrun_fmt_h */

