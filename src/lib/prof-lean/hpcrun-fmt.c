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
// Copyright ((c)) 2002-2016, Rice University
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

#include <include/gcc-attr.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"

//***************************************************************************

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

  nr = fread(hdr->versionStr, 1, HPCRUN_FMT_VersionLen, infs);
  hdr->versionStr[HPCRUN_FMT_VersionLen] = '\0';
  if (nr != HPCRUN_FMT_VersionLen) {
    return HPCFMT_ERR;
  }
  hdr->version = atof(hdr->versionStr);

  nr = fread(&hdr->endian, 1, HPCRUN_FMT_EndianLen, infs);
  if (nr != HPCRUN_FMT_EndianLen) {
    return HPCFMT_ERR;
  }

  hpcfmt_nvpairList_fread(&(hdr->nvps), infs, alloc);

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
  fprintf(fs, "  (version: %s)\n", hdr->versionStr);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  hpcfmt_nvpairList_fprint(&hdr->nvps, fs, "  ");
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_hdr_free(hpcrun_fmt_hdr_t* hdr, hpcfmt_free_fn dealloc)
{
  if (dealloc) {
    hpcfmt_nvpairList_free(&(hdr->nvps), dealloc);
  }
}


//***************************************************************************
// epoch-hdr
//***************************************************************************

int
hpcrun_fmt_epochHdr_fread(hpcrun_fmt_epochHdr_t* ehdr, FILE* fs,
			  hpcfmt_alloc_fn alloc)
{
  char tag[HPCRUN_FMT_EpochTagLen + 1];

  int nr = fread(tag, 1, HPCRUN_FMT_EpochTagLen, fs);
  tag[HPCRUN_FMT_EpochTagLen] = '\0';
  
  if (nr != HPCRUN_FMT_EpochTagLen) {
    return (nr == 0 && feof(fs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }

  if (strcmp(tag, HPCRUN_FMT_EpochTag) != 0) {
    return HPCFMT_ERR;
  }

  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(ehdr->flags.bits), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(ehdr->measurementGranularity), fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(ehdr->raToCallsiteOfst), fs));
  HPCFMT_ThrowIfError(hpcfmt_nvpairList_fread(&(ehdr->nvps), fs, alloc));

  return HPCFMT_OK;
}


int
hpcrun_fmt_epochHdr_fwrite(FILE* fs, epoch_flags_t flags,
			   uint64_t measurementGranularity,
			   uint32_t raToCallsiteOfst, ...)
{
  va_list args;
  va_start(args, raToCallsiteOfst);

  fwrite(HPCRUN_FMT_EpochTag, 1, HPCRUN_FMT_EpochTagLen, fs);

  hpcfmt_int8_fwrite(flags.bits, fs);
  hpcfmt_int8_fwrite(measurementGranularity, fs);
  hpcfmt_int4_fwrite(raToCallsiteOfst, fs);

  hpcfmt_nvpairs_vfwrite(fs, args);

  va_end(args);

  return HPCFMT_OK;
}


int
hpcrun_fmt_epochHdr_fprint(hpcrun_fmt_epochHdr_t* ehdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCRUN_FMT_EpochTag);
  fprintf(fs, "[epoch-hdr:\n");
  fprintf(fs, "  (flags: 0x%"PRIx64")\n", ehdr->flags.bits);
  fprintf(fs, "  (measurement-granularity: %"PRIu64")\n",
	  ehdr->measurementGranularity);
  fprintf(fs, "  (RA-to-callsite-offset: %"PRIu32")\n", ehdr->raToCallsiteOfst);
  hpcfmt_nvpairList_fprint(&(ehdr->nvps), fs, "  ");
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


void
hpcrun_fmt_epochHdr_free(hpcrun_fmt_epochHdr_t* ehdr, hpcfmt_free_fn dealloc)
{
  if (dealloc) {
    hpcfmt_nvpairList_free(&(ehdr->nvps), dealloc);
  }
}


//***************************************************************************
// metric-tbl
//***************************************************************************

