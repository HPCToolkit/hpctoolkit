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
//   Low-level types and functions for reading/writing cct_major_sparse.db
//
//   See cct_major_sparse figure. //TODO change this
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CMS_FORMAT_H
#define CMS_FORMAT_H

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
// cms_cct_info_t
//***************************************************************************
const int CMS_num_cct_SIZE      = 4;
const int CMS_cct_id_SIZE       = 4;
const int CMS_num_val_SIZE      = 8;
const int CMS_num_nzmid_SIZE    = 2;
const int CMS_cct_offset_SIZE   = 8;
const int CMS_cct_info_SIZE     = CMS_cct_id_SIZE + CMS_num_val_SIZE + CMS_num_nzmid_SIZE + CMS_cct_offset_SIZE;

typedef struct cms_cct_info_t{
  uint32_t cct_id;
  uint64_t num_val;
  uint16_t num_nzmid;
  uint64_t offset;
}cms_cct_info_t;

int
cms_cct_info_fwrite(cms_cct_info_t* x, uint32_t num_cct, FILE* fs);

int 
cms_cct_info_fread(cms_cct_info_t** x, uint32_t* num_cct,FILE* fs);

int 
cms_cct_info_fprint(uint32_t num_cct, cms_cct_info_t* x, FILE* fs);

void 
cms_cct_info_free(cms_cct_info_t** x);

//***************************************************************************
// cct_sparse_metrics_t
//***************************************************************************
const int CMS_mid_SIZE          = 2;
const int CMS_m_offset_SIZE     = 8;
const int CMS_m_pair_SIZE       = CMS_mid_SIZE + CMS_m_offset_SIZE;
const int CMS_val_SIZE          = 8;
const int CMS_tid_SIZE          = 4;
const int CMS_val_tid_pair_SIZE = CMS_val_SIZE + CMS_tid_SIZE;

typedef struct cct_sparse_metrics_t{
  uint32_t cct_node_id;

  uint64_t num_vals;
  uint16_t num_nzmid;
  hpcrun_metricVal_t* values;
  uint32_t* tids; 
  uint16_t* mids;
  uint64_t* m_offsets;
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
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //CMS_FORMAT_H