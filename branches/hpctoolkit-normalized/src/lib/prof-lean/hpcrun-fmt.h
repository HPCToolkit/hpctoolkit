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
// Copyright ((c)) 2002-2010, Rice University 
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

#include "lush/lush-support.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************

// hpcrun profile filename suffix
static const char HPCRUN_ProfileFnmSfx[] = "hpcrun";

// hpcrun trace filename suffix
static const char HPCRUN_TraceFnmSfx[] = "hpctrace";

// hpcrun log filename suffix
static const char HPCRUN_LogFnmSfx[] = "log";

// hpcprof metric db filename suffix
static const char HPCPROF_MetricDBSfx[] = "metric-db";

static const char HPCPROF_TmpFnmSfx[] = "tmp";


//***************************************************************************
// hdr
//***************************************************************************

// N.B.: The header string is 24 bytes of character data

static const char HPCRUN_FMT_Magic[]   = "HPCRUN-profile____"; // 18 bytes
static const char HPCRUN_FMT_Version[] = "02.00";              // 5 bytes
static const char HPCRUN_FMT_Endian[]  = "b";                  // 1 byte

static const int HPCRUN_FMT_MagicLen   = (sizeof(HPCRUN_FMT_Magic) - 1);
static const int HPCRUN_FMT_VersionLen = (sizeof(HPCRUN_FMT_Version) - 1);
static const int HPCRUN_FMT_EndianLen  = (sizeof(HPCRUN_FMT_Endian) - 1);


// currently supported versions
static const double HPCRUN_FMT_Version_20  = 2.0;
static const double HPCRUN_FMT_Version_19A = 1.9;


typedef struct hpcrun_fmt_hdr_t {

  char versionStr[sizeof(HPCRUN_FMT_Version)];
  double version;

  char endian;

  HPCFMT_List(hpcfmt_nvpair_t) nvps;

} hpcrun_fmt_hdr_t;


extern int 
hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t* hdr, FILE* infs, hpcfmt_alloc_fn alloc);

extern int 
hpcrun_fmt_hdr_fwrite(FILE* outfs, ...);

extern int 
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t* hdr, FILE* outf);

extern void 
hpcrun_fmt_hdr_free(hpcrun_fmt_hdr_t* hdr, hpcfmt_free_fn dealloc);


// Note: use #defines to avoid warnings about unused "static const
// char*" variable in a cross-compiler way.
#define HPCRUN_FMT_NV_prog    "program-name"
#define HPCRUN_FMT_NV_path    "path-name"
#define HPCRUN_FMT_NV_jobId   "job-id"
#define HPCRUN_FMT_NV_mpiRank "mpi-rank"
#define HPCRUN_FMT_NV_tid     "thread-id"
#define HPCRUN_FMT_NV_hostid  "host-id"
#define HPCRUN_FMT_NV_pid     "process-id"


//***************************************************************************
// hdr (trace, located here for now)
//***************************************************************************

static const char HPCTRACE_FMT_Magic[]   = "HPCRUN-trace______"; // 18 bytes
static const char HPCTRACE_FMT_Version[] = "01.00";              // 5 bytes
static const char HPCTRACE_FMT_Endian[]  = "b";                  // 1 byte

static const int HPCTRACE_FMT_MagicLen   = (sizeof(HPCTRACE_FMT_Magic) - 1);
static const int HPCTRACE_FMT_VersionLen = (sizeof(HPCTRACE_FMT_Version) - 1);
static const int HPCTRACE_FMT_EndianLen  = (sizeof(HPCTRACE_FMT_Endian) - 1);


int
hpctrace_fmt_hdr_fread(FILE* infs);

int
hpctrace_fmt_hdr_fwrite(FILE* fs);

int
hpctrace_fmt_hdr_fprint(FILE* fs);


//***************************************************************************
// hdr (hpcprof-metricdb, located here for now)
//***************************************************************************

static const char HPCMETRICDB_FMT_Magic[]   = "HPCPROF-metricdb__"; // 18 bytes
static const char HPCMETRICDB_FMT_Version[] = "00.10";              // 5 bytes
static const char HPCMETRICDB_FMT_Endian[]  = "b";                  // 1 byte

static const int HPCMETRICDB_FMT_MagicLen = 
  (sizeof(HPCMETRICDB_FMT_Magic) - 1);
static const int HPCMETRICDB_FMT_VersionLen =
  (sizeof(HPCMETRICDB_FMT_Version) - 1);
static const int HPCMETRICDB_FMT_EndianLen =
  (sizeof(HPCMETRICDB_FMT_Endian) - 1);


typedef struct hpcmetricDB_fmt_hdr_t {

  char versionStr[sizeof(HPCMETRICDB_FMT_Version)];
  double version;
  char endian;

  uint32_t numNodes;
  uint32_t numMetrics;

} hpcmetricDB_fmt_hdr_t;


int
hpcmetricDB_fmt_hdr_fread(hpcmetricDB_fmt_hdr_t* hdr, FILE* infs);

