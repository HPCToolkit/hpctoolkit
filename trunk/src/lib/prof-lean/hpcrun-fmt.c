// -*-Mode: c;-*- // technically C99
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
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
#include <stdarg.h>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include "hpcio.h"
#include "hpcrun-fmt.h"
#include "hpcfmt.h"

//*************************** Forward Declarations **************************


//***************************************************************************
//
//***************************************************************************

int
nvpairs_vfwrite(FILE *out, va_list args)
{
  va_list _tmp;
  va_copy(_tmp, args);

  uint32_t len = 0;

  for (char *arg = va_arg(_tmp, char *); arg != NULL; arg = va_arg(_tmp, char *)) {
    arg = va_arg(_tmp, char *);
    len++;
  }
  va_end(_tmp);

  hpcfmt_byte4_fwrite(len, out);

  for (char *arg = va_arg(args, char *); arg != NULL; arg = va_arg(args, char *)) {
    hpcfmt_fstr_fwrite(arg, out); // write NAME
    arg = va_arg(args, char *);
    hpcfmt_fstr_fwrite(arg, out); // write VALUE
  }
  return HPCFILE_OK;
}


char *
hpcrun_fmt_nvpair_search(LIST_OF(nvpair_t) *lst, const char *name)
{
  nvpair_t *p = lst->lst;
  for (int i=0; i<lst->len; p++,i++) {
    if (strcmp(p->name, name) == 0) {
      return p->val;
    }
  }
  return "NOT FOUND";
}


//***************************************************************************
//
//***************************************************************************

hpcrun_metric_data_t hpcrun_metric_data_ZERO = { .bits = 0 };

static int
hpcrun_fmt_nvpair_t_fwrite(nvpair_t* nvp, FILE* fs)
{
  int ret;

  ret = hpcfmt_fstr_fwrite(nvp->name, fs);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR;
  }
  
  ret = hpcfmt_fstr_fwrite(nvp->val, fs);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR;
  }

  return HPCFILE_OK;
}


static int
hpcrun_fmt_nvpair_t_fread(nvpair_t* inp, FILE* infs, alloc_fn alloc)
{
  hpcfmt_fstr_fread(&(inp->name), infs, alloc);
  hpcfmt_fstr_fread(&(inp->val), infs, alloc);

  return HPCFILE_OK;
}


static int
hpcrun_fmt_nvpair_t_fprint(nvpair_t* nvp, FILE* fs, const char* pre)
{
  fprintf(fs, "%s{nv-pair: %s, %s}\n", pre, nvp->name, nvp->val);
  return HPCFILE_OK;
}


static int
hpcrun_fmt_list_of_nvpair_t_fread(LIST_OF(nvpair_t)* nvps, FILE* infs, 
				  alloc_fn alloc)
{
  hpcfmt_byte4_fread(&(nvps->len), infs);
  if (alloc != NULL) {
    nvps->lst = (nvpair_t *) alloc(nvps->len * sizeof(nvpair_t));
  }
  for (uint32_t i = 0; i < nvps->len; i++) {
    hpcrun_fmt_nvpair_t_fread(&nvps->lst[i], infs, alloc);
  }
  return HPCFILE_OK;
}


static int
hpcrun_fmt_list_of_nvpair_t_fprint(LIST_OF(nvpair_t)* nvps, FILE* fs,
				   const char* pre)
{
  for (uint32_t i = 0; i < nvps->len; ++i) {
    hpcrun_fmt_nvpair_t_fprint(&nvps->lst[i], fs, pre);
  }
  return HPCFILE_OK;
}


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

  nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFILE_OK;
}


int
hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t* hdr, FILE* infs, alloc_fn alloc)
{
  char tag[HPCRUN_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCRUN_FMT_MagicLen, infs);
  tag[HPCRUN_FMT_MagicLen] = '\0';

  if (nr != HPCRUN_FMT_MagicLen) {
    return HPCFILE_ERR;
  }
  if (strcmp(tag, HPCRUN_FMT_Magic) != 0) {
    return HPCFILE_ERR;
  }

  nr = fread(hdr->version, 1, HPCRUN_FMT_VersionLen, infs);
  tag[HPCRUN_FMT_VersionLen] = '\0';
  if (nr != HPCRUN_FMT_VersionLen) {
    return HPCFILE_ERR;
  }

  nr = fread(&hdr->endian, 1, HPCRUN_FMT_EndianLen, infs);
  if (nr != HPCRUN_FMT_EndianLen) {
    return HPCFILE_ERR;
  }

  hpcrun_fmt_list_of_nvpair_t_fread(&(hdr->nvps), infs, alloc);

  return HPCFILE_OK;
}


