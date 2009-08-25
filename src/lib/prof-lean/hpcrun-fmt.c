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
// Copyright ((c)) 2002-2009, Rice University 
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
#include "hpcrun-fmt.h"
#include "hpcfmt.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// hdr
//***************************************************************************

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

  nr = fread(&hdr->endian, 1, HPCRUN_FMT_EndianLen, infs);
  if (nr != HPCRUN_FMT_EndianLen) {
    return HPCFMT_ERR;
  }

  hpcfmt_nvpair_list_fread(&(hdr->nvps), infs, alloc);

  return HPCFMT_OK;
}


int
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_Magic);

  fprintf(fs, "{hdr:\n");
  fprintf(fs, "  (version: %s)\n", hdr->version);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  hpcfmt_nvpair_list_fprint(&hdr->nvps, fs, "  ");
  fprintf(fs, "}\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_hdr_free(hpcrun_fmt_hdr_t* hdr, hpcfmt_free_fn dealloc)
{
  if (!dealloc) {
    dealloc(hdr->nvps.lst);
  }
}


//***************************************************************************
// epoch-hdr
//***************************************************************************

int
hpcrun_fmt_epoch_hdr_fwrite(FILE* fs, epoch_flags_t flags,
			    uint32_t ra_distance, uint64_t granularity, ...)
{
  va_list args;
  va_start(args, granularity);

  fwrite(HPCRUN_FMT_EpochTag, 1, HPCRUN_FMT_EpochTagLen, fs);

  hpcfmt_byte8_fwrite(flags.bits, fs);
  hpcfmt_byte4_fwrite(ra_distance, fs);
  hpcfmt_byte8_fwrite(granularity, fs);

  hpcfmt_nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFMT_OK;
}


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

  THROW_IF_HPCERR(hpcfmt_byte8_fread(&(ehdr->flags.bits), fs));
  THROW_IF_HPCERR(hpcfmt_byte4_fread(&(ehdr->ra_distance), fs));
  THROW_IF_HPCERR(hpcfmt_byte8_fread(&(ehdr->granularity), fs));
  THROW_IF_HPCERR(hpcfmt_nvpair_list_fread(&(ehdr->nvps), fs, alloc));

  return HPCFMT_OK;
}


int
hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_EpochTag);
  fprintf(fs, "{epoch-hdr:\n");
  fprintf(fs, "  (flags: %"PRIx64") ", ehdr->flags.bits);
  fprintf(fs, "(RA distance: %d) ", ehdr->ra_distance);
  fprintf(fs, "(address granularity: %"PRIu64")\n", ehdr->granularity);
  hpcfmt_nvpair_list_fprint(&(ehdr->nvps), fs, "  ");
  fprintf(fs, "}\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_epoch_hdr_free(hpcrun_fmt_epoch_hdr_t* ehdr, hpcfmt_free_fn dealloc)
{
  if (!dealloc) {
    dealloc(ehdr->nvps.lst);
  }
}

//***************************************************************************
// metric-tbl
//***************************************************************************

int
hpcrun_fmt_metric_tbl_fwrite(metric_tbl_t* metric_tbl, FILE* fs)
{
  hpcfmt_byte4_fwrite(metric_tbl->len, fs);
  metric_desc_t* m_lst = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; ++i) {
    hpcfmt_str_fwrite(m_lst[i].name, fs);
    hpcfmt_byte8_fwrite(m_lst[i].flags, fs);
    hpcfmt_byte8_fwrite(m_lst[i].period, fs);
  }
  return HPCFMT_OK;
}


int
hpcrun_fmt_metric_tbl_fread(metric_tbl_t* metric_tbl, FILE* in, 
			    hpcfmt_alloc_fn alloc)
{
  THROW_IF_HPCERR(hpcfmt_byte4_fread(&(metric_tbl->len), in));
  if (alloc != NULL) {
    metric_tbl->lst = 
      (metric_desc_t *) alloc(metric_tbl->len * sizeof(metric_desc_t));
  }
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    THROW_IF_HPCERR(hpcfmt_str_fread(&(p->name), in, alloc));
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&(p->flags), in));
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&(p->period), in));
  }
  return HPCFMT_OK;
}


