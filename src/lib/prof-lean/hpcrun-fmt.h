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
// Copyright ((c)) 2002-2021, Rice University
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
#include <limits.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "id-tuple.h"

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

//***************************************************************************
// hdr
//***************************************************************************

// N.B.: The header string is 24 bytes of character data

static const char HPCRUN_FMT_Magic[]   = "HPCRUN-profile____"; // 18 bytes
static const char HPCRUN_FMT_Version[] = "04.00";              // 5 bytes
static const char HPCRUN_FMT_Endian[]  = "b";                  // 1 byte

static const int HPCRUN_FMT_MagicLen   = (sizeof(HPCRUN_FMT_Magic) - 1);
static const int HPCRUN_FMT_VersionLen = (sizeof(HPCRUN_FMT_Version) - 1);
static const int HPCRUN_FMT_EndianLen  = (sizeof(HPCRUN_FMT_Endian) - 1);


// currently supported versions
static const double HPCRUN_FMT_Version_20 = 2.0;


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


// Note: use #defines to (portably) avoid warnings about unused
// "static const char*" variables.
#define HPCRUN_FMT_NV_prog     "program-name"
#define HPCRUN_FMT_NV_progPath "program-path"
#define HPCRUN_FMT_NV_envPath  "env-path"
#define HPCRUN_FMT_NV_jobId    "job-id"
#define HPCRUN_FMT_NV_mpiRank  "mpi-rank"
#define HPCRUN_FMT_NV_tid      "thread-id"
#define HPCRUN_FMT_NV_hostid   "host-id"
#define HPCRUN_FMT_NV_pid      "process-id"

#define HPCRUN_FMT_NV_traceMinTime "trace-min-time"
#define HPCRUN_FMT_NV_traceMaxTime "trace-max-time"
#define HPCRUN_FMT_NV_traceOrdered "trace-time-ordered"

#define HPCRUN_FMT_METRIC_HIDE            0
#define HPCRUN_FMT_METRIC_SHOW            1
#define HPCRUN_FMT_METRIC_SHOW_INCLUSIVE  2
#define HPCRUN_FMT_METRIC_SHOW_EXCLUSIVE  3
#define HPCRUN_FMT_METRIC_INVISIBLE       4



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

typedef struct metric_desc_properties_t {
  unsigned time:1;
  unsigned cycles:1;
} metric_desc_properties_t;

#define metric_property_time  ( (metric_desc_properties_t) { .time = 1 } )
#define metric_property_cycles (  (metric_desc_properties_t) { .cycles = 1 } )
#define metric_property_none (  (metric_desc_properties_t) { } )

extern int
hpcrun_fmt_epochHdr_fread(hpcrun_fmt_epochHdr_t* ehdr, FILE* fs,
			  hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_epochHdr_fwrite(FILE* out, epoch_flags_t flags,
			   uint64_t measurementGranularity,
			   ...);

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

} MetricFlags_ValFmt_t;


// N.B. do *not* use sub-byte bit fields since compilers are free to
// reorder them within the byte.
typedef struct hpcrun_metricFlags_fields {
  MetricFlags_Ty_t     ty     : 8;
  MetricFlags_ValTy_t  valTy  : 8;
  MetricFlags_ValFmt_t valFmt : 8;
  uint8_t              unused0;

  uint16_t             partner;
  uint8_t /*bool*/     show;
  uint8_t /*bool*/     showPercent;

  uint64_t unused1;
} hpcrun_metricFlags_fields;


typedef union hpcrun_metricFlags_t {

  hpcrun_metricFlags_fields fields;

  uint8_t bits[2 * 8]; // for reading/writing

  uint64_t bits_big[2]; // for easy initialization

} hpcrun_metricFlags_t;


// FIXME: tallent: temporarily support old non-portable convention
typedef struct hpcrun_metricFlags_bitfield_XXX {
  MetricFlags_Ty_t     ty     : 4;
  MetricFlags_ValTy_t  valTy  : 4;
  MetricFlags_ValFmt_t valFmt : 4;
  uint partner      : 16;
  bool show         : 1;
  bool showPercent  : 1;

  uint64_t unused0 : 34;
  uint64_t unused1;
} hpcrun_metricFlags_bitfield_XXX;


// FIXME: tallent: temporarily support old non-portable convention
typedef union hpcrun_metricFlags_XXX_t {

  hpcrun_metricFlags_bitfield_XXX fields;

  uint64_t bits[2]; // for reading/writing

} hpcrun_metricFlags_XXX_t;


