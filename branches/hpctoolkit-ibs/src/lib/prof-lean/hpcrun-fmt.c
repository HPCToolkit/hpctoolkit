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

//************************* System Include Files ****************************

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include "hpcio.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"


//***************************************************************************
// hdr
//***************************************************************************

int
hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t* hdr, FILE* infs, hpcfmt_alloc_fn alloc)
{
  char tag[HPCRUN_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCRUN_FMT_MagicLen, infs);
  tag[HPCRUN_FMT_MagicLen] = '\0';

  if (nr != HPCRUN_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCRUN_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(hdr->version, 1, HPCRUN_FMT_VersionLen, infs);
  hdr->version[HPCRUN_FMT_VersionLen] = '\0';
  if (nr != HPCRUN_FMT_VersionLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(hdr->version, HPCRUN_FMT_Version) != 0) { // TODO: numeric test
    // TODO: set error message
  }

  nr = fread(&hdr->endian, 1, HPCRUN_FMT_EndianLen, infs);
  if (nr != HPCRUN_FMT_EndianLen) {
    return HPCFMT_ERR;
  }

  hpcfmt_nvpair_list_fread(&(hdr->nvps), infs, alloc);

  return HPCFMT_OK;
}


int
hpcrun_fmt_hdr_fwrite(FILE* fs, ...)
{
  va_list args;
  va_start(args, fs);

  fwrite(HPCRUN_FMT_Magic,   1, HPCRUN_FMT_MagicLen, fs);
  fwrite(HPCRUN_FMT_Version, 1, HPCRUN_FMT_VersionLen, fs);
  fwrite(HPCRUN_FMT_Endian,  1, HPCRUN_FMT_EndianLen, fs);

  hpcfmt_nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFMT_OK;
}


int
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %s)\n", hdr->version);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  hpcfmt_nvpair_list_fprint(&hdr->nvps, fs, "  ");
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_hdr_free(hpcrun_fmt_hdr_t* hdr, hpcfmt_free_fn dealloc)
{
  if (dealloc) {
    hpcfmt_nvpair_free(&(hdr->nvps), dealloc);
  }
}


//***************************************************************************
// epoch-hdr
//***************************************************************************

int
hpcrun_fmt_epoch_hdr_fread(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs, 
			   hpcfmt_alloc_fn alloc)
{
  char tag[HPCRUN_FMT_EpochTagLen + 1];

  int nr = fread(tag, 1, HPCRUN_FMT_EpochTagLen, fs);
  tag[HPCRUN_FMT_EpochTagLen] = '\0';
  
  if (nr == 0) {
    return HPCFMT_EOF;
  }
  else if (nr != HPCRUN_FMT_EpochTagLen) {
    return HPCFMT_ERR;
  }

  if (strcmp(tag, HPCRUN_FMT_EpochTag) != 0) {
    //fprintf(stderr,"invalid epoch header tag: %s\n", tag);
    return HPCFMT_ERR;
  }

  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(ehdr->flags.bits), fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(ehdr->measurementGranularity), fs));
  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&(ehdr->raToCallsiteOfst), fs));
  HPCFMT_ThrowIfError(hpcfmt_nvpair_list_fread(&(ehdr->nvps), fs, alloc));

  return HPCFMT_OK;
}


int
hpcrun_fmt_epoch_hdr_fwrite(FILE* fs, epoch_flags_t flags,
			    uint64_t measurementGranularity, 
			    uint32_t raToCallsiteOfst, ...)
{
  va_list args;
  va_start(args, raToCallsiteOfst);

  fwrite(HPCRUN_FMT_EpochTag, 1, HPCRUN_FMT_EpochTagLen, fs);

  hpcfmt_byte8_fwrite(flags.bits, fs);
  hpcfmt_byte8_fwrite(measurementGranularity, fs);
  hpcfmt_byte4_fwrite(raToCallsiteOfst, fs);

  hpcfmt_nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFMT_OK;
}


int
hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_EpochTag);
  fprintf(fs, "[epoch-hdr:\n");
  fprintf(fs, "  (flags: %"PRIx64")\n", ehdr->flags.bits);
  fprintf(fs, "  (measurement-granularity: %"PRIu64")\n", 
	  ehdr->measurementGranularity);
  fprintf(fs, "  (RA-to-callsite-offset: %"PRIu32")\n", ehdr->raToCallsiteOfst);
  hpcfmt_nvpair_list_fprint(&(ehdr->nvps), fs, "  ");
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_epoch_hdr_free(hpcrun_fmt_epoch_hdr_t* ehdr, hpcfmt_free_fn dealloc)
{
  if (dealloc) {
    hpcfmt_nvpair_free(&(ehdr->nvps), dealloc);
  }
}