int
hpcrun_fmt_metric_tbl_fprint(metric_tbl_t* metric_tbl, FILE* fs)
{
  fputs("{metric-tbl:\n", fs);

  fprintf(fs, "  metrics: %d\n", metric_tbl->len);
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    fprintf(fs, "  {(event: %s) ", p->name); 
    fprintf(fs, "(period: %"PRIu64")}\n", p->period); 
  }
  fputs("}\n", fs);
  
  return HPCFMT_OK;
}


void
hpcrun_fmt_metric_tbl_free(metric_tbl_t* metric_tbl, hpcfmt_free_fn dealloc)
{
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    hpcfmt_str_free(p->name, dealloc);
  }
  dealloc((void*)metric_tbl->lst);
  metric_tbl->lst = NULL;
}


//***************************************************************************
// loadmap
//***************************************************************************

int
hpcrun_fmt_loadmap_fwrite(uint32_t num_modules, loadmap_src_t* src, FILE* fs)
{
  hpcfmt_byte4_fwrite(num_modules, fs);

  for(int i = 0; i < num_modules; i++) {
    hpcfmt_str_fwrite(src->name, fs);
    hpcfmt_byte8_fwrite((uint64_t)(unsigned long)src->vaddr, fs);
    hpcfmt_byte8_fwrite((uint64_t)(unsigned long)src->mapaddr, fs);
    src = src->next;
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* fs, hpcfmt_alloc_fn alloc)
{
  THROW_IF_HPCERR(hpcfmt_byte4_fread(&(loadmap->len), fs));
  if (alloc) {
    loadmap->lst = alloc(loadmap->len * sizeof(loadmap_entry_t));
  }

  // FIXME: incomplete and hard-to-understand OSR?  Why not index by i?
  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {

    THROW_IF_HPCERR(hpcfmt_str_fread(&(e->name), fs, alloc));
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&(e->vaddr), fs));
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&(e->mapaddr), fs));

    // FIXME ?? (don't know what these are for yet ...
    e->flags = 0;
  }

  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmap_fprint(loadmap_t* loadmap, FILE* fs)
{
  loadmap_entry_t* e = loadmap->lst;
  fprintf(fs, "{loadmap:\n");

  // FIXME: incomplete and hard-to-understand OSR?  Why not index by i?
  for (int i = 0; i < loadmap->len; e++,i++) {
    fprintf(fs, "  lm %d: %s %"PRIx64" -> %"PRIx64"\n",
            i, e->name, e->vaddr, e->mapaddr);
  }
  fprintf(fs, "}\n");
  
  return HPCFMT_OK;
}


void
hpcrun_fmt_loadmap_free(loadmap_t* loadmap, hpcfmt_free_fn dealloc)
{
  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {
    hpcfmt_str_free(e->name, dealloc);
    e->name = NULL;
  }
  dealloc(loadmap->lst);
  loadmap->lst = NULL;
}


//***************************************************************************
// cct
//***************************************************************************

hpcrun_metric_data_t hpcrun_metric_data_ZERO = { .bits = 0 };


int 
hpcfile_cstree_nodedata__init(hpcfile_cstree_nodedata_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFMT_OK;
}


int 
hpcfile_cstree_nodedata__fini(hpcfile_cstree_nodedata_t* x)
{
  return HPCFMT_OK;
}


int 
hpcfile_cstree_nodedata__fread(hpcfile_cstree_nodedata_t* x, 
			       epoch_flags_t flags, FILE* fs)
{
  // ASSUMES: space for metrics has been allocated

  x->as_info = lush_assoc_info_NULL;
  if (flags.flags.lush_active) {
    THROW_IF_HPCERR(hpcfmt_byte4_fread(&x->as_info.bits, fs));
  }

  THROW_IF_HPCERR(hpcfmt_byte8_fread(&x->ip, fs));

  lush_lip_init(&x->lip);
  if (flags.flags.lush_active) {
    hpcfile_cstree_lip__fread(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&x->metrics[i].bits, fs));
  }

  return HPCFMT_OK;
}