extern const hpcrun_metricFlags_t hpcrun_metricFlags_NULL;


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

  metric_desc_properties_t properties;

  char* formula;
  char* format;

  bool is_frequency_metric;
} metric_desc_t;

extern const metric_desc_t metricDesc_NULL;


HPCFMT_List_declare(metric_desc_t);
typedef HPCFMT_List(metric_desc_t) metric_tbl_t; // hpcrun_metricTbl_t

typedef metric_desc_t* metric_desc_p_t;
HPCFMT_List_declare(metric_desc_p_t);
typedef HPCFMT_List(metric_desc_p_t) metric_desc_p_tbl_t; // HPCFMT_List of metric_desc_t*


extern int
hpcrun_fmt_metricTbl_fread(metric_tbl_t* metric_tbl, metric_aux_info_t **aux_info, FILE* in,
			   double fmtVersion, hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_metricTbl_fwrite(metric_desc_p_tbl_t* metric_tbl, metric_aux_info_t *aux_info, FILE* out);

extern int
hpcrun_fmt_metricTbl_fprint(metric_tbl_t* metrics, metric_aux_info_t *aux_info, FILE* out);

extern void
hpcrun_fmt_metricTbl_free(metric_tbl_t* metric_tbl, hpcfmt_free_fn dealloc);


extern int
hpcrun_fmt_metricDesc_fread(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* infs,
			    double fmtVersion, hpcfmt_alloc_fn alloc);

extern int
hpcrun_fmt_metricDesc_fwrite(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* outfs);

extern int
hpcrun_fmt_metricDesc_fprint(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* outfs, const char* pre, uint32_t id);

extern void
hpcrun_fmt_metricDesc_free(metric_desc_t* x, hpcfmt_free_fn dealloc);

// ---------------------------------------------------------
// metric get and set
// ---------------------------------------------------------

double
hpcrun_fmt_metric_get_value(metric_desc_t metric_desc, hpcrun_metricVal_t metric);

void
hpcrun_fmt_metric_set_value(metric_desc_t metric_desc,
   hpcrun_metricVal_t *metric, double value);

void
hpcrun_fmt_metric_set_value_int( hpcrun_metricFlags_t *flags,
   hpcrun_metricVal_t *metric, int value);

void
hpcrun_fmt_metric_set_value_real( hpcrun_metricFlags_t *flags,
   hpcrun_metricVal_t *metric, double value);

void
hpcrun_fmt_metric_set_format(metric_desc_t *metric_desc, char *format);


//***************************************************************************
// loadmap
//***************************************************************************

typedef struct loadmap_entry_t {

  uint16_t id; // HPCRUN_FMT_LMId_NULL is the NULL value
  char* name;
  uint64_t flags;

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

#define HPCRUN_FMT_LMId_NULL (0)

#define HPCRUN_FMT_LMIp_NULL  (0)
#define HPCRUN_FMT_LMIp_Flag1 (1)

// Primary syntethic root:   <lm-id: NULL, lm-ip: NULL>
// Secondary synthetic root: <lm-id: NULL, lm-ip: Flag1>


typedef struct hpcrun_fmt_cct_node_t {

  // id and parent id.  0 is reserved as a NULL value
  uint32_t id;
  uint32_t id_parent;

  lush_assoc_info_t as_info;

  // load module id. Use HPCRUN_FMT_LMId_NULL as a NULL value.
  uint16_t lm_id;

  // static instruction pointer: more accurately, this is a static
  // 'operation pointer'.  The operation in the instruction packet is
  // represented by adding 0, 1, or 2 to the instruction pointer for
  // the first, second and third operation, respectively.
  hpcfmt_vma_t lm_ip;

  // static logical instruction pointer
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

#if 0
extern int
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs,
			   epoch_flags_t flags, const metric_tbl_t* metricTbl,
			   const char* pre);
#else
//YUMENG: no need to parse metricTbl for sparse format
extern int
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs,
			   epoch_flags_t flags,const char* pre);
#endif

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
// sparse metrics - YUMENG
//***************************************************************************

// --------------------------------------------------------------------------
// hpcrun_fmt_sparse_metrics_t
// --------------------------------------------------------------------------
static const uint32_t LastNodeEnd = 0x656E6421;
static const uint16_t LastMidEnd  = 0x6564;

typedef struct hpcrun_fmt_sparse_metrics_t{
  //uint32_t tid; 
  id_tuple_t id_tuple;
  uint64_t num_vals;
  uint64_t num_cct_nodes;
  hpcrun_metricVal_t* values;
  uint16_t* mids;

  uint64_t cur_cct_node_idx;

  //cct_node_id : cct_node_idxs pairs
  uint32_t *cct_node_ids;
  uint64_t *cct_node_idxs;
  uint32_t num_nz_cct_nodes;
}hpcrun_fmt_sparse_metrics_t;

typedef struct hpcrun_fmt_sparse_metrics_t hpcrun_fmt_sparse_metrics_t;

extern int
hpcrun_fmt_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

extern int
hpcrun_fmt_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

extern int
hpcrun_fmt_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
			   const metric_tbl_t* metricTbl, const char* pre, bool easy_grep);

