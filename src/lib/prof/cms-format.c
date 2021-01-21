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
#include "cms-format.h"

//***************************************************************************

//***************************************************************************
// hdr
//***************************************************************************
int 
cms_hdr_fwrite(cms_hdr_t* hdr,FILE* fs)
{
  fwrite(HPCCCTSPARSE_FMT_Magic,   1, HPCCCTSPARSE_FMT_MagicLen,   fs);
  uint8_t versionMajor = HPCCCTSPARSE_FMT_VersionMajor;
  uint8_t versionMinor = HPCCCTSPARSE_FMT_VersionMinor;
  fwrite(&versionMajor, 1, 1, fs);
  fwrite(&versionMinor, 1, 1, fs);
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(hdr->num_ctx, fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(hdr->num_sec, fs));

  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->ctx_info_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->ctx_info_sec_ptr, fs));
  
  return HPCFMT_OK;
}

int
cms_hdr_fread(cms_hdr_t* hdr, FILE* infs)
{
  char tag[HPCCCTSPARSE_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCCCTSPARSE_FMT_MagicLen, infs);
  tag[HPCCCTSPARSE_FMT_MagicLen] = '\0';

  if (nr != HPCCCTSPARSE_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCCCTSPARSE_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(&hdr->versionMajor, 1, 1, infs);
  if (nr != 1) {
    return HPCFMT_ERR;
  }

  nr = fread(&hdr->versionMinor, 1, 1, infs);
  if (nr != 1) {
    return HPCFMT_ERR;
  }

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(hdr->num_ctx), infs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(hdr->num_sec), infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->ctx_info_sec_size), infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->ctx_info_sec_ptr), infs));

  return HPCFMT_OK;
}

int
cms_hdr_fprint(cms_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCCCTSPARSE_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %d.%d)\n", hdr->versionMajor, hdr->versionMinor);
  fprintf(fs, "  (num_ctx: %d)\n", hdr->num_ctx);
  fprintf(fs, "  (num_sec: %d)\n", hdr->num_sec);
  fprintf(fs, "  (ctx_info_sec_size: %ld)\n", hdr->ctx_info_sec_size);
  fprintf(fs, "  (ctx_info_sec_ptr: %ld)\n", hdr->ctx_info_sec_ptr);
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}

//***************************************************************************
// cms_ctx_info_t
//***************************************************************************
int
cms_ctx_info_fwrite(cms_ctx_info_t* x, uint32_t num_ctx, FILE* fs)
{
  for (uint i = 0; i < num_ctx; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x[i].ctx_id, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].num_vals, fs));
    HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x[i].num_nzmids, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].offset, fs));
  }

  return HPCFMT_OK;
}


int
cms_ctx_info_fread(cms_ctx_info_t** x, uint32_t num_ctx,FILE* fs)
{
  cms_ctx_info_t * cct_infos = (cms_ctx_info_t *) malloc(num_ctx*sizeof(cms_ctx_info_t));

  for (uint i = 0; i < num_ctx; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(cct_infos[i].ctx_id), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(cct_infos[i].num_vals), fs));
    HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(cct_infos[i].num_nzmids), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(cct_infos[i].offset), fs));
  }

  *x = cct_infos;
  return HPCFMT_OK;
}


int
cms_ctx_info_fprint(uint32_t num_ctx, cms_ctx_info_t* x, FILE* fs)
{
  fprintf(fs,"[Context informations for %d Contexts\n", num_ctx);

  for (uint i = 0; i < num_ctx; ++i) {
    fprintf(fs,"  [(context id: %d) (num_vals: %ld) (num_nzmids: %d) (starting location: %ld)]\n", 
      x[i].ctx_id, x[i].num_vals, x[i].num_nzmids, x[i].offset);
  }
  fprintf(fs,"]\n");
  return HPCFMT_OK;
}


void
cms_ctx_info_free(cms_ctx_info_t** x)
{
  free(*x);
  *x = NULL;
}