int 
hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, epoch_flags_t flags, FILE* fs)
{
  if (flags.flags.lush_active) {
    hpcfmt_byte4_fwrite(x->as_info.bits, fs);
  }

  hpcfmt_byte8_fwrite(x->ip, fs);

  if (flags.flags.lush_active) {
    hpcfile_cstree_lip__fwrite(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfmt_byte8_fwrite(x->metrics[i].bits, fs);
  }

  return HPCFMT_OK;
}


int 
hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs, 
				epoch_flags_t flags, const char* pre)
{
  if (flags.flags.lush_active) {
    char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
    lush_assoc_info_sprintf(as_str, x->as_info);

    fprintf(fs, "(as: %s) ", as_str);
  }

  fprintf(fs, "(ip: 0x%"PRIx64") ", x->ip);

  if (flags.flags.lush_active) {
    hpcfile_cstree_lip__fprint(&x->lip, fs, "");
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

  return HPCFMT_OK;
}


//***************************************************************************

int 
hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcfmt_byte4_fread(&x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFMT_ERR; }
  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcfmt_byte4_fwrite(x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFMT_ERR; }

  return HPCFMT_OK;
}


int 
hpcfile_cstree_as_info__fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  // not done
  return HPCFMT_OK;
}


//***************************************************************************

int 
hpcfile_cstree_lip__fread(lush_lip_t* x, FILE* fs)
{

  //  HPCFMT_TAG__CSTREE_LIP has already been read

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    THROW_IF_HPCERR(hpcfmt_byte8_fread(&x->data8[i], fs));
  }
  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs)
{
  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    hpcfmt_byte8_fwrite(x->data8[i], fs);
  }
  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_lip__fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  char lip_str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(lip_str, x);

  fprintf(fs, "%s(lip: %s)", pre, lip_str);

  return HPCFMT_OK;
}


//***************************************************************************

int 
hpcfile_cstree_node__init(hpcfile_cstree_node_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_cstree_nodedata__init(&x->data);
  return HPCFMT_OK;
}


int 
hpcfile_cstree_node__fini(hpcfile_cstree_node_t* x)
{
  hpcfile_cstree_nodedata__fini(&x->data);  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_node__fread(hpcfile_cstree_node_t* x, 
			   epoch_flags_t flags, FILE* fs)
{
  int ret;

  THROW_IF_HPCERR(hpcfmt_byte4_fread(&x->id, fs));
  THROW_IF_HPCERR(hpcfmt_byte4_fread(&x->id_parent, fs));
  
  ret = hpcfile_cstree_nodedata__fread(&x->data, flags, fs);
  if (ret != HPCFMT_OK) {
    return HPCFMT_ERR; 
  }
  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, 
			    epoch_flags_t flags, FILE* fs)
{
  THROW_IF_HPCERR(hpcfmt_byte4_fwrite(x->id, fs));   // if node is leaf, make the id neg
  THROW_IF_HPCERR(hpcfmt_byte4_fwrite(x->id_parent, fs));
  
  if (hpcfile_cstree_nodedata__fwrite(&x->data, flags, fs) != HPCFMT_OK) {
    return HPCFMT_ERR; 
  }
  
  return HPCFMT_OK;
}


int 
hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs, 
			    epoch_flags_t flags, const char* pre)
{
  fprintf(fs, "%s{node: (id: %d) (id_parent: %d) ",
	  pre, x->id, x->id_parent);

  hpcfile_cstree_nodedata__fprint(&x->data, fs, flags, pre); // pre + "  "

  fprintf(fs, "%s}\n", pre);
  
  return HPCFMT_OK;
}

//***************************************************************************