//***************************************************************************
// metric-tbl
//***************************************************************************

hpcrun_metricVal_t hpcrun_metricVal_ZERO = { .bits = 0 };

//***************************************************************************

int
hpcrun_fmt_metricTbl_fread(metric_tbl_t* metric_tbl, FILE* fs, 
			    hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&(metric_tbl->len), fs));
  if (alloc) {
    metric_tbl->lst = 
      (metric_desc_t*) alloc(metric_tbl->len * sizeof(metric_desc_t));
  }

  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_metricDesc_fread(x, fs, alloc));
  }
  
  return HPCFMT_OK;
}


//
// metric list ==> written out as metric tbl
//
int
hpcrun_fmt_metricTbl_fwrite(metric_desc_p_tbl_t* metric_tbl, FILE* fs)
{
  hpcfmt_byte4_fwrite(metric_tbl->len, fs);
  for (uint32_t i = 0; i < metric_tbl->len; i++){
    hpcrun_fmt_metricDesc_fwrite(metric_tbl->lst[i], fs);
  }

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricTbl_fprint(metric_tbl_t* metric_tbl, FILE* fs)
{
  fprintf(fs, "[metric-tbl: (num-entries: %u)\n", metric_tbl->len);
  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    hpcrun_fmt_metricDesc_fprint(x, fs, "  ");
  }
  fputs("]\n", fs);
  
  return HPCFMT_OK;
}


void
hpcrun_fmt_metricTbl_free(metric_tbl_t* metric_tbl, hpcfmt_free_fn dealloc)
{
  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    hpcrun_fmt_metricDesc_free(x, dealloc);
  }
  dealloc((void*)metric_tbl->lst);
  metric_tbl->lst = NULL;
}


//***************************************************************************

int
hpcrun_fmt_metricDesc_fread(metric_desc_t* x, FILE* fs, hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->name), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->description), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(x->flags), fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(x->period), fs));

  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&(x->fmt_flag), fs));
  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fwrite(metric_desc_t* x, FILE* fs)
{
  hpcfmt_str_fwrite(x->name, fs);
  hpcfmt_str_fwrite(x->description, fs);
  hpcfmt_byte8_fwrite(x->flags, fs);
  hpcfmt_byte8_fwrite(x->period, fs);
  
  hpcfmt_byte4_fwrite(x->fmt_flag, fs);

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fprint(metric_desc_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s[(nm: %s) (desc: %s) (flg: 0x%"PRIx64") (period: %"PRIu64")]\n", pre, x->name, x->description, x->flags, x->period);
  return HPCFMT_OK;
}


void
hpcrun_fmt_metricDesc_free(metric_desc_t* x, hpcfmt_free_fn dealloc)
{
  hpcfmt_str_free(x->name, dealloc);
  hpcfmt_str_free(x->description, dealloc);
  x->name = NULL;
  x->description = NULL;
}


//***************************************************************************
// loadmap
//***************************************************************************

int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* fs, hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&(loadmap->len), fs));
  if (alloc) {
    loadmap->lst = alloc(loadmap->len * sizeof(loadmap_entry_t));
  }

  for (uint32_t i = 0; i < loadmap->len; i++) {
    loadmap_entry_t* e = &loadmap->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_loadmapEntry_fread(e, fs, alloc));
  }

  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmap_fwrite(loadmap_t* loadmap, FILE* fs)
{
  hpcfmt_byte4_fwrite(loadmap->len, fs);
  for (uint32_t i = 0; i < loadmap->len; i++) {
    loadmap_entry_t* e = &loadmap->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_loadmapEntry_fwrite(e, fs));
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmap_fprint(loadmap_t* loadmap, FILE* fs)
{
  fprintf(fs, "[loadmap: (num-entries: %u)\n", loadmap->len);
  for (uint32_t i = 0; i < loadmap->len; i++) {
    loadmap_entry_t* e = &loadmap->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_loadmapEntry_fprint(e, fs, "  "));
  }
  fprintf(fs, "]\n");
  
  return HPCFMT_OK;
}


void
hpcrun_fmt_loadmap_free(loadmap_t* loadmap, hpcfmt_free_fn dealloc)
{
  for (uint32_t i = 0; i < loadmap->len; i++) {
    loadmap_entry_t* e = &loadmap->lst[i];
    hpcrun_fmt_loadmapEntry_free(e, dealloc);
  }
  dealloc(loadmap->lst);
  loadmap->lst = NULL;
}