int
hpcrun_fmt_sparse_metrics_fprint_grep_helper(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const char* pre);

void
hpcrun_fmt_sparse_metrics_free(hpcrun_fmt_sparse_metrics_t* x, hpcfmt_free_fn dealloc);


// --------------------------------------------------------------------------
// hpcrun_fmt_footer_t
// --------------------------------------------------------------------------
static const uint64_t HPCRUNsm = 0x48504352554E736D;

typedef struct hpcrun_fmt_footer_t{
  uint64_t hdr_start;
  uint64_t hdr_end;
  uint64_t loadmap_start;
  uint64_t loadmap_end;
  uint64_t cct_start;
  uint64_t cct_end;
  uint64_t met_tbl_start;
  uint64_t met_tbl_end;
  uint64_t sm_start;
  uint64_t sm_end;
  uint64_t footer_start;

  uint64_t HPCRUNsm;
}hpcrun_fmt_footer_t;

int
hpcrun_fmt_footer_fwrite(hpcrun_fmt_footer_t* x, FILE* fs);

int
hpcrun_fmt_footer_fread(hpcrun_fmt_footer_t* x, FILE* fs);

int
hpcrun_fmt_footer_fprint(hpcrun_fmt_footer_t* x, FILE* fs, const char* pre);


// --------------------------------------------------------------------------
// hpcrun_sparse_file
// --------------------------------------------------------------------------
#define OPENED 0
#define PAUSED 1
#define MODE(m) (m == 0) ? "OPENED" : "PAUSED"

static const int SF_SUCCEED = 0;
static const int SF_END     = 0;
static const int SF_FAIL    = 1;
static const int SF_ERR     = -1;

static const int SF_footer_SIZE           = 96; 
static const int SF_num_lm_SIZE           = 4; 
static const int SF_num_metric_SIZE       = 4;
static const int SF_num_cct_SIZE          = 8;
static const int SF_cct_node_SIZE         = 18; // id:4 id-parent:4 lm-id:2 im-ip:8
static const int SF_tid_SIZE              = 4;
static const int SF_num_val_SIZE          = 8;
static const int SF_val_SIZE              = 8;
static const int SF_mid_SIZE              = 2;
static const int SF_num_nz_cct_node_SIZE  = 4;
static const int SF_cct_node_id_SIZE      = 4;
static const int SF_cct_node_idx_SIZE     = 8;

typedef struct hpcrun_sparse_file {
  FILE* file;
  hpcrun_fmt_footer_t footer;

  //use for Pause, Resume
  bool mode;
  size_t cur_pos;
  size_t start_pos;
  size_t end_pos;

  //keep track for next_xx functions
  uint64_t num_cct_nodes;       //should be 32-bit since cct id is 32-bit, but the original writing in cct section for num_cct_nodes is 64-bit
  uint32_t cct_nodes_read;
  size_t metric_bytes_read;
  uint16_t cur_metric_id; //count the id, metric desc doesn't have it
  size_t lm_bytes_read;
  uint32_t sm_block_touched;

  //to read metric values for current block, initialized when hpcrun_sparse_next_block is called
  size_t cur_block_end; //in terms of number of nzvals 
  size_t cur_block_start;//in terms of number of nzvals 
  uint64_t num_nzval;
  uint32_t num_nz_cct_nodes;
  size_t cct_node_id_idx_offset;
  size_t val_mid_offset;
  

} hpcrun_sparse_file_t;

void hpcrun_sparse_footer_update_w_start(hpcrun_fmt_footer_t *f, size_t start_pos);

hpcrun_sparse_file_t* hpcrun_sparse_open(const char* path, size_t start_pos, size_t end_pos);
int hpcrun_sparse_pause(hpcrun_sparse_file_t* sparse_fs);
int hpcrun_sparse_resume(hpcrun_sparse_file_t* sparse_fs, const char* path);
void hpcrun_sparse_close(hpcrun_sparse_file_t* sparse_fs);
int hpcrun_sparse_check_mode(hpcrun_sparse_file_t* sparse_fs, bool expected, const char* msg);