const metric_desc_t metricDesc_NULL = {
  .name          = NULL,
  .description   = NULL,
  .flags.bits_big[0] = 0,
  .flags.bits_big[1] = 0,
  .period        = 0,
  .properties = {.time = 0,.cycles = 0},
  .formula       = NULL,
  .format        = NULL,
};

const hpcrun_metricFlags_t hpcrun_metricFlags_NULL = {
  .fields.ty          = MetricFlags_Ty_NULL,
  .fields.valTy       = MetricFlags_ValTy_NULL,
  .fields.valFmt      = MetricFlags_ValFmt_NULL,
  .fields.unused0     = 0,

  .fields.partner     = 0,
  .fields.show        = (uint8_t)true,
  .fields.showPercent = (uint8_t)true,
  .fields.unused1     = 0,
};

hpcrun_metricVal_t hpcrun_metricVal_ZERO = { .bits = 0 };

//***************************************************************************

int
hpcrun_fmt_metricTbl_fread(metric_tbl_t* metric_tbl, FILE* fs,
			   double fmtVersion, hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(metric_tbl->len), fs));
  if (alloc) {
    metric_tbl->lst =
      (metric_desc_t*) alloc(metric_tbl->len * sizeof(metric_desc_t));
  }

  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_metricDesc_fread(x, fs, fmtVersion, alloc));
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_metricTbl_fwrite(metric_desc_p_tbl_t* metric_tbl, FILE* fs)
{
  hpcfmt_int4_fwrite(metric_tbl->len, fs);
  for (uint32_t i = 0; i < metric_tbl->len; i++) {
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
hpcrun_fmt_metricDesc_fread(metric_desc_t* x, FILE* fs,
			    double GCC_ATTR_UNUSED fmtVersion,
			    hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->name), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->description), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_intX_fread(x->flags.bits, sizeof(x->flags), fs));

  // FIXME: tallent: temporarily support old non-portable convention
  if ( !(x->flags.fields.ty == MetricFlags_Ty_Raw
	 || x->flags.fields.ty == MetricFlags_Ty_Final)
       || x->flags.fields.unused0 != 0
       || x->flags.fields.unused1 != 0) {
    fseek(fs, -sizeof(x->flags), SEEK_CUR);
    
    hpcrun_metricFlags_XXX_t x_flags_old;
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x_flags_old.bits[0]), fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x_flags_old.bits[1]), fs));
    
    x->flags.bits_big[0] = 0;
    x->flags.bits_big[1] = 0;

    x->flags.fields.ty          = x_flags_old.fields.ty;
    x->flags.fields.valTy       = x_flags_old.fields.valTy;
    x->flags.fields.valFmt      = x_flags_old.fields.valFmt;
    x->flags.fields.partner     = (uint16_t) x_flags_old.fields.partner;
    x->flags.fields.show        = x_flags_old.fields.show;
    x->flags.fields.showPercent = x_flags_old.fields.showPercent;
  }

  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->period), fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->formula), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->format), fs, alloc));

  // These two aren't written into the hpcrun file; hence manually set them.
  x->properties.time = 0;
  x->properties.cycles = 0;

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fwrite(metric_desc_t* x, FILE* fs)
{
  hpcfmt_str_fwrite(x->name, fs);
  hpcfmt_str_fwrite(x->description, fs);
  hpcfmt_intX_fwrite(x->flags.bits, sizeof(x->flags), fs);
  hpcfmt_int8_fwrite(x->period, fs);
  hpcfmt_str_fwrite(x->formula, fs);
  hpcfmt_str_fwrite(x->format, fs);
  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fprint(metric_desc_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s[(nm: %s) (desc: %s) "
	  "((ty: %d) (val-ty: %d) (val-fmt: %d) (partner: %u) (show: %d) (showPercent: %d)) "
	  "(period: %"PRIu64") (formula: %s) (format: %s)]\n",
	  pre, hpcfmt_str_ensure(x->name), hpcfmt_str_ensure(x->description),
	  (int)x->flags.fields.ty, (int)x->flags.fields.valTy,
	  (int)x->flags.fields.valFmt,
	  (uint)x->flags.fields.partner, x->flags.fields.show, x->flags.fields.showPercent,
	  x->period,
	  hpcfmt_str_ensure(x->formula), hpcfmt_str_ensure(x->format));
  return HPCFMT_OK;
}


void
hpcrun_fmt_metricDesc_free(metric_desc_t* x, hpcfmt_free_fn dealloc)
{
  hpcfmt_str_free(x->name, dealloc);
  x->name = NULL;
  hpcfmt_str_free(x->description, dealloc);
  x->description = NULL;
  hpcfmt_str_free(x->formula, dealloc);
  x->formula = NULL;
  hpcfmt_str_free(x->format, dealloc);
  x->format = NULL;
}


//***************************************************************************
// loadmap
//***************************************************************************

int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* fs, hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(loadmap->len), fs));
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
  hpcfmt_int4_fwrite(loadmap->len, fs);
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
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(x->id), fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->name), fs, alloc));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->flags), fs));
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmapEntry_fwrite(loadmap_entry_t* x, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->name, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->flags, fs));
  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmapEntry_fprint(loadmap_entry_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s[(id: %u) (nm: %s) (flg: 0x%"PRIx64")]\n",
	  pre, (uint)x->id, x->name, x->flags);
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
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&x->id_parent, fs));

  x->as_info = lush_assoc_info_NULL;
  if (flags.fields.isLogicalUnwind) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&x->as_info.bits, fs));
  }

  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&x->lm_id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->lm_ip, fs));

  lush_lip_init(&x->lip);
  if (flags.fields.isLogicalUnwind) {
    hpcrun_fmt_lip_fread(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->metrics[i].bits, fs));
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_cct_node_fwrite(hpcrun_fmt_cct_node_t* x,
			   epoch_flags_t flags, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->id_parent, fs));

  if (flags.fields.isLogicalUnwind) {
    hpcfmt_int4_fwrite(x->as_info.bits, fs);
  }

  hpcfmt_int2_fwrite(x->lm_id, fs);
  hpcfmt_int8_fwrite(x->lm_ip, fs);

  if (flags.fields.isLogicalUnwind) {
    hpcrun_fmt_lip_fwrite(&x->lip, fs);
  }

  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfmt_int8_fwrite(x->metrics[i].bits, fs);
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs,
			   epoch_flags_t flags, const metric_tbl_t* metricTbl,
			   const char* pre)
{
  // N.B.: convert 'id' and 'id_parent' to ints so leaf flag
  // (negative) is apparent
  fprintf(fs, "%s[node: (id: %d) (id-parent: %d) ",
	  pre, (int)x->id, (int)x->id_parent);

  if (flags.fields.isLogicalUnwind) {
    char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
    lush_assoc_info_sprintf(as_str, x->as_info);

    fprintf(fs, "(as: %s) ", as_str);
  }

  fprintf(fs, "(lm-id: %u) (lm-ip: 0x%"PRIx64") ", (uint)x->lm_id, x->lm_ip);

  if (flags.fields.isLogicalUnwind) {
    hpcrun_fmt_lip_fprint(&x->lip, fs, "");
  }

  fprintf(fs, "\n");

  fprintf(fs, "%s(metrics:", pre);
  for (uint i = 0; i < x->num_metrics; ++i) {
    hpcrun_metricFlags_t mflags = hpcrun_metricFlags_NULL;
    if (metricTbl) {
      const metric_desc_t* mdesc = &(metricTbl->lst[i]);
      mflags = mdesc->flags;
    }

    switch (mflags.fields.valFmt) {
      default:
      case MetricFlags_ValFmt_Int:
	fprintf(fs, " %"PRIu64, x->metrics[i].i);
	break;
      case MetricFlags_ValFmt_Real:
	fprintf(fs, " %g", x->metrics[i].r);
	break;
    }

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
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->data8[i], fs));
  }
  
  return HPCFMT_OK;
}