//***************************************************************************
// cct_sparse_metrics_t
//***************************************************************************
/* assume each cct_sparse_metrics_t already got num_vals, num_nzmids, ctx_id */
int
cms_sparse_metrics_fwrite(cct_sparse_metrics_t* x, FILE* fs)
{
  for (uint i = 0; i < x->num_vals; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->values[i].bits, fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->tids[i], fs));
  }

  for (uint i = 0; i < x->num_nzmids+1; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x->mids[i], fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->m_idxs[i], fs));
  }

  return HPCFMT_OK;

}

/* assume each cct_sparse_metrics_t already got num_vals, num_nzmids, ctx_id */
int
cms_sparse_metrics_fread(cct_sparse_metrics_t* x, FILE* fs)
{
  x->values = (hpcrun_metricVal_t *) malloc((x->num_vals)*sizeof(hpcrun_metricVal_t));
  x->tids   = (uint32_t *) malloc((x->num_vals)*sizeof(uint32_t));
  for (uint i = 0; i < x->num_vals; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->values[i].bits), fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->tids[i]), fs));
  }

  x->mids   = (uint16_t *) malloc((x->num_nzmids+1)*sizeof(uint16_t));
  x->m_idxs = (uint64_t *) malloc((x->num_nzmids+1)*sizeof(uint64_t));
  for (uint i = 0; i < x->num_nzmids+1; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int2_fread(&x->mids[i], fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->m_idxs[i], fs));
  }

  return HPCFMT_OK;

}


int
cms_sparse_metrics_fprint(cct_sparse_metrics_t* x, FILE* fs, const char* pre, bool easygrep)
{
  const char* double_pre = "    ";

  fprintf(fs, "[context %d\n", x->ctx_id);

  if(easygrep){
    cms_sparse_metrics_fprint_grep_helper(x, fs, pre);
  }else{
    fprintf(fs, "%s[metrics:\n%s(NOTES: printed in file order, help checking if the file is correct)\n", pre, pre);
    for (uint i = 0; i < x->num_vals; ++i) {
      fprintf(fs, "%s(value: %g, profile idx: %d)\n", double_pre, x->values[i].r, x->tids[i]); //cct doesn't have metricTbl, so all print as real value for now
    }
    fprintf(fs, "%s]\n", pre);
  }

  fprintf(fs, "%s[metric indices:\n",pre);
  for (uint i = 0; i < x->num_nzmids+1; ++i) {
    if(i == x->num_nzmids && x->mids[i] == LastMidEnd){
      fprintf(fs, "%s(metric id: END, index: %ld)\n%s]\n", double_pre, x->m_idxs[i], pre);
    }else{
      fprintf(fs, "%s(metric id: %d, index: %ld)\n", double_pre, x->mids[i],x->m_idxs[i]);
    } 
  }

  fprintf(fs, "]\n");

  return HPCFMT_OK;
}

int
cms_sparse_metrics_fprint_grep_helper(cct_sparse_metrics_t* x, FILE* fs, const char* pre)
{
  const char* double_pre = "    ";

  fprintf(fs, "%s[metrics easy grep version:\n%s(NOTES: metrics for a metric id are printed together, easy to grep)\n", pre, pre);
  for (uint i = 0; i < x->num_nzmids; ++i) {
    uint16_t mid = x->mids[i];
    uint64_t m_off = x->m_idxs[i];
    uint64_t m_next_off = x->m_idxs[i+1];

    fprintf(fs, "%s(metric id: %d) ", double_pre, mid);

    for(uint j = m_off; j < m_next_off; j++){
      fprintf(fs, "(value: %g, profile idx: %d) ", x->values[j].r, x->tids[j]); 
    }
    fprintf(fs, "\n");
  }
  fprintf(fs, "%s]\n", pre);

  return HPCFMT_OK;
}



void
cms_sparse_metrics_free(cct_sparse_metrics_t* x)
{
  free(x->values);
  free(x->tids);
  free(x->mids);
  free(x->m_idxs);

  x->values    = NULL;
  x->tids      = NULL;
  x->mids      = NULL;
  x->m_idxs = NULL;

}