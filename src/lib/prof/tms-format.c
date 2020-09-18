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
//   See thread.db figure. //TODO change this
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
// hdr
//***************************************************************************
int 
tms_hdr_fwrite(FILE* fs)
{
  fwrite(HPCTHREADSPARSE_FMT_Magic,   1, HPCTHREADSPARSE_FMT_MagicLen,   fs);
  int version = HPCTHREADSPARSE_FMT_Version;
  fwrite(&version, 1, HPCTHREADSPARSE_FMT_VersionLen, fs);
  return HPCFMT_OK;
}

int
tms_hdr_fread(tms_hdr_t* hdr, FILE* infs)
{
  char tag[HPCTHREADSPARSE_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCTHREADSPARSE_FMT_MagicLen, infs);
  tag[HPCTHREADSPARSE_FMT_MagicLen] = '\0';

  if (nr != HPCTHREADSPARSE_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCTHREADSPARSE_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(&hdr->version, 1, HPCTHREADSPARSE_FMT_VersionLen, infs);
  if (nr != HPCTHREADSPARSE_FMT_VersionLen) {
    return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}

int
tms_hdr_fprint(tms_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCTHREADSPARSE_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %d)\n", hdr->version);
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}

#if 0
//***************************************************************************
// id tuple
//***************************************************************************
char* kindStr(const uint16_t kind)
{
  if(kind == IDTUPLE_SUMMARY){
    return "SUMMARY";
  }
  else if(kind == IDTUPLE_RANK){
    return "RANK";
  }
  else if(kind == IDTUPLE_THREAD){
    return "THREAD";
  }
  else{
    return "ERROR";
  }
}


int
id_tuples_tms_fwrite(uint32_t num_tuples,id_tuple_t* x, FILE* fs)
{
  for (uint i = 0; i < num_tuples; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x[i].length, fs));
    for (uint j = 0; j < x[i].length; ++j) {
      HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x[i].ids[j].kind, fs));
      HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].ids[j].index, fs));
    }
  }
  return HPCFMT_OK;
}

int
id_tuples_tms_fread(id_tuple_t** x, uint32_t num_tuples,FILE* fs)
{
  id_tuple_t * id_tuples = (id_tuple_t *) malloc(num_tuples*sizeof(id_tuple_t));

  for (uint i = 0; i < num_tuples; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(id_tuples[i].length), fs));
    id_tuples[i].ids = (tms_id_t *) malloc(id_tuples[i].length * sizeof(tms_id_t)); 
    for (uint j = 0; j < id_tuples[i].length; ++j) {
      HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(id_tuples[i].ids[j].kind), fs));
      HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(id_tuples[i].ids[j].index), fs));
    }
  }

  *x = id_tuples;
  return HPCFMT_OK;
}

int
id_tuples_tms_fprint(uint32_t num_tuples, id_tuple_t* x, FILE* fs)
{
  fprintf(fs,"[Id tuples for %d profiles\n", num_tuples);

  for (uint i = 0; i < num_tuples; ++i) {
    fprintf(fs,"  %d[", i);
    for (uint j = 0; j < x[i].length; ++j) {
      fprintf(fs,"(%s: %ld) ", kindStr(x[i].ids[j].kind), x[i].ids[j].index);
    }
    fprintf(fs,"]\n");
  }
  fprintf(fs,"]\n");
  return HPCFMT_OK;
}

void
id_tuples_tms_free(id_tuple_t** x, uint32_t num_tuples)
{
  for (uint i = 0; i < num_tuples; ++i) {
    free((*x)[i].ids);
    (*x)[i].ids = NULL;
  }

  free(*x);
  *x = NULL;
}
#endif


//***************************************************************************
// tms_profile_info_t
//***************************************************************************

int
tms_profile_info_fwrite(uint32_t num_t,tms_profile_info_t* x, FILE* fs)
{
  for (uint i = 0; i < num_t; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].id_tuple_ptr, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].metadata_ptr, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].spare_one, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].spare_two, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].num_vals, fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x[i].num_nzctxs, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x[i].offset, fs));
  }
  return HPCFMT_OK;
}

int
tms_profile_info_fread(tms_profile_info_t** x, uint32_t num_prof,FILE* fs)
{

  tms_profile_info_t * profile_infos = (tms_profile_info_t *) malloc(num_prof*sizeof(tms_profile_info_t));

  for (uint i = 0; i < num_prof; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].id_tuple_ptr), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].metadata_ptr), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].spare_one), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].spare_two), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].num_vals), fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(profile_infos[i].num_nzctxs), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(profile_infos[i].offset), fs));
    profile_infos[i].prof_info_idx = i;
  }

  *x = profile_infos;
  return HPCFMT_OK;
}

int
tms_profile_info_fprint(uint32_t num_prof, tms_profile_info_t* x, FILE* fs)
{
  fprintf(fs,"[Profile informations for %d profiles\n", num_prof);

  for (uint i = 0; i < num_prof; ++i) {
    fprintf(fs,"  %d[(id_tuple_ptr: %ld) (metadata_ptr: %ld) (spare_one: %ld) (spare_two: %ld) (num_vals: %ld) (num_nzctxs: %d) (starting location: %ld)]\n", 
      i, x[i].id_tuple_ptr, x[i].metadata_ptr, x[i].spare_one, x[i].spare_two, x[i].num_vals, x[i].num_nzctxs,x[i].offset);
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
          const metric_tbl_t* metricTbl, const int prof_info_idx, const char* pre, bool easygrep)
{
  const char* double_pre = "    ";

  fprintf(fs, "[profile %d\n", prof_info_idx);

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
