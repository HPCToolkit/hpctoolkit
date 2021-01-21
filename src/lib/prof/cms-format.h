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
// Copyright ((c)) 2002-2020, Rice University
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
// Purpose:
//   Low-level types and functions for reading/writing cct.db
//
//   See cct.db figure. //TODO change this
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCTOOLKIT_PROF_CMS_FORMAT_H
#define HPCTOOLKIT_PROF_CMS_FORMAT_H

//************************* System Include Files ****************************

#include <stdbool.h>
#include <limits.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "../prof-lean/hpcio.h"
#include "../prof-lean/hpcio-buffer.h"
#include "../prof-lean/hpcfmt.h"
#include "../prof-lean/hpcrun-fmt.h"


//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// hdr
//***************************************************************************
#define HPCCCTSPARSE_FMT_Magic        "HPCPROF-cctdb___"  //16 bytes
#define HPCCCTSPARSE_FMT_VersionMajor 1                   //1  byte
#define HPCCCTSPARSE_FMT_VersionMinor 0                   //1  byte
#define HPCCCTSPARSE_FMT_NumSec       1                   //2  byte

#define HPCCCTSPARSE_FMT_MagicLen     (sizeof(HPCCCTSPARSE_FMT_Magic) - 1)
#define HPCCCTSPARSE_FMT_VersionLen   2
#define HPCCCTSPARSE_FMT_NumCtxLen    4
#define HPCCCTSPARSE_FMT_NumSecLen    2
#define HPCCCTSPARSE_FMT_SecSizeLen   8
#define HPCCCTSPARSE_FMT_SecPtrLen    8
#define HPCCCTSPARSE_FMT_SecLen       (HPCCCTSPARSE_FMT_SecSizeLen + HPCCCTSPARSE_FMT_SecPtrLen)


#define CMS_real_hdr_SIZE (HPCCCTSPARSE_FMT_MagicLen + HPCCCTSPARSE_FMT_VersionLen \
  + HPCCCTSPARSE_FMT_NumCtxLen + HPCCCTSPARSE_FMT_NumSecLen \
  + HPCCCTSPARSE_FMT_SecLen * HPCCCTSPARSE_FMT_NumSec)
#define CMS_hdr_SIZE 128 //starting position of context info section

typedef struct cms_hdr_t{
  uint8_t versionMajor;
  uint8_t versionMinor;
  uint32_t num_ctx;
  uint16_t num_sec; //number of sections include hdr and sparse metrics section

  uint64_t ctx_info_sec_size;
  uint64_t ctx_info_sec_ptr;
}cms_hdr_t;

int 
cms_hdr_fwrite(cms_hdr_t* hdr,FILE* fs);

int
cms_hdr_fread(cms_hdr_t* hdr, FILE* infs);

int
cms_hdr_fprint(cms_hdr_t* hdr, FILE* fs);


//***************************************************************************
// cms_ctx_info_t
//***************************************************************************
#define CMS_num_ctx_SIZE       4
#define CMS_ctx_id_SIZE        4
#define CMS_num_val_SIZE       8
#define CMS_num_nzmid_SIZE     2
#define CMS_ctx_offset_SIZE    8
#define CMS_ctx_info_SIZE      (CMS_ctx_id_SIZE + CMS_num_val_SIZE + CMS_num_nzmid_SIZE + CMS_ctx_offset_SIZE)


typedef struct cms_ctx_info_t{
  uint32_t ctx_id;
  uint64_t num_vals;
  uint16_t num_nzmids;
  uint64_t offset;
}cms_ctx_info_t;

int
cms_ctx_info_fwrite(cms_ctx_info_t* x, uint32_t num_ctx, FILE* fs);

int 
cms_ctx_info_fread(cms_ctx_info_t** x, uint32_t num_ctx,FILE* fs);

int 
cms_ctx_info_fprint(uint32_t num_ctx, cms_ctx_info_t* x, FILE* fs);

void 
cms_ctx_info_free(cms_ctx_info_t** x);

//***************************************************************************
// cct_sparse_metrics_t
//***************************************************************************
#define CMS_mid_SIZE                2
#define CMS_m_idx_SIZE              8
#define CMS_m_pair_SIZE             (CMS_mid_SIZE + CMS_m_idx_SIZE)
#define CMS_val_SIZE                8
#define CMS_prof_idx_SIZE           4
#define CMS_val_prof_idx_pair_SIZE  (CMS_val_SIZE + CMS_prof_idx_SIZE)

typedef struct cct_sparse_metrics_t{
  uint32_t ctx_id;

  uint64_t num_vals;
  uint16_t num_nzmids;
  hpcrun_metricVal_t* values;
  uint32_t* tids; 
  uint16_t* mids;
  uint64_t* m_idxs;
}cct_sparse_metrics_t;

int
cms_sparse_metrics_fwrite(cct_sparse_metrics_t* x, FILE* fs);

int 
cms_sparse_metrics_fread(cct_sparse_metrics_t* x, FILE* fs);

int 
cms_sparse_metrics_fprint(cct_sparse_metrics_t* x, FILE* fs,
          const char* pre, bool easygrep);

int
cms_sparse_metrics_fprint_grep_helper(cct_sparse_metrics_t* x, 
          FILE* fs, const char* pre);

void 
cms_sparse_metrics_free(cct_sparse_metrics_t* x);


//***************************************************************************
// footer
//***************************************************************************
#define CCTDBftr 0x4343544442667472


//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //HPCTOOLKIT_PROF_CMS_FORMAT_H