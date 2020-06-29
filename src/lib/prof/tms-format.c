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

//************************* System Include Files ****************************

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "../prof-lean/hpcio.h"
#include "../prof-lean/hpcio-buffer.h"
#include "../prof-lean/hpcfmt.h"
#include "../prof-lean/hpcrun-fmt.h"
#include "tms-format.h"

//***************************************************************************


//***************************************************************************
// tms_profile_info_t
//***************************************************************************

int
tms_profile_info_fwrite(uint32_t num_t,tms_profile_info_t* x, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(num_t, fs));

  for (uint i = 0; i < num_t; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x[i].tid, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].num_vals, fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x[i].num_nzctxs, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].offset, fs));
  }
  return HPCFMT_OK;
}

int
tms_profile_info_fread(tms_profile_info_t** x, uint32_t* num_t,FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(num_t, fs));

  tms_profile_info_t * profile_infos = (tms_profile_info_t *) malloc((*num_t)*sizeof(tms_profile_info_t));

  for (uint i = 0; i < *num_t; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(profile_infos[i].tid), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].num_vals), fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(profile_infos[i].num_nzctxs), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].offset), fs));
  }

  *x = profile_infos;
  return HPCFMT_OK;
}

int
tms_profile_info_fprint(uint32_t num_thread, tms_profile_info_t* x, FILE* fs)
{
  fprintf(fs,"[Profile informations for %d profiles\n", num_thread);

  for (uint i = 0; i < num_thread; ++i) {
    fprintf(fs,"  [(thread id: %d) (num_vals: %ld) (num_nzctxs: %d) (starting location: %ld)]\n", 
      x[i].tid, x[i].num_vals, x[i].num_nzctxs,x[i].offset);
  }
  fprintf(fs,"]\n");
  return HPCFMT_OK;
}

void
tms_profile_info_free(tms_profile_info_t** x)
{
  free(*x);
  *x = NULL;
}

//***************************************************************************
// hpcrun_fmt_sparse_metrics_t related, defined in hpcrun-fmt.h
//***************************************************************************
int
tms_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x, FILE* fs)
{
  for (uint i = 0; i < x->num_vals; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->values[i].bits, fs));
    HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x->mids[i], fs));
  }

  for (uint i = 0; i < x->num_nz_cct_nodes + 1; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->cct_node_ids[i], fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->cct_node_idxs[i], fs));
  }

  return HPCFMT_OK;
}

int
tms_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs)
{
  x->values = (hpcrun_metricVal_t *) malloc((x->num_vals)*sizeof(hpcrun_metricVal_t));
  x->mids = (uint16_t *) malloc((x->num_vals)*sizeof(uint16_t));
  for (uint i = 0; i < x->num_vals; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->values[i].bits), fs));
    HPCFMT_ThrowIfError(hpcfmt_int2_fread(&x->mids[i], fs));
  }

  x->cct_node_ids = (uint32_t *) malloc((x->num_nz_cct_nodes + 1)*sizeof(uint32_t));
  x->cct_node_idxs = (uint64_t *) malloc((x->num_nz_cct_nodes + 1)*sizeof(uint64_t));
  for (uint i = 0; i < x->num_nz_cct_nodes + 1; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&x->cct_node_ids[i], fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->cct_node_idxs[i], fs));
  }

  return HPCFMT_OK;
}


int
tms_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const char* pre, bool easygrep)
{
  const char* double_pre = "    ";

  fprintf(fs, "[thread %d\n", x->tid);

  if(easygrep){
    tms_sparse_metrics_fprint_grep_helper(x, fs, metricTbl, pre);
  }else{
    fprintf(fs, "%s[metrics:\n%s(NOTES: printed in file order, help checking if the file is correct)\n", pre, pre);
    for (uint i = 0; i < x->num_vals; ++i) {
      fprintf(fs, "%s(value:", double_pre);
      hpcrun_metricFlags_t mflags = hpcrun_metricFlags_NULL;
      if (metricTbl) {
        int metric_id = x->mids[i];
        const metric_desc_t* mdesc = &(metricTbl->lst[metric_id]);
        mflags = mdesc->flags;
      }

      switch (mflags.fields.valFmt) {
        default: //used for printing tmp sparse-db files, it does not have metric_tbl included
          fprintf(fs, " %g", x->values[i].r);
          break;
        case MetricFlags_ValFmt_Int:
          fprintf(fs, " %"PRIu64, x->values[i].i);
          break;
        case MetricFlags_ValFmt_Real:
          fprintf(fs, " %g", x->values[i].r);
          break;
      }
      fprintf(fs, ", metric id: %d)\n", x->mids[i]);
    }
    fprintf(fs, "%s]\n", pre);
  }
  
  fprintf(fs, "%s[ctx indices:\n",pre);
  for (uint i = 0; i < x->num_nz_cct_nodes + 1; ++i) {
    if(i == x->num_nz_cct_nodes && x->cct_node_ids[i] == LastNodeEnd){
      fprintf(fs, "%s(ctx id: END, index: %ld)\n%s]\n", double_pre, x->cct_node_idxs[i], pre);
    }else{
      fprintf(fs, "%s(ctx id: %d, index: %ld)\n", double_pre, x->cct_node_ids[i], x->cct_node_idxs[i]);
    } 
  }

  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


int
tms_sparse_metrics_fprint_grep_helper(hpcrun_fmt_sparse_metrics_t* x, FILE* fs, 
          const metric_tbl_t* metricTbl, const char* pre)
{
  const char* double_pre = "    ";

  fprintf(fs, "%s[metrics easy grep version:\n%s(NOTES: metrics for a cct node are printed together, easy to grep)\n", pre, pre);
  for (uint i = 0; i < x->num_nz_cct_nodes; ++i) {
    uint16_t cct_node_id = x->cct_node_ids[i];
    uint64_t cct_off = x->cct_node_idxs[i];
    uint64_t cct_next_off = x->cct_node_idxs[i+1];

    fprintf(fs, "%s(ctx id: %d) ", double_pre, cct_node_id);

    for(uint j = cct_off; j < cct_next_off; j++){
      fprintf(fs, "(value:"); 

      hpcrun_metricFlags_t mflags = hpcrun_metricFlags_NULL;
      if (metricTbl) {
        int metric_id = x->mids[j];
        const metric_desc_t* mdesc = &(metricTbl->lst[metric_id]);
        mflags = mdesc->flags;
      }

      switch (mflags.fields.valFmt) {
        default: //used for printing tmp sparse-db files, it does not have metric_tbl included
          fprintf(fs, " %g", x->values[j].r);
          break;
        case MetricFlags_ValFmt_Int:
          fprintf(fs, " %"PRIu64, x->values[j].i);
          break;
        case MetricFlags_ValFmt_Real:
          fprintf(fs, " %g", x->values[j].r);
          break;
      }
      fprintf(fs, ", metric id: %d) ", x->mids[j]);
    } //END of value & metric_id pairs for one cct node
    fprintf(fs, "\n");

  }//END of value & metric_id pairs for ALL cct node
  fprintf(fs, "%s]\n", pre);

  return HPCFMT_OK;
}


void
tms_sparse_metrics_free(hpcrun_fmt_sparse_metrics_t* x)
{
  free(x->values);
  free(x->mids);
  free(x->cct_node_ids);
  free(x->cct_node_idxs);

  x->values = NULL;
  x->mids   = NULL;
  x->cct_node_ids = NULL;
  x->cct_node_idxs= NULL;
}