int
hpcmetricDB_fmt_hdr_fwrite(hpcmetricDB_fmt_hdr_t* hdr, FILE* outfs);

int
hpcmetricDB_fmt_hdr_fprint(hpcmetricDB_fmt_hdr_t* hdr, FILE* outfs);


//***************************************************************************
// epoch-hdr
//***************************************************************************

static const char HPCRUN_FMT_EpochTag[] = "EPOCH___";
static const int  HPCRUN_FMT_EpochTagLen = (sizeof(HPCRUN_FMT_EpochTag) - 1);


typedef struct epoch_flags_bitfield {
  bool isLogicalUnwind : 1;
  uint64_t unused      : 63;
} epoch_flags_bitfield;


typedef union epoch_flags_t {
  epoch_flags_bitfield fields;
  uint64_t             bits; // for reading/writing
} epoch_flags_t;


typedef struct hpcrun_fmt_epochHdr_t {

  epoch_flags_t flags;
  uint64_t measurementGranularity;
  uint32_t raToCallsiteOfst;
  HPCFMT_List(hpcfmt_nvpair_t) nvps;

} hpcrun_fmt_epochHdr_t;


extern int 
hpcrun_fmt_epochHdr_fread(hpcrun_fmt_epochHdr_t* ehdr, FILE* fs,
			  hpcfmt_alloc_fn alloc);

extern int 
hpcrun_fmt_epochHdr_fwrite(FILE* out, epoch_flags_t flags, 
			   uint64_t measurementGranularity, 
			   uint32_t raToCallsiteOfst, ...);

extern int 
hpcrun_fmt_epochHdr_fprint(hpcrun_fmt_epochHdr_t* ehdr, FILE* out);

extern void 
hpcrun_fmt_epochHdr_free(hpcrun_fmt_epochHdr_t* ehdr, hpcfmt_free_fn dealloc);


//***************************************************************************
// metric-tbl
//***************************************************************************

// --------------------------------------------------------------------------
// hpcrun_metricFlags_t
// --------------------------------------------------------------------------


typedef enum {

  MetricFlags_Ty_NULL = 0,
  MetricFlags_Ty_Raw,
  MetricFlags_Ty_Final,
  MetricFlags_Ty_Derived

} MetricFlags_Ty_t;


typedef enum {

  MetricFlags_ValTy_NULL = 0,
  MetricFlags_ValTy_Incl,
  MetricFlags_ValTy_Excl

} MetricFlags_ValTy_t;


typedef enum {

  MetricFlags_ValFmt_NULL = 0,
  MetricFlags_ValFmt_Int,
  MetricFlags_ValFmt_Real,

  MetricFlags_ValFmt_Real_19A = (1 << 2) // TODO: deprecate old file version

} MetricFlags_ValFmt_t;


typedef struct hpcrun_metricFlags_bitfield {
  MetricFlags_Ty_t     ty     : 4;
  MetricFlags_ValTy_t  valTy  : 4;
  MetricFlags_ValFmt_t valFmt : 4;
  uint partner      : 16;
  bool show         : 1;
  bool showPercent  : 1;

  uint64_t unused0 : 34;
  uint64_t unused1;
} hpcrun_metricFlags_bitfield;


typedef union hpcrun_metricFlags_t {

  hpcrun_metricFlags_bitfield fields;

  uint64_t bits[2]; // for reading/writing

} hpcrun_metricFlags_t;


extern hpcrun_metricFlags_t hpcrun_metricFlags_NULL;


#if 0
static inline bool
hpcrun_metricFlags_isFlag(hpcrun_metricFlags_t flagbits, 
			  hpcrun_metricFlags_t f)
{ 
  return (flagbits & f); 
}


static inline void 
hpcrun_metricFlags_setFlag(hpcrun_metricFlags_t* flagbits, 
			   hpcrun_metricFlags_t f)
{
  *flagbits = (*flagbits | f);
}


static inline void 
hpcrun_metricFlags_unsetFlag(hpcrun_metricFlags_t* flagbits, 
			     hpcrun_metricFlags_t f)
{
  *flagbits = (*flagbits & ~f);
}
#endif


// --------------------------------------------------------------------------
// hpcrun_metricVal_t
// --------------------------------------------------------------------------

typedef union hpcrun_metricVal_u {

  uint64_t i; // integral data
  double   r; // real
  void*    p; // address data

  uint64_t bits; // for reading/writing
  
} hpcrun_metricVal_t;

extern hpcrun_metricVal_t hpcrun_metricVal_ZERO;

static inline bool
hpcrun_metricVal_isZero(hpcrun_metricVal_t x)
{
  return (x.bits == hpcrun_metricVal_ZERO.bits);
}


// --------------------------------------------------------------------------
// metric_desc_t
// --------------------------------------------------------------------------

typedef struct metric_desc_t {

  char* name;
  char* description;

  hpcrun_metricFlags_t flags;

  uint64_t period;

  char* formula;
  char* format;

} metric_desc_t;

extern metric_desc_t metricDesc_NULL;