int hpcrun_sparse_read_hdr(hpcrun_sparse_file_t* sparse_fs, hpcrun_fmt_hdr_t* hdr);
int hpcrun_sparse_next_lm(hpcrun_sparse_file_t* sparse_fs, loadmap_entry_t* lm);
int hpcrun_sparse_next_metric(hpcrun_sparse_file_t* sparse_fs, metric_desc_t* m, metric_aux_info_t* perf_info,double fmtVersion);
int hpcrun_sparse_next_context(hpcrun_sparse_file_t* sparse_fs, hpcrun_fmt_cct_node_t* node);
int hpcrun_sparse_read_id_tuple(hpcrun_sparse_file_t* sparse_fs, id_tuple_t* id_tuple);
int hpcrun_sparse_next_block(hpcrun_sparse_file_t* sparse_fs);
int hpcrun_sparse_next_entry(hpcrun_sparse_file_t* sparse_fs, hpcrun_metricVal_t* val);


//***************************************************************************
// hpctrace (located here for now)
//***************************************************************************

//***************************************************************************
// [hpctrace] hdr
//***************************************************************************

// Header sizes:
// - version 1.00: 24 bytes
// - version 1.01: 32 bytes: 24 + sizeof(hpctrace_hdr_flags_t)

static const char HPCTRACE_FMT_Magic[]   = "HPCRUN-trace______"; // 18 bytes
static const char HPCTRACE_FMT_Version[] = "01.01";              // 5 bytes
static const char HPCTRACE_FMT_Endian[]  = "b";                  // 1 byte

// Use of bit fields is not recommended as the order of fields
// is compiler and architecture dependent.
/*
typedef struct hpctrace_hdr_flags_bitfield {
  bool isDataCentric : 1;
  bool isLCARecorded : 1;
  uint64_t unused    : 62;
} hpctrace_hdr_flags_bitfield;
*/

// Substitute bit fields with macros
#define HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS 0U
#define HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS 1U

#define HPCTRACE_HDR_FLAGS_GET_BIT(flag, pos) \
  ((flag >> pos) & 1U)
#define HPCTRACE_HDR_FLAGS_SET_BIT(flag, pos, value) \
  flag ^= (-value ^ flag) & (1ULL << pos)

typedef uint64_t hpctrace_hdr_flags_t;

extern const hpctrace_hdr_flags_t hpctrace_hdr_flags_NULL;


#define HPCTRACE_FMT_MagicLenX   (sizeof(HPCTRACE_FMT_Magic) - 1)
#define HPCTRACE_FMT_VersionLenX (sizeof(HPCTRACE_FMT_Version) - 1)
#define HPCTRACE_FMT_EndianLenX  (sizeof(HPCTRACE_FMT_Endian) - 1)
#define HPCTRACE_FMT_FlagsLenX   (sizeof(hpctrace_hdr_flags_t))

static const int HPCTRACE_FMT_MagicLen   = HPCTRACE_FMT_MagicLenX;
static const int HPCTRACE_FMT_VersionLen = HPCTRACE_FMT_VersionLenX;
static const int HPCTRACE_FMT_EndianLen  = HPCTRACE_FMT_EndianLenX;
static const int HPCTRACE_FMT_FlagsLen   = HPCTRACE_FMT_FlagsLenX;

static const int HPCTRACE_FMT_HeaderLen =
  HPCTRACE_FMT_MagicLenX +
  HPCTRACE_FMT_VersionLenX +
  HPCTRACE_FMT_EndianLenX +
  HPCTRACE_FMT_FlagsLenX;


typedef struct hpctrace_fmt_hdr_t {

  char versionStr[sizeof(HPCTRACE_FMT_Version)];
  double version;

  char endian;

  hpctrace_hdr_flags_t flags;

} hpctrace_fmt_hdr_t;


int
hpctrace_fmt_hdr_fread(hpctrace_fmt_hdr_t* hdr, FILE* infs);

int
hpctrace_fmt_hdr_outbuf(hpctrace_hdr_flags_t flags, hpcio_outbuf_t* outbuf);

// N.B.: not async safe
int
hpctrace_fmt_hdr_fwrite(hpctrace_hdr_flags_t flags, FILE* fs);

int
hpctrace_fmt_hdr_fprint(hpctrace_fmt_hdr_t* hdr, FILE* fs);


//***************************************************************************
// [hpctrace] trace record/datum
//***************************************************************************

// Time and dLCA is stored in one 64-bit integer (not true at present)

// Time in nanoseconds is stored in lower HPCTRACE_FMT_TIME_BITS bits.
#define HPCTRACE_FMT_TIME_BITS 64
#if 0
#define HPCTRACE_FMT_TIME_MAX ((~(0ULL)) >> (64 - HPCTRACE_FMT_TIME_BITS))
#define HPCTRACE_FMT_GET_TIME(bits) \
  (bits & HPCTRACE_FMT_TIME_MAX)
