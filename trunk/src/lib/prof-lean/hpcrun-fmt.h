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
//   Low-level types and functions for reading/writing a call path
//   profile as formatted data.
//
//   See hpcrun-fmt.txt.
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
// hdr
//***************************************************************************

// N.B.: The header string is 24 bytes of character data

static const char HPCRUN_FMT_Magic[]   = "HPCRUN-profile____"; // 18 bytes
static const char HPCRUN_FMT_Version[] = "01.98";              // 5 bytes
static const char HPCRUN_FMT_Endian[]  = "b";                  // 1 byte

static const int HPCRUN_FMT_MagicLen   = (sizeof(HPCRUN_FMT_Magic) - 1);
static const int HPCRUN_FMT_VersionLen = (sizeof(HPCRUN_FMT_Version) - 1);
static const int HPCRUN_FMT_EndianLen  = (sizeof(HPCRUN_FMT_Endian) - 1);

typedef struct hpcrun_fmt_hdr_t {

  char version[sizeof(HPCRUN_FMT_Version)];
  char endian;

  HPCFMT_List(hpcfmt_nvpair_t) nvps;

} hpcrun_fmt_hdr_t;


int
hpcrun_fmt_hdr_fwrite(FILE* outfs, ...);

int
hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t* hdr, FILE* infs, hpcfmt_alloc_fn alloc);

int
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t* hdr, FILE* outf);

// FIXME: need hpcrun_fmt_hdr_free()


//***************************************************************************
// epoch-hdr
//***************************************************************************

static const char HPCRUN_FMT_EpochTag[] = "EPOCH___";
static const int  HPCRUN_FMT_EpochTagLen = (sizeof(HPCRUN_FMT_EpochTag) - 1);


typedef struct hpcrun_fmt_epoch_hdr_t {

  uint64_t flags;
  uint32_t ra_distance;
  uint64_t granularity;
  HPCFMT_List(hpcfmt_nvpair_t) nvps;

} hpcrun_fmt_epoch_hdr_t;


int
hpcrun_fmt_epoch_hdr_fread(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs, 
			   hpcfmt_alloc_fn alloc);

int 
hpcrun_fmt_epoch_hdr_fwrite(FILE* out, uint64_t flags, uint32_t ra_distance, 
			    uint64_t granularity, ...);

int
hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* out);

// FIXME: need hpcrun_fmt_hdr_free()


//***************************************************************************
// metric-tbl
//***************************************************************************

typedef struct metric_desc_t {

  char* name;
  uint64_t flags;
  uint64_t period;

} metric_desc_t;

HPCFMT_List_declare(metric_desc_t);

typedef HPCFMT_List(metric_desc_t) hpcrun_fmt_metric_tbl_t;

typedef hpcrun_fmt_metric_tbl_t metric_tbl_t;

int 
hpcrun_fmt_metric_tbl_fwrite(metric_tbl_t* metric_tbl, FILE* out);

int
hpcrun_fmt_metric_tbl_fread(metric_tbl_t* metric_tbl, FILE* in, 
			    hpcfmt_alloc_fn alloc);

int
hpcrun_fmt_metric_tbl_fprint(metric_tbl_t* metrics, FILE* out);

void
hpcrun_fmt_metric_tbl_free(metric_tbl_t* metric_tbl, hpcfmt_free_fn dealloc);


//***************************************************************************
// loadmap
//***************************************************************************

typedef struct loadmap_entry_t {

  char* name;
  uint64_t vaddr;
  uint64_t mapaddr;
  uint32_t flags;

} loadmap_entry_t;

// This is an actual list of loadmap_entry_t elements

typedef struct loadmap_src_t {

  char* name;
  void* vaddr;
  void* mapaddr;
  size_t size;
  struct loadmap_src_t* next;

} loadmap_src_t;

HPCFMT_List_declare(loadmap_entry_t);
typedef HPCFMT_List(loadmap_entry_t) loadmap_t;

int
hpcrun_fmt_loadmap_fwrite(uint32_t num_modules, loadmap_src_t* src, FILE* out);

int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* in, hpcfmt_alloc_fn alloc);