int
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_Magic);

  fprintf(fs, "{hdr:\n");
  fprintf(fs, "  (version: %s)\n", hdr->version);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  hpcrun_fmt_list_of_nvpair_t_fprint(&hdr->nvps, fs, "  ");
  fprintf(fs, "}\n");

  return HPCFILE_OK;
}


//***************************************************************************
// epoch-hdr
//***************************************************************************

int
hpcrun_fmt_epoch_hdr_fwrite(FILE* fs, uint64_t flags, 
			    uint32_t ra_distance, uint64_t granularity, ...)
{
  va_list args;
  va_start(args, granularity);

  fwrite(HPCRUN_FMT_EpochTag, 1, HPCRUN_FMT_EpochTagLen, fs);

  hpcfmt_byte8_fwrite(flags, fs);
  hpcfmt_byte4_fwrite(ra_distance, fs);
  hpcfmt_byte8_fwrite(granularity, fs);

  nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFILE_OK;
}


int
hpcrun_fmt_epoch_hdr_fread(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs, 
			   alloc_fn alloc)
{
  char tag[HPCRUN_FMT_EpochTagLen + 1];

  int nr = fread(tag, 1, HPCRUN_FMT_EpochTagLen, fs);
  tag[HPCRUN_FMT_EpochTagLen] = '\0';
  
  if (nr == 0) {
    return HPCFILE_EOF;
  }
  else if (nr != HPCRUN_FMT_EpochTagLen) {
    return HPCFILE_ERR;
  }

  if (strcmp(tag, HPCRUN_FMT_EpochTag) != 0) {
    //fprintf(stderr,"invalid epoch header tag: %s\n", tag);
    return HPCFILE_ERR;
  }

  // FIXME: please check for errors!
  hpcfmt_byte8_fread(&(ehdr->flags), fs);
  hpcfmt_byte4_fread(&(ehdr->ra_distance), fs);
  hpcfmt_byte8_fread(&(ehdr->granularity), fs);
  hpcrun_fmt_list_of_nvpair_t_fread(&(ehdr->nvps), fs, alloc);

  return HPCFILE_OK;
}


int
hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t* ehdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_EpochTag);
  fprintf(fs, "{epoch-hdr:\n");
  fprintf(fs, "  (flags: %"PRIx64") ", ehdr->flags);
  fprintf(fs, "(RA distance: %d) ", ehdr->ra_distance);
  fprintf(fs, "(address granularity: %"PRIu64")\n", ehdr->granularity);
  hpcrun_fmt_list_of_nvpair_t_fprint(&(ehdr->nvps), fs, "  ");
  fprintf(fs, "}\n");

  return HPCFILE_OK;
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
    hpcfmt_fstr_fwrite(m_lst[i].name, fs);
    hpcfmt_byte8_fwrite(m_lst[i].flags, fs);
    hpcfmt_byte8_fwrite(m_lst[i].period, fs);
  }
  return HPCFILE_OK;
}


int
hpcrun_fmt_metric_tbl_fread(metric_tbl_t* metric_tbl, FILE* in, alloc_fn alloc)
{
  hpcfmt_byte4_fread(&(metric_tbl->len), in);
  if (alloc != NULL) {
    metric_tbl->lst = 
      (metric_desc_t *) alloc(metric_tbl->len * sizeof(metric_desc_t));
  }
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    hpcfmt_fstr_fread(&(p->name), in, alloc);
    hpcfmt_byte8_fread(&(p->flags), in);
    hpcfmt_byte8_fread(&(p->period), in);
  }
  return HPCFILE_OK;
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
  
  return HPCFILE_OK;
}


void
hpcrun_fmt_metric_tbl_free(metric_tbl_t* metric_tbl, free_fn dealloc)
{
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    hpcfmt_fstr_free(p->name, dealloc);
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
    hpcfmt_fstr_fwrite(src->name, fs);
    hpcfmt_byte8_fwrite((uint64_t)(unsigned long)src->vaddr, fs);
    hpcfmt_byte8_fwrite((uint64_t)(unsigned long)src->mapaddr, fs);
    src = src->next;
  }
  
  return HPCFILE_OK;
}