#define HPCTRACE_FMT_SET_TIME(bits, time) \
  bits = (bits & (~HPCTRACE_FMT_TIME_MAX)) | (time & HPCTRACE_FMT_TIME_MAX)
// dLCA = distance of previous sample's leaf call frame to
// the Least Common Ancestor (LCA) with this sample in the CCT.
// dLCA is only valid when trampoline is used.
// dLCA is stored in higher HPCTRACE_FMT_DLCA_BITS bits, supporting up to 1023.
#define HPCTRACE_FMT_DLCA_BITS 10 // Use 10 bits to store dLCA.
#define HPCTRACE_FMT_DLCA_NULL ((1ULL << HPCTRACE_FMT_DLCA_BITS) - 1) // 10 bits of 1s
#define HPCTRACE_FMT_GET_DLCA(bits) \
  ((bits >> HPCTRACE_FMT_TIME_BITS) & HPCTRACE_FMT_DLCA_NULL)
#define HPCTRACE_FMT_SET_DLCA(bits, dLCA) \
  bits = (bits & (~(HPCTRACE_FMT_DLCA_NULL << HPCTRACE_FMT_TIME_BITS))) \
         | ((((uint64_t)dLCA) & HPCTRACE_FMT_DLCA_NULL) << HPCTRACE_FMT_TIME_BITS)

#else
#define HPCTRACE_FMT_GET_TIME(bits) (bits & ~0ULL)
#define HPCTRACE_FMT_SET_TIME(bits, time) bits = time
#define HPCTRACE_FMT_GET_DLCA(bits) (0ULL)
#define HPCTRACE_FMT_SET_DLCA(bits, dLCA) ;
#define HPCTRACE_FMT_DLCA_NULL  (0ULL)
#endif

#define HPCTRACE_FMT_MetricId_NULL (INT_MAX) // for Java, no UINT32_MAX

typedef uint64_t hpctrace_fmt_time_dLCA_composite_t;

typedef struct hpctrace_fmt_datum_t {
  hpctrace_fmt_time_dLCA_composite_t comp; // composite field that stores both time and dLCA
  uint32_t cpId; // call path id (CCT leaf id); cf. HPCRUN_FMT_CCTNodeId_NULL
  uint32_t metricId;
} hpctrace_fmt_datum_t;


int
hpctrace_fmt_datum_fread(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			 FILE* fs);

int
hpctrace_fmt_datum_outbuf(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  hpcio_outbuf_t* outbuf);

// N.B.: not async safe
int
hpctrace_fmt_datum_fwrite(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  FILE* outfs);

char*
hpctrace_fmt_datum_swrite(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  char* buf);

int
hpctrace_fmt_datum_fprint(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  FILE* fs);


//***************************************************************************
// hpcprof-metricdb (located here for now)
//***************************************************************************

//***************************************************************************
// [hpcprof-metricdb] hdr
//***************************************************************************

static const char HPCMETRICDB_FMT_Magic[]   = "HPCPROF-metricdb__"; // 18 bytes
static const char HPCMETRICDB_FMT_Version[] = "00.10";              // 5 bytes
static const char HPCMETRICDB_FMT_Endian[]  = "b";                  // 1 byte

#define HPCMETRICDB_FMT_MagicLenX   (sizeof(HPCMETRICDB_FMT_Magic) - 1)
#define HPCMETRICDB_FMT_VersionLenX (sizeof(HPCMETRICDB_FMT_Version) - 1)
#define HPCMETRICDB_FMT_EndianLenX  (sizeof(HPCMETRICDB_FMT_Endian) - 1)

static const int HPCMETRICDB_FMT_MagicLen   = HPCMETRICDB_FMT_MagicLenX;
static const int HPCMETRICDB_FMT_VersionLen = HPCMETRICDB_FMT_VersionLenX;
static const int HPCMETRICDB_FMT_EndianLen  = HPCMETRICDB_FMT_EndianLenX;

static const int HPCMETRICDB_FMT_HeaderLen =
  (HPCMETRICDB_FMT_MagicLenX + HPCMETRICDB_FMT_VersionLenX
   + HPCMETRICDB_FMT_EndianLenX);



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

// --------------------------------------------------------------------------
// additional sampling info
// --------------------------------------------------------------------------

typedef struct sampling_info_s {
  uint64_t  sample_clock;
  void     *sample_data;
} sampling_info_t;


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcrun_fmt_h */

