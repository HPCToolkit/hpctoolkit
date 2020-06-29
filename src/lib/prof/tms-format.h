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
//   Low-level types and functions for reading/writing thread_major_sparse.db
//
//   See thread_major_sparse figure. //TODO change this
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
// tms_profile_info_t
//***************************************************************************
const int TMS_total_prof_SIZE   = 4;
const int TMS_tid_SIZE          = 4;
const int TMS_num_val_SIZE      = 8;
const int TMS_num_nzctx_SIZE    = 4;
const int TMS_prof_offset_SIZE  = 8;
const int TMS_prof_skip_SIZE    = TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE; 
const int TMS_prof_info_SIZE    = TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE + TMS_prof_offset_SIZE;


typedef struct tms_profile_info_t{
  uint32_t tid;
  uint64_t num_vals;
  uint32_t num_nzctxs;
  uint64_t offset;
}tms_profile_info_t;

int 
tms_profile_info_fwrite(uint32_t num_t,tms_profile_info_t* x, FILE* fs);

int 
tms_profile_info_fread(tms_profile_info_t** x, uint32_t* num_prof,FILE* fs);

int 
tms_profile_info_fprint(uint32_t num_prof,tms_profile_info_t* x, FILE* fs);

void 
tms_profile_info_free(tms_profile_info_t** x);

//***************************************************************************
// hpcrun_fmt_sparse_metrics_t related, defined in hpcrun-fmt.h
//***************************************************************************
const int TMS_ctx_id_SIZE   = 4;
const int TMS_ctx_idx_SIZE  = 8;
const int TMS_ctx_pair_SIZE = TMS_ctx_id_SIZE + TMS_ctx_idx_SIZE;
const int TMS_val_SIZE      = 8;
const int TMS_mid_SIZE      = 2;


int
tms_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
tms_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
tms_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const char* pre, bool easygrep);

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