//***************************************************************************

int
hpcrun_fmt_loadmapEntry_fread(loadmap_entry_t* x, FILE* fs, 
			      hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_byte2_fread(&(x->id), fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->name), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(x->vaddr), fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(x->mapaddr), fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&(x->flags), fs));
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmapEntry_fwrite(loadmap_entry_t* x, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_byte2_fwrite(x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->name, fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fwrite(x->vaddr, fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fwrite(x->mapaddr, fs));
  HPCFMT_ThrowIfError(hpcfmt_byte8_fwrite(x->flags, fs));
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmapEntry_fprint(loadmap_entry_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s[(id: %"PRIu16") (nm: %s) (0x%"PRIx64" 0x%"PRIx64") (flg: 0x%"PRIx64")]\n",
	  pre, x->id, x->name, x->vaddr, x->mapaddr, x->flags);
  return HPCFMT_OK;
}


void
hpcrun_fmt_loadmapEntry_free(loadmap_entry_t* x, hpcfmt_free_fn dealloc)
{
  hpcfmt_str_free(x->name, dealloc);
  x->name = NULL;
}


//***************************************************************************
// cct
//***************************************************************************

int 
hpcrun_fmt_cct_node_fread(hpcrun_fmt_cct_node_t* x, 
			  epoch_flags_t flags, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&x->id_parent, fs));

  x->as_info = lush_assoc_info_NULL;
  if (flags.flags.isLogicalUnwind) {
    HPCFMT_ThrowIfError(hpcfmt_byte4_fread(&x->as_info.bits, fs));
  }

  HPCFMT_ThrowIfError(hpcfmt_byte2_fread(&x->lm_id, fs));

  HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&x->ip, fs));

  lush_lip_init(&x->lip);
  if (flags.flags.isLogicalUnwind) {
    hpcrun_fmt_lip_fread(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&x->metrics[i].bits, fs));
  }
  
  return HPCFMT_OK;
}


int 
hpcrun_fmt_cct_node_fwrite(hpcrun_fmt_cct_node_t* x, 
			   epoch_flags_t flags, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_byte4_fwrite(x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_byte4_fwrite(x->id_parent, fs));

  if (flags.flags.isLogicalUnwind) {
    hpcfmt_byte4_fwrite(x->as_info.bits, fs);
  }

  hpcfmt_byte2_fwrite(x->lm_id, fs);

  hpcfmt_byte8_fwrite(x->ip, fs);

  if (flags.flags.isLogicalUnwind) {
    hpcrun_fmt_lip_fwrite(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfmt_byte8_fwrite(x->metrics[i].bits, fs);
  }
  
  return HPCFMT_OK;
}


int 
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs, 
			   epoch_flags_t flags, const char* pre)
{
  fprintf(fs, "%s[node: (id: %d) (id-parent: %d) ",
	  pre, x->id, x->id_parent);

  if (flags.flags.isLogicalUnwind) {
    char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
    lush_assoc_info_sprintf(as_str, x->as_info);

    fprintf(fs, "(as: %s) ", as_str);
  }

  fprintf(fs, "(lm-id: %"PRIu16") (ip: 0x%"PRIx64") ", x->lm_id, x->ip);

  if (flags.flags.isLogicalUnwind) {
    hpcrun_fmt_lip_fprint(&x->lip, fs, "");
  }

  fprintf(fs, "\n");

  fprintf(fs, "%s(metrics:", pre);
  for (int i = 0; i < x->num_metrics; ++i) {
    fprintf(fs, " %"PRIu64, x->metrics[i].bits);
    if (i + 1 < x->num_metrics) {
      fprintf(fs, " ");
    }
  }
  fprintf(fs, ")\n");

  fprintf(fs, "%s]\n", pre);
  
  return HPCFMT_OK;
}


//***************************************************************************

int 
hpcrun_fmt_lip_fread(lush_lip_t* x, FILE* fs)
{
  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_byte8_fread(&x->data8[i], fs));
  }
  
  return HPCFMT_OK;
}


int 
hpcrun_fmt_lip_fwrite(lush_lip_t* x, FILE* fs)
{
  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    hpcfmt_byte8_fwrite(x->data8[i], fs);
  }
  
  return HPCFMT_OK;
}


int 
hpcrun_fmt_lip_fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  char lip_str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(lip_str, x);

  fprintf(fs, "%s(lip: %s)", pre, lip_str);

  return HPCFMT_OK;
}

//***************************************************************************