int
hpcrun_fmt_lip_fwrite(lush_lip_t* x, FILE* fs)
{
  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    hpcfmt_int8_fwrite(x->data8[i], fs);
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
// hpctrace (located here for now)
//***************************************************************************

//***************************************************************************
// [hpctrace] hdr
//***************************************************************************

const hpctrace_hdr_flags_t hpctrace_hdr_flags_NULL = {
  .bits = 0
};


int
hpctrace_fmt_hdr_fread(hpctrace_fmt_hdr_t* hdr, FILE* infs)
{
  char tag[HPCTRACE_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCTRACE_FMT_MagicLen, infs);
  tag[HPCTRACE_FMT_MagicLen] = '\0';

  if (nr != HPCTRACE_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCTRACE_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(hdr->versionStr, 1, HPCTRACE_FMT_VersionLen, infs);
  hdr->versionStr[HPCTRACE_FMT_VersionLen] = '\0';
  if (nr != HPCTRACE_FMT_VersionLen) {
    return HPCFMT_ERR;
  }
  hdr->version = atof(hdr->versionStr);

  nr = fread(&hdr->endian, 1, HPCTRACE_FMT_EndianLen, infs);
  if (nr != HPCTRACE_FMT_EndianLen) {
    return HPCFMT_ERR;
  }

  hdr->flags = hpctrace_hdr_flags_NULL;
  if (hdr->version > 1.0) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->flags.bits), infs));
  }

  return HPCFMT_OK;
}


