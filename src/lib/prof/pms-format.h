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
//   Low-level types and functions for reading/writing profile.db
//
//   See profile.db figure.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCTOOLKIT_PROF_PMS_FORMAT_H
#define HPCTOOLKIT_PROF_PMS_FORMAT_H

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

#define MULTIPLE_8(v) ((v + 7) & ~7)
//***************************************************************************
// hdr
//***************************************************************************
#define HPCPROFILESPARSE_FMT_Magic   "HPCPROF-profdb__" //16 bytes
#define HPCPROFILESPARSE_FMT_VersionMajor 1             //1  byte
#define HPCPROFILESPARSE_FMT_VersionMinor 0             //1  byte
#define HPCPROFILESPARSE_FMT_NumSec       2             //2  byte

#define HPCPROFILESPARSE_FMT_MagicLen     (sizeof(HPCPROFILESPARSE_FMT_Magic) - 1)
#define HPCPROFILESPARSE_FMT_VersionLen   2
#define HPCPROFILESPARSE_FMT_NumProfLen   4
#define HPCPROFILESPARSE_FMT_NumSecLen    2
#define HPCPROFILESPARSE_FMT_SecSizeLen   8
#define HPCPROFILESPARSE_FMT_SecPtrLen    8
#define HPCPROFILESPARSE_FMT_SecLen       (HPCPROFILESPARSE_FMT_SecSizeLen + HPCPROFILESPARSE_FMT_SecPtrLen)


#define PMS_real_hdr_SIZE (HPCPROFILESPARSE_FMT_MagicLen + HPCPROFILESPARSE_FMT_VersionLen \
  + HPCPROFILESPARSE_FMT_NumProfLen + HPCPROFILESPARSE_FMT_NumSecLen \
  + HPCPROFILESPARSE_FMT_SecLen * HPCPROFILESPARSE_FMT_NumSec)
#define PMS_hdr_SIZE 128 //starting position of profile info section

typedef struct pms_hdr_t{
  uint8_t versionMajor;
  uint8_t versionMinor;
  uint32_t num_prof;
  uint16_t num_sec; //number of sections include hdr and sparse metrics section

  uint64_t prof_info_sec_size;
  uint64_t prof_info_sec_ptr;
  uint64_t id_tuples_sec_size;
  uint64_t id_tuples_sec_ptr;
}pms_hdr_t;

int 
pms_hdr_fwrite(pms_hdr_t* hdr,FILE* fs);

int
pms_hdr_fread(pms_hdr_t* hdr, FILE* infs);

int
pms_hdr_fprint(pms_hdr_t* hdr, FILE* fs);


//***************************************************************************
// pms_profile_info_t
//***************************************************************************
#define PMS_fake_id_tuple_SIZE   2 //length = 0

#define PMS_num_val_SIZE         8
#define PMS_num_nzctx_SIZE       4
#define PMS_prof_offset_SIZE     8
#define PMS_id_tuple_ptr_SIZE    8
#define PMS_metadata_ptr_SIZE    8
#define PMS_spare_one_SIZE       8
#define PMS_spare_two_SIZE       8
#define PMS_ptrs_SIZE            (PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE + PMS_spare_one_SIZE + PMS_spare_two_SIZE)
//bytes to skip when we only want val_mids and ctx_id_idxs
#define PMS_prof_skip_SIZE       (PMS_fake_id_tuple_SIZE + PMS_num_val_SIZE + PMS_num_nzctx_SIZE) 
#define PMS_prof_info_SIZE       (PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE + PMS_spare_one_SIZE \
  + PMS_spare_two_SIZE + PMS_num_val_SIZE + PMS_num_nzctx_SIZE + PMS_prof_offset_SIZE)


typedef struct pms_profile_info_t{
  uint32_t prof_info_idx; //won't be written in profile.db

  uint64_t num_vals;
  uint32_t num_nzctxs;
  uint64_t offset;

  uint64_t id_tuple_ptr;
  uint64_t metadata_ptr; 
  uint64_t spare_one;
  uint64_t spare_two;
}pms_profile_info_t;


int 
pms_profile_info_fwrite(uint32_t num_t,pms_profile_info_t* x, FILE* fs);

int 
pms_profile_info_fread(pms_profile_info_t** x, uint32_t num_prof,FILE* fs);

int 
pms_profile_info_fprint(uint32_t num_prof,pms_profile_info_t* x, FILE* fs);

void 
pms_profile_info_free(pms_profile_info_t** x);

//***************************************************************************
// id tuple
//***************************************************************************
typedef struct pms_id_tuple_t{
  id_tuple_t idtuple;

  uint32_t rank; //rank that read/write this profile
  uint32_t prof_info_idx;
  uint32_t all_at_root_idx;
}pms_id_tuple_t;

//***************************************************************************
// hpcrun_fmt_sparse_metrics_t related, defined in hpcrun-fmt.h
//***************************************************************************
#define PMS_ctx_id_SIZE    4
#define PMS_ctx_idx_SIZE   8
#define PMS_ctx_pair_SIZE  (PMS_ctx_id_SIZE + PMS_ctx_idx_SIZE)
#define PMS_val_SIZE       8
#define PMS_mid_SIZE       2
#define PMS_vm_pair_SIZE   (PMS_val_SIZE + PMS_mid_SIZE)


int
pms_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
pms_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs);

int 
pms_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const int prof_info_idx, const char* pre, bool easygrep);

int
pms_sparse_metrics_fprint_grep_helper(hpcrun_fmt_sparse_metrics_t* x, FILE* fs, 
          const metric_tbl_t* metricTbl, const char* pre);

void 
pms_sparse_metrics_free(hpcrun_fmt_sparse_metrics_t* x);


//***************************************************************************
// footer
//***************************************************************************
#define PROFDBft 0x50524f4644426674


//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //HPCTOOLKIT_PROF_PMS_FORMAT_H