int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* fs, alloc_fn alloc)
{
  hpcfmt_byte4_fread(&(loadmap->len), fs);
  if (alloc) {
    loadmap->lst = alloc(loadmap->len * sizeof(loadmap_entry_t));
  }

  // FIXME: incomplete and hard-to-understand OSR?  Why not index by i?
  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {
    hpcfmt_fstr_fread(&(e->name), fs, alloc);
    hpcfmt_byte8_fread(&(e->vaddr), fs);
    hpcfmt_byte8_fread(&(e->mapaddr), fs);
    // FIXME ?? (don't know what these are for yet ...
    e->flags = 0;
  }

  return HPCFILE_OK;
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
  
  return HPCFILE_OK;
}


void
hpcrun_fmt_loadmap_free(loadmap_t* loadmap, free_fn dealloc)
{
  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {
    hpcfmt_fstr_free(e->name, dealloc);
    e->name = NULL;
  }
  dealloc(loadmap->lst);
  loadmap->lst = NULL;
}


//***************************************************************************
// cct
//***************************************************************************


//***************************************************************************

int 
hpcfile_cstree_nodedata__init(hpcfile_cstree_nodedata_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fini(hpcfile_cstree_nodedata_t* x)
{
  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fread(hpcfile_cstree_nodedata_t* x, FILE* fs)
{
  // ASSUMES: space for metrics has been allocated
  
  hpcfmt_byte4_fread(&x->as_info.bits, fs);
  hpcfmt_byte8_fread(&x->ip, fs);
  hpcfile_cstree_lip__fread(&x->lip, fs);

  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfmt_byte8_fread(&x->metrics[i].bits, fs);
  }

  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs)
{
  hpcfmt_byte4_fwrite(x->as_info.bits, fs);
  hpcfmt_byte8_fwrite(x->ip, fs);

  // lip stuff

  hpcfile_cstree_lip__fwrite(&x->lip, fs);

  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfmt_byte8_fwrite(x->metrics[i].bits, fs);
  }

  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs,
				const char* pre)
{
  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(as_str, x->as_info);

  fprintf(fs, "(as: %s) (ip: 0x%"PRIx64") ", as_str, x->ip);
  hpcfile_cstree_lip__fprint(&x->lip, fs, "");
  fprintf(fs, "\n");
	  
  fprintf(fs, "%s(metrics:", pre);
  for (int i = 0; i < x->num_metrics; ++i) {
    fprintf(fs, " %"PRIu64, x->metrics[i].bits);
    if (i + 1 < x->num_metrics) {
      fprintf(fs, " ");
    }
  }
  fprintf(fs, ")\n");

  return HPCFILE_OK;
}


//***************************************************************************

int 
hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcfmt_byte4_fread(&x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcfmt_byte4_fwrite(x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}


int 
hpcfile_cstree_as_info__fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  // not done
  return HPCFILE_OK;
}


//***************************************************************************

int 
hpcfile_cstree_lip__fread(lush_lip_t* x, FILE* fs)
{

  //  HPCFILE_TAG__CSTREE_LIP has already been read

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    hpcfmt_byte8_fread(&x->data8[i], fs);
  }
  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs)
{
  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    hpcfmt_byte8_fwrite(x->data8[i], fs);
  }
  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_lip__fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  char lip_str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(lip_str, x);

  fprintf(fs, "%s(lip: %s)", pre, lip_str);

  return HPCFILE_OK;
}


//***************************************************************************

int 
hpcfile_cstree_node__init(hpcfile_cstree_node_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_cstree_nodedata__init(&x->data);
  return HPCFILE_OK;
}


int 
hpcfile_cstree_node__fini(hpcfile_cstree_node_t* x)
{
  hpcfile_cstree_nodedata__fini(&x->data);  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_node__fread(hpcfile_cstree_node_t* x, FILE* fs)
{
  int ret;

  hpcfmt_byte4_fread(&x->id, fs);
  hpcfmt_byte4_fread(&x->id_parent, fs);
  
  ret = hpcfile_cstree_nodedata__fread(&x->data, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs)
{
  hpcfmt_byte4_fwrite(x->id, fs);   // if node is leaf, make the id neg
  hpcfmt_byte4_fwrite(x->id_parent, fs);
  
  if (hpcfile_cstree_nodedata__fwrite(&x->data, fs) != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}


int 
hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s{node: (id: %"PRIu32") (id_parent: %"PRIu32") ", 
	  pre, x->id, x->id_parent);

  hpcfile_cstree_nodedata__fprint(&x->data, fs, pre); // pre + "  "

  fprintf(fs, "%s}\n", pre);
  
  return HPCFILE_OK;
}

//***************************************************************************