int
hpcrun_fmt_loadmap_fprint(loadmap_t* loadmap, FILE* out);

void
hpcrun_fmt_loadmap_free(loadmap_t* loadmap, hpcfmt_free_fn dealloc);


//***************************************************************************
// cct
//***************************************************************************

// --------------------------------------------------------------------------
// hpcrun_metric_data_t
// --------------------------------------------------------------------------

typedef union hpcrun_metric_data_u {

  uint64_t bits; // for reading/writing

  uint64_t i; // integral data
  double   r; // real
  
} hpcrun_metric_data_t;

extern hpcrun_metric_data_t hpcrun_metric_data_ZERO;

static inline bool hpcrun_metric_data_iszero(hpcrun_metric_data_t x) {
  return (x.bits == hpcrun_metric_data_ZERO.bits);
}


// --------------------------------------------------------------------------
// hpcfile_csprof_metric_flag_t
// --------------------------------------------------------------------------

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


// --------------------------------------------------------------------------
// hpcfile_csprof_metric_t
// --------------------------------------------------------------------------
// FIXME: is this needed?

typedef struct hpcfile_csprof_metric_s {

  char *metric_name;          /* name of the metric */
  hpcfile_csprof_metric_flag_t flags;  /* metric flags (async, etc.) */
  uint64_t sample_period;     /* sample period of the metric */

} hpcfile_csprof_metric_t;


int hpcfile_csprof_metric__init(hpcfile_csprof_metric_t* x);
int hpcfile_csprof_metric__fini(hpcfile_csprof_metric_t* x);

int hpcfile_csprof_metric__fread(hpcfile_csprof_metric_t* x, FILE* fs);
int hpcfile_csprof_metric__fwrite(hpcfile_csprof_metric_t* x, FILE* fs);
int hpcfile_csprof_metric__fprint(hpcfile_csprof_metric_t* x, FILE* fs);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs);
int hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs);


int hpcfile_cstree_lip__fread(lush_lip_t* x, FILE* fs);
int hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs);
int hpcfile_cstree_lip__fprint(lush_lip_t* x, FILE* fs, const char* pre);


// --------------------------------------------------------------------------
// hpcrun_fmt_cct_node_t
// --------------------------------------------------------------------------

#define HPCRUN_FMT_CCTNode_NULL 0


typedef struct hpcrun_fmt_cct_node_t {

  lush_assoc_info_t as_info;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  hpcfmt_vma_t ip;

  lush_lip_t lip;

  hpcfmt_uint_t num_metrics;
  hpcrun_metric_data_t* metrics;
  
} hpcrun_fmt_cct_node_t;


// --------------------------------------------------------------------------
// FIXME: this code should be replaced by hpcrun_fmt_cct_node
// --------------------------------------------------------------------------

#define RETAIN_ID_FOR_TRACE_FLAG 1 // FIXME

typedef struct hpcfile_cstree_nodedata_s {

  lush_assoc_info_t as_info;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  hpcfmt_vma_t ip;

  lush_lip_t lip;

  hpcfmt_uint_t num_metrics;
  hpcrun_metric_data_t* metrics;

} hpcfile_cstree_nodedata_t;

int hpcfile_cstree_nodedata__init(hpcfile_cstree_nodedata_t* x);
int hpcfile_cstree_nodedata__fini(hpcfile_cstree_nodedata_t* x);

int hpcfile_cstree_nodedata__fread(hpcfile_cstree_nodedata_t* x, FILE* fs);
int hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs);
int hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs,
				    const char* pre);


typedef struct hpcfile_cstree_node_s {

  hpcfile_cstree_nodedata_t data;

  uint32_t id;        // persistent id of self
  uint32_t id_parent; // persistent id of parent

} hpcfile_cstree_node_t;

int hpcfile_cstree_node__init(hpcfile_cstree_node_t* x);
int hpcfile_cstree_node__fini(hpcfile_cstree_node_t* x);

int hpcfile_cstree_node__fread(hpcfile_cstree_node_t* x, FILE* fs);
int hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs);
int hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs, 
				const char* pre);


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcrun_fmt_h */