// Writer based on outbuf.
// Returns: HPCFMT_OK on success, else HPCFMT_ERR.
int
hpctrace_fmt_hdr_outbuf(hpctrace_hdr_flags_t flags, hpcio_outbuf_t* outbuf)
{
  ssize_t ret;

  const int bufSZ = sizeof(flags);
  unsigned char buf[bufSZ];

  uint64_t flag_bits = flags.bits;
  int k = 0;
  for (int shift = 56; shift >= 0; shift -= 8) {
    buf[k] = (flag_bits >> shift) & 0xff;
    k++;
  }

  hpcio_outbuf_write(outbuf, HPCTRACE_FMT_Magic, HPCTRACE_FMT_MagicLen);
  hpcio_outbuf_write(outbuf, HPCTRACE_FMT_Version, HPCTRACE_FMT_VersionLen);
  hpcio_outbuf_write(outbuf, HPCTRACE_FMT_Endian, HPCTRACE_FMT_EndianLen);
  ret = hpcio_outbuf_write(outbuf, buf, bufSZ);

  if (ret != bufSZ) {
    return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}


// N.B.: not async safe
int
hpctrace_fmt_hdr_fwrite(hpctrace_hdr_flags_t flags, FILE* fs)
{
  fwrite(HPCTRACE_FMT_Magic,   1, HPCTRACE_FMT_MagicLen, fs);
  fwrite(HPCTRACE_FMT_Version, 1, HPCTRACE_FMT_VersionLen, fs);
  fwrite(HPCTRACE_FMT_Endian,  1, HPCTRACE_FMT_EndianLen, fs);
  hpcfmt_int8_fwrite(flags.bits, fs);

  return HPCFMT_OK;
}


int
hpctrace_fmt_hdr_fprint(hpctrace_fmt_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCTRACE_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %s)\n", hdr->versionStr);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  fprintf(fs, "  (flags: 0x%"PRIx64")\n", hdr->flags.bits);
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}


//***************************************************************************
// [hpctrace] datum (trace record)
//***************************************************************************

int
hpctrace_fmt_datum_fread(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			 FILE* fs)
{
  int ret = HPCFMT_OK;
  
  ret = hpcfmt_int8_fread(&(x->time), fs);
  if (ret != HPCFMT_OK) {
    return ret; // can be HPCFMT_EOF
  }

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->cpId), fs));

  if (flags.fields.isDataCentric) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->metricId), fs));
  }
  else {
    x->metricId = HPCRUN_FMT_MetricId_NULL;
  }

  return HPCFMT_OK;
}


