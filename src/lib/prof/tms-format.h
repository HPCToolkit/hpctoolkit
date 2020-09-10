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
//   Low-level types and functions for reading/writing thread.db
//
//   See thread.db figure.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef TMS_FORMAT_H
#define TMS_FORMAT_H

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
#define HPCTHREADSPARSE_FMT_Magic   "HPCPROF-tmsdb_____" //18 bytes
#define HPCTHREADSPARSE_FMT_Version 0                    //1  byte

#define HPCTHREADSPARSE_FMT_MagicLen   (sizeof(HPCTHREADSPARSE_FMT_Magic) - 1)
#define HPCTHREADSPARSE_FMT_VersionLen 1 

#define HPCTHREADSPARSE_FMT_HeaderLen  (HPCTHREADSPARSE_FMT_MagicLen + HPCTHREADSPARSE_FMT_VersionLen)
#define HPCTHREADSPARSE_FMT_HeaderOff  0

typedef struct tms_hdr_t{
  uint8_t version;
}tms_hdr_t;

int 
tms_hdr_fwrite(FILE* fs);

int
tms_hdr_fread(tms_hdr_t* hdr, FILE* infs);

int
tms_hdr_fprint(tms_hdr_t* hdr, FILE* fs);


//***************************************************************************
// tms_profile_info_t
//***************************************************************************
#define TMS_fake_id_tuple_SIZE   2 //length = 0
#define TMS_total_prof_SIZE      4
#define HPCTHREADSPARSE_FMT_ProfInfoOff (HPCTHREADSPARSE_FMT_HeaderLen + TMS_total_prof_SIZE)

#define TMS_num_val_SIZE         8
#define TMS_num_nzctx_SIZE       4
#define TMS_prof_offset_SIZE     8
#define TMS_id_tuple_ptr_SIZE    8
#define TMS_metadata_ptr_SIZE    8
#define TMS_spare_one_SIZE       8
#define TMS_spare_two_SIZE       8
#define TMS_ptrs_SIZE            (TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE + TMS_spare_one_SIZE + TMS_spare_two_SIZE)
//bytes to skip when we only want val_mids and ctx_id_idxs
#define TMS_prof_skip_SIZE       (TMS_fake_id_tuple_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE) 
#define TMS_prof_info_SIZE       (TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE + TMS_spare_one_SIZE \
  + TMS_spare_two_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE + TMS_prof_offset_SIZE)


typedef struct tms_profile_info_t{
  uint32_t prof_info_idx; //won't be written in thread.db

  uint64_t num_vals;
  uint32_t num_nzctxs;
  uint64_t offset;

  uint64_t id_tuple_ptr;
  uint64_t metadata_ptr; 
  uint64_t spare_one;
  uint64_t spare_two;
}tms_profile_info_t;


int 
tms_profile_info_fwrite(uint32_t num_t,tms_profile_info_t* x, FILE* fs);

int 
tms_profile_info_fread(tms_profile_info_t** x, uint32_t num_prof,FILE* fs);

int 
tms_profile_info_fprint(uint32_t num_prof,tms_profile_info_t* x, FILE* fs);

void 
tms_profile_info_free(tms_profile_info_t** x);

//***************************************************************************
// id tuple
//***************************************************************************
#define TMS_id_tuples_size_SIZE 8

//***************************************************************************
// hpcrun_fmt_sparse_metrics_t related, defined in hpcrun-fmt.h
//***************************************************************************
#define TMS_ctx_id_SIZE    4
#define TMS_ctx_idx_SIZE   8
#define TMS_ctx_pair_SIZE  (TMS_ctx_id_SIZE + TMS_ctx_idx_SIZE)
#define TMS_val_SIZE       8
#define TMS_mid_SIZE       2


int
tms_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
tms_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
tms_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const int prof_info_idx, const char* pre, bool easygrep);

int
tms_sparse_metrics_fprint_grep_helper(hpcrun_fmt_sparse_metrics_t* x, FILE* fs, 
          const metric_tbl_t* metricTbl, const char* pre);

void 
tms_sparse_metrics_free(hpcrun_fmt_sparse_metrics_t* x);


//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //TMS_FORMAT_H