typedef struct metric_list_t {
  struct metric_list_t* next;
  metric_desc_t val;
  int id;
} metric_list_t;



HPCFMT_List_declare(metric_desc_t);
typedef HPCFMT_List(metric_desc_t) metric_tbl_t; // hpcrun_metricTbl_t

typedef metric_desc_t* metric_desc_p_t;
HPCFMT_List_declare(metric_desc_p_t);
typedef HPCFMT_List(metric_desc_p_t) metric_desc_p_tbl_t; // HPCFMT_List of metric_desc_t*

extern int
hpcrun_fmt_metricTbl_fread(metric_tbl_t* metric_tbl, FILE* in, 
			   double fmtVersion, hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_metricTbl_fwrite(metric_desc_p_tbl_t* metric_tbl, FILE* out);

extern int
hpcrun_fmt_metricTbl_fprint(metric_tbl_t* metrics, FILE* out);

extern void
hpcrun_fmt_metricTbl_free(metric_tbl_t* metric_tbl, hpcfmt_free_fn dealloc);


extern int
hpcrun_fmt_metricDesc_fread(metric_desc_t* x, FILE* infs, 
			    double fmtVersion, hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_metricDesc_fwrite(metric_desc_t* x, FILE* outfs);

extern int
hpcrun_fmt_metricDesc_fprint(metric_desc_t* x, FILE* outfs, const char* pre);

extern void
hpcrun_fmt_metricDesc_free(metric_desc_t* x, hpcfmt_free_fn dealloc);


//***************************************************************************
// loadmap
//***************************************************************************

typedef struct loadmap_entry_t {

  uint16_t id;      // 0 is reserved as a NULL value
  char* name;
  uint64_t flags;
  uint64_t mapaddr; // FIXME: obsolete
} loadmap_entry_t;


HPCFMT_List_declare(loadmap_entry_t);
typedef HPCFMT_List(loadmap_entry_t) loadmap_t; // hpcrun_loadmap_t


extern int 
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* infs, hpcfmt_alloc_fn alloc);

extern int 
hpcrun_fmt_loadmap_fwrite(loadmap_t* loadmap, FILE* outfs);

extern int 
hpcrun_fmt_loadmap_fprint(loadmap_t* loadmap, FILE* outfs);

extern void 
hpcrun_fmt_loadmap_free(loadmap_t* loadmap, hpcfmt_free_fn dealloc);


extern int
hpcrun_fmt_loadmapEntry_fread(loadmap_entry_t* x, FILE* infs, 
			      hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_loadmapEntry_fwrite(loadmap_entry_t* x, FILE* outfs);

extern int
hpcrun_fmt_loadmapEntry_fprint(loadmap_entry_t* x, FILE* outfs, 
			       const char* pre);

extern void 
hpcrun_fmt_loadmapEntry_free(loadmap_entry_t* x, hpcfmt_free_fn dealloc);


//***************************************************************************
// cct
//***************************************************************************

#define HPCRUN_FMT_CCTNodeId_NULL (0)

#define HPCRUN_FMT_RetainIdFlag (0x1)

static inline bool
hpcrun_fmt_doRetainId(uint32_t id)
{
  return (id & HPCRUN_FMT_RetainIdFlag);
}


// --------------------------------------------------------------------------
// hpcrun_fmt_cct_node_t
// --------------------------------------------------------------------------

typedef struct hpcrun_fmt_cct_node_t {

  // id and parent id.  0 is reserved as a NULL value
  uint32_t id;
  uint32_t id_parent;

  lush_assoc_info_t as_info;

  // load module id.  0 is reserved as a NULL value
  uint16_t lm_id;

  // instruction pointer: more accurately, this is an 'operation
  // pointer'.  The operation in the instruction packet is represented
  // by adding 0, 1, or 2 to the instruction pointer for the first,
  // second and third operation, respectively.
  hpcfmt_vma_t lm_offset;

  // logical instruction pointer
  lush_lip_t lip;

  hpcfmt_uint_t num_metrics;
  hpcrun_metricVal_t* metrics;
  
} hpcrun_fmt_cct_node_t;


static inline void
hpcrun_fmt_cct_node_init(hpcrun_fmt_cct_node_t* x)
{
  memset(x, 0, sizeof(*x));
}


// N.B.: assumes space for metrics has been allocated
extern int 
hpcrun_fmt_cct_node_fread(hpcrun_fmt_cct_node_t* x, 
			  epoch_flags_t flags, FILE* fs);

extern int 
hpcrun_fmt_cct_node_fwrite(hpcrun_fmt_cct_node_t* x, 
			   epoch_flags_t flags, FILE* fs);

extern int 
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs, 
			   epoch_flags_t flags, const char* pre);


// --------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------

extern int 
hpcrun_fmt_lip_fread(lush_lip_t* x, FILE* fs);

extern int 
hpcrun_fmt_lip_fwrite(lush_lip_t* x, FILE* fs);

extern int 
hpcrun_fmt_lip_fprint(lush_lip_t* x, FILE* fs, const char* pre);


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcrun_fmt_h */