// Append the trace record to the outbuf.
// Returns: HPCFMT_OK on success, else HPCFMT_ERR.
int
hpctrace_fmt_datum_outbuf(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  hpcio_outbuf_t* outbuf)
{
  const int bufSZ = sizeof(hpctrace_fmt_datum_t);
  unsigned char buf[bufSZ];
  int shift, k;

  k = 0;

  uint64_t time = x->time;
  for (shift = 56; shift >= 0; shift -= 8) {
    buf[k] = (time >> shift) & 0xff;
    k++;
  }

  uint32_t cpId = x->cpId;
  for (shift = 24; shift >= 0; shift -= 8) {
    buf[k] = (cpId >> shift) & 0xff;
    k++;
  }

  if (flags.fields.isDataCentric) {
    uint32_t metricId = x->metricId;
    for (shift = 24; shift >= 0; shift -= 8) {
      buf[k] = (metricId >> shift) & 0xff;
      k++;
    }
  }

  if (hpcio_outbuf_write(outbuf, buf, k) != k) {
    return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}


int
hpctrace_fmt_datum_fwrite(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  FILE* outfs)
{
  hpcfmt_int8_fwrite(x->time, outfs);
  hpcfmt_int4_fwrite(x->cpId, outfs);
  if (flags.fields.isDataCentric) {
    hpcfmt_int4_fwrite(x->metricId, outfs);
  }

  return HPCFMT_OK;
}


int
hpctrace_fmt_datum_fprint(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  FILE* fs)
{
  fprintf(fs, "(%"PRIu64", %u", x->time, x->cpId);
  if (flags.fields.isDataCentric) {
    fprintf(fs, ", %u",  x->metricId);
  }
  fputs(")\n", fs);
  return HPCFMT_OK;
}


//***************************************************************************
// hpcprof-metricdb (located here for now)
//***************************************************************************

//***************************************************************************
// [hpcprof-metricdb] hdr
//***************************************************************************

int
hpcmetricDB_fmt_hdr_fread(hpcmetricDB_fmt_hdr_t* hdr, FILE* infs)
{
  char tag[HPCMETRICDB_FMT_MagicLen + 1];
  char version[HPCMETRICDB_FMT_VersionLen + 1];
  char endian[HPCMETRICDB_FMT_EndianLen + 1];

  int nr = fread(tag, 1, HPCMETRICDB_FMT_MagicLen, infs);
  tag[HPCMETRICDB_FMT_MagicLen] = '\0';

  if (nr != HPCMETRICDB_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCMETRICDB_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(&version, 1, HPCMETRICDB_FMT_VersionLen, infs);
  version[HPCMETRICDB_FMT_VersionLen] = '\0';
  if (nr != HPCMETRICDB_FMT_VersionLen) {
    return HPCFMT_ERR;
  }
  hdr->version = atof(hdr->versionStr);

  nr = fread(&endian, 1, HPCMETRICDB_FMT_EndianLen, infs);
  if (nr != HPCMETRICDB_FMT_EndianLen) {
    return HPCFMT_ERR;
  }

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(hdr->numNodes), infs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(hdr->numMetrics), infs));

  return HPCFMT_OK;
}


int
hpcmetricDB_fmt_hdr_fwrite(hpcmetricDB_fmt_hdr_t* hdr, FILE* outfs)
{
  fwrite(HPCMETRICDB_FMT_Magic,   1, HPCMETRICDB_FMT_MagicLen, outfs);
  fwrite(HPCMETRICDB_FMT_Version, 1, HPCMETRICDB_FMT_VersionLen, outfs);
  fwrite(HPCMETRICDB_FMT_Endian,  1, HPCMETRICDB_FMT_EndianLen, outfs);

  hpcfmt_int4_fwrite(hdr->numNodes, outfs);
  hpcfmt_int4_fwrite(hdr->numMetrics, outfs);

  return HPCFMT_OK;
}


int
hpcmetricDB_fmt_hdr_fprint(hpcmetricDB_fmt_hdr_t* hdr, FILE* outfs)
{
  fprintf(outfs, "%s\n", HPCMETRICDB_FMT_Magic);
  fprintf(outfs, "[hdr:...]\n");

  fprintf(outfs, "(num-nodes:   %u)\n", hdr->numNodes);
  fprintf(outfs, "(num-metrics: %u)\n", hdr->numMetrics);

  return HPCFMT_OK;
}

