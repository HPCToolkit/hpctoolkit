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
// Copyright ((c)) 2002-2021, Rice University
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
#include "id-tuple.h"

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


  // removed m_raToCallsiteOfst from epoch Hdr. don't change file format!
  uint32_t dummy;

  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(ehdr->flags.bits), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(ehdr->measurementGranularity), fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&dummy, fs));
  HPCFMT_ThrowIfError(hpcfmt_nvpairList_fread(&(ehdr->nvps), fs, alloc));

  return HPCFMT_OK;

}


int
hpcrun_fmt_epochHdr_fwrite(FILE* fs, epoch_flags_t flags,
			   uint64_t measurementGranularity,
			   ...)
{
  va_list args;
  va_start(args, measurementGranularity);

  int nw = fwrite(HPCRUN_FMT_EpochTag, 1, HPCRUN_FMT_EpochTagLen, fs);
  if (nw != HPCRUN_FMT_EpochTagLen) return HPCFMT_ERR;

  // removed m_raToCallsiteOfst from epoch Hdr. don't change file format!
  uint32_t dummy = 0;

  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(flags.bits, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(measurementGranularity, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(dummy, fs));

  HPCFMT_ThrowIfError(hpcfmt_nvpairs_vfwrite(fs, args));

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
hpcrun_fmt_metricTbl_fread(metric_tbl_t* metric_tbl, metric_aux_info_t **aux_info,
		FILE* fs, double fmtVersion, hpcfmt_alloc_fn alloc)
{

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(metric_tbl->len), fs));
  if (alloc) {
    metric_tbl->lst =
      (metric_desc_t*) alloc(metric_tbl->len * sizeof(metric_desc_t));
  }

  size_t aux_info_size = sizeof(metric_aux_info_t) * metric_tbl->len;
  metric_aux_info_t *perf_info = (metric_aux_info_t*)malloc(aux_info_size);
  memset(perf_info, 0, aux_info_size);

  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    HPCFMT_ThrowIfError(hpcrun_fmt_metricDesc_fread(x, &(perf_info)[i], fs, fmtVersion, alloc));
  }
  *aux_info = perf_info;

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricTbl_fwrite(metric_desc_p_tbl_t* metric_tbl, metric_aux_info_t *aux_info, FILE* fs)
{
  for (uint32_t i = 0; i < metric_tbl->len; i++) {

	  // corner case: for other sampling sources than perf event, the
	  // value of aux_info is NULL

    metric_aux_info_t info_tmp;
	metric_aux_info_t *info_ptr = aux_info;

	if (aux_info == NULL) {
	  memset(&info_tmp, 0, sizeof(metric_aux_info_t));
	  info_ptr = &info_tmp;
	} else {
	  info_ptr = &(aux_info[i]);
	}

    hpcrun_fmt_metricDesc_fwrite(metric_tbl->lst[i], info_ptr, fs);
  }

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricTbl_fprint(metric_tbl_t* metric_tbl, metric_aux_info_t *aux_info, FILE* fs)
{
  fprintf(fs, "[metric-tbl: (num-entries: %u)\n", metric_tbl->len);
  for (uint32_t i = 0; i < metric_tbl->len; i++) {
    metric_desc_t* x = &metric_tbl->lst[i];
    hpcrun_fmt_metricDesc_fprint(x, &(aux_info[i]), fs, "  ", i);
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
hpcrun_fmt_metricDesc_fread(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* fs,
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

  HPCFMT_ThrowIfError(hpcfmt_int2_fread ((uint16_t*)&(x->is_frequency_metric),    fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread ((uint16_t*)&(aux_info->is_multiplexed),  fs));
  HPCFMT_ThrowIfError(hpcfmt_real8_fread(&(aux_info->threshold_mean),  fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread ((&aux_info->num_samples),     fs));

  // These two aren't written into the hpcrun file; hence manually set them.
  x->properties.time = 0;
  x->properties.cycles = 0;

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fwrite(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->name, fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->description, fs));
  HPCFMT_ThrowIfError(hpcfmt_intX_fwrite(x->flags.bits, sizeof(x->flags), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->period, fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->formula, fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fwrite(x->format, fs));

  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x->is_frequency_metric, fs));

  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(aux_info->is_multiplexed, fs));
  HPCFMT_ThrowIfError(hpcfmt_real8_fwrite(aux_info->threshold_mean, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(aux_info->num_samples, fs));

  return HPCFMT_OK;
}


int
hpcrun_fmt_metricDesc_fprint(metric_desc_t* x, metric_aux_info_t *aux_info, FILE* fs, const char* pre, uint32_t id)
{
  fprintf(fs, "%s[(id: %d) (nm: %s) (desc: %s) "
	  "((ty: %d) (val-ty: %d) (val-fmt: %d) (partner: %u) (show: %d) (showPercent: %d)) "
	  "(period: %"PRIu64") (formula: %s) (format: %s)\n" ,
	  pre, id, hpcfmt_str_ensure(x->name), hpcfmt_str_ensure(x->description),
	  (int)x->flags.fields.ty, (int)x->flags.fields.valTy,
	  (int)x->flags.fields.valFmt,
	  (uint)x->flags.fields.partner, x->flags.fields.show, x->flags.fields.showPercent,
	  x->period,
	  hpcfmt_str_ensure(x->formula), hpcfmt_str_ensure(x->format));
  fprintf(fs, "    (frequency: %d) (multiplexed: %d) (period-mean: %f) (num-samples: %d)]\n",
          (int)x->is_frequency_metric, (int)aux_info->is_multiplexed,
		  aux_info->threshold_mean,  (int) aux_info->num_samples);
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


void
hpcrun_fmt_metric_set_format(metric_desc_t *metric_desc, char *format)
{
  metric_desc->format = format;
}



double
hpcrun_fmt_metric_get_value(metric_desc_t metric_desc, hpcrun_metricVal_t metric)
{
  if (metric_desc.flags.fields.valFmt == MetricFlags_ValFmt_Int) {
    return (double) metric.i;
  }
  else if (metric_desc.flags.fields.valFmt == MetricFlags_ValFmt_Real) {
    return metric.r;
  }
  // TODO: default value
  return metric.r;
}

// set a new value into a metric
void
hpcrun_fmt_metric_set_value(metric_desc_t metric_desc,
   hpcrun_metricVal_t *metric, double value)
{
  if (metric_desc.flags.fields.valFmt == MetricFlags_ValFmt_Int) {
    metric->i = (int) value;
  }
  else if (metric_desc.flags.fields.valFmt == MetricFlags_ValFmt_Real) {
    metric->r = value;
  }
}

// set a new integer value into a metric, and force it to be
// an integer type metric
void
hpcrun_fmt_metric_set_value_int( hpcrun_metricFlags_t *flags,
   hpcrun_metricVal_t *metric, int value)
{
  flags->fields.valFmt = MetricFlags_ValFmt_Int;
  metric->i = value;
}

// set a new double value into a metric, and force it to be
// a real type metric
void
hpcrun_fmt_metric_set_value_real( hpcrun_metricFlags_t *flags,
   hpcrun_metricVal_t *metric, double value)
{
  flags->fields.valFmt = MetricFlags_ValFmt_Real;
  metric->r = value;
}


//***************************************************************************
// loadmap
//***************************************************************************

int
hpcrun_fmt_loadmap_fread(loadmap_t* loadmap, FILE* fs, hpcfmt_alloc_fn alloc)
{

#if 1
//YUMENG: no epoch, so loadmap needs to handle EOF situation
  int r = hpcfmt_int4_fread(&(loadmap->len), fs);
  if(r == HPCFMT_EOF ){
    return HPCFMT_EOF;
  }
  if(r != HPCFMT_OK){
    return HPCFMT_ERR;
  }
#else
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(loadmap->len), fs));
#endif

  if (alloc) {
    loadmap->lst = alloc(loadmap->len * sizeof(loadmap_entry_t));
  }
  for (uint32_t i = 0; i < loadmap->len; i++) {
    loadmap_entry_t* e = &loadmap->lst[i];
    int ret = hpcrun_fmt_loadmapEntry_fread(e, fs, alloc);
    if(ret == HPCFMT_ERR) return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}


int
hpcrun_fmt_loadmap_fwrite(loadmap_t* loadmap, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(loadmap->len, fs));
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
  //int ret = 10;
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(x->id), fs));
  HPCFMT_ThrowIfError(hpcfmt_str_fread(&(x->name), fs, alloc));
  //ret += (strlen(x->name) + 4);
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->flags), fs));
  return HPCFMT_OK;
  //return ret;
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
  /* YUMENG: no need for sparse metrics
  for (int i = 0; i < x->num_metrics; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->metrics[i].bits, fs));
  }
  */
  return HPCFMT_OK;
}


int
hpcrun_fmt_cct_node_fwrite(hpcrun_fmt_cct_node_t* x,
			   epoch_flags_t flags, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->id_parent, fs));

  if (flags.fields.isLogicalUnwind) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->as_info.bits, fs));
  }

  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x->lm_id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->lm_ip, fs));

  if (flags.fields.isLogicalUnwind) {
    HPCFMT_ThrowIfError(hpcrun_fmt_lip_fwrite(&x->lip, fs));
  }
  /*YUMENG: no need for sparse metrics
  for (int i = 0; i < x->num_metrics; ++i) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->metrics[i].bits, fs));
  }
  */
  return HPCFMT_OK;
}


#if 0
int
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs,
			   epoch_flags_t flags, const metric_tbl_t* metricTbl,
			   const char* pre)
#else
//YUMENG: no need to parse metricTbl for sparse format
int
hpcrun_fmt_cct_node_fprint(hpcrun_fmt_cct_node_t* x, FILE* fs,
			   epoch_flags_t flags, const char* pre)
#endif
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

//YUMENG: no need for sparse format
#if 0
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
#endif
  fprintf(fs, "]\n");

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
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->data8[i], fs));
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
// sparse metircs - YUMENG
// 
/* EXAMPLE
[sparse metrics:
  [basic information:
    (thread id: 69)
    (number of non-zero metrics: 13)
    (number of non-zero cct nodes: 13)
  ]
  [metrics:
  (NOTES: printed in file order, help checking if hpcrun file is correct)
    (value: 0.000157, metric id: 0)
    (value: 0.000107, metric id: 0)
    ...
  ]
  [cct node indices:
    (cct node id: 1504, index: 0)
    (cct node id: 2300, index: 1)
    ...
  ]
]
*/
//***************************************************************************

int
hpcrun_fmt_sparse_metrics_fread(hpcrun_fmt_sparse_metrics_t* x, FILE* fs)
{

  //HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->tid), fs));
  HPCFMT_ThrowIfError(id_tuple_fread(&(x->id_tuple), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->num_vals), fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->num_nz_cct_nodes), fs));

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
hpcrun_fmt_sparse_metrics_fwrite(hpcrun_fmt_sparse_metrics_t* x,FILE* fs)
{
  //HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->tid, fs));
  HPCFMT_ThrowIfError(id_tuple_fwrite(&(x->id_tuple), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->num_vals, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->num_nz_cct_nodes, fs));


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
hpcrun_fmt_sparse_metrics_fprint(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const char* pre, bool easy_grep)
{
  char* double_pre = "    ";
  fprintf(fs, "[sparse metrics:\n");
  fprintf(fs, "%s[basic information:\n", pre);
  fprintf(fs, "%s id tuple:", double_pre);
  id_tuple_fprint(&(x->id_tuple),fs);
  fprintf(fs, "%s(number of non-zero metrics: %ld)\n%s(number of non-zero cct nodes: %d)\n%s]\n",
	  double_pre, x->num_vals, double_pre, x->num_nz_cct_nodes, pre);

  if(easy_grep){
    HPCFMT_ThrowIfError(hpcrun_fmt_sparse_metrics_fprint_grep_helper(x, fs, metricTbl, pre));
  }else{
    fprintf(fs, "%s[metrics:\n%s(NOTES: printed in file order, help checking if hpcrun file is correct)\n", pre, pre);
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

  fprintf(fs, "%s[cct node indices:\n", pre);
  for (uint i = 0; i < x->num_nz_cct_nodes + 1; i++) {
    if(i < x->num_nz_cct_nodes){
      fprintf(fs, "%s(cct node id: %d, index: %ld)\n", double_pre, x->cct_node_ids[i], x->cct_node_idxs[i]);
    }else{
      if(x->cct_node_ids[i] == LastNodeEnd) fprintf(fs, "%s(cct node id: END, index: %ld)\n", double_pre, x->cct_node_idxs[i]);
    }
  }

  fprintf(fs, "%s]\n", pre);

  fprintf(fs, "]\n");

  return HPCFMT_OK;
}

int
hpcrun_fmt_sparse_metrics_fprint_grep_helper(hpcrun_fmt_sparse_metrics_t* x, FILE* fs,
          const metric_tbl_t* metricTbl, const char* pre)
{
  char* double_pre = "    ";
  fprintf(fs, "%s[metrics easy grep version:\n%s(NOTES: metrics for a cct node are printed together, easy to grep)\n", pre, pre);

  for (uint i = 0; i < x->num_nz_cct_nodes; ++i) {
    uint32_t cct_node_id = x->cct_node_ids[i];
    uint64_t cct_node_off = x->cct_node_idxs[i];
    uint64_t cct_next_node_off = x->cct_node_idxs[i+1];
    fprintf(fs, "%s(cct node id: %d) ", double_pre, cct_node_id);

    for(uint j = cct_node_off; j < cct_next_node_off; j++){
      fprintf(fs, "(metric %d:", x->mids[j]);

      hpcrun_metricFlags_t mflags = hpcrun_metricFlags_NULL;
      if (metricTbl) {
        int metric_id = x->mids[j];
        const metric_desc_t* mdesc = &(metricTbl->lst[metric_id]);
        mflags = mdesc->flags;
      }

      switch (mflags.fields.valFmt) {
        default: //used for printing tmp sparse-db files, it does not have metric_tbl included
          fprintf(fs, "%g", x->values[j].r);
          break;
        case MetricFlags_ValFmt_Int:
          fprintf(fs, "%"PRIu64, x->values[j].i);
          break;
        case MetricFlags_ValFmt_Real:
          fprintf(fs, "%g", x->values[j].r);
          break;
      }

      fprintf(fs, ") ");
    }
    fprintf(fs, "\n");
    
  }
  fprintf(fs, "%s]\n", pre);

  return HPCFMT_OK;
}

void
hpcrun_fmt_sparse_metrics_free(hpcrun_fmt_sparse_metrics_t* x, hpcfmt_free_fn dealloc)
{
  dealloc(x->cct_node_ids);
  dealloc(x->cct_node_idxs);
  dealloc(x->values);
  dealloc(x->mids);
  id_tuple_free(&(x->id_tuple));

  x->cct_node_ids  = NULL;
  x->cct_node_idxs = NULL;
  x->values  = NULL;
  x->mids    = NULL;
}

//***************************************************************************
// hpcrun_fmt_footer_t - YUMENG
//***************************************************************************
int
hpcrun_fmt_footer_fwrite(hpcrun_fmt_footer_t* x, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->hdr_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->hdr_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->loadmap_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->loadmap_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->cct_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->cct_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->met_tbl_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->met_tbl_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->sm_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->sm_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->footer_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->HPCRUNsm,fs));

  return HPCFMT_OK;
}

int
hpcrun_fmt_footer_fread(hpcrun_fmt_footer_t* x, FILE* fs)
{
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->hdr_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->hdr_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->loadmap_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->loadmap_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->cct_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->cct_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->met_tbl_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->met_tbl_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->sm_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->sm_end,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->footer_start,fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&x->HPCRUNsm,fs));

  if(x->HPCRUNsm != HPCRUNsm){
    //fprintf(stderr, "ERROR: hpcrun output file is incomplete due to wrong HPCRUNsm! Value read: 0x%" PRIx64 ", expected: 0x%" PRIx64 "\n", x->HPCRUNsm, HPCRUNsm);
    return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}

int
hpcrun_fmt_footer_fprint(hpcrun_fmt_footer_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "[footer:\n");
  fprintf(fs, "%s[           hdr start: %ld, end: %ld]\n", pre, x->hdr_start, x->hdr_end);
  fprintf(fs, "%s[       loadmap start: %ld, end: %ld]\n", pre, x->loadmap_start, x->loadmap_end);
  fprintf(fs, "%s[           cct start: %ld, end: %ld]\n", pre, x->cct_start, x->cct_end);
  fprintf(fs, "%s[    metric-tbl start: %ld, end: %ld]\n", pre, x->met_tbl_start, x->met_tbl_end);
  fprintf(fs, "%s[sparse metrics start: %ld, end: %ld]\n", pre, x->sm_start, x->sm_end);
  fprintf(fs, "%s[        footer start: %ld]\n", pre, x->footer_start);

  char* check_magic_num = "NO!";
  if(x->HPCRUNsm == HPCRUNsm) check_magic_num = "YES!"; 
  fprintf(fs, "%s[        MAGIC NUMBER: equal to the expected? %s]\n", pre, check_magic_num);

  fprintf(fs,"]\n");


  return HPCFMT_OK;
}


//***************************************************************************
// hpcrun_sparse_file - YUMENG
//
// File sections in order:
// hdr, loadmap, ccts, metric-tbl, sparse metrics, footer
//***************************************************************************
hpcrun_sparse_file_t* hpcrun_sparse_open(const char* path, size_t start_pos, size_t end_pos)
{
  FILE* fs = hpcio_fopen_r(path);
  if(!fs) return NULL;

  hpcrun_sparse_file_t* sparse_fs = (hpcrun_sparse_file_t*) malloc(sizeof(hpcrun_sparse_file_t));
  sparse_fs->file = fs;
  sparse_fs->mode = OPENED;
  sparse_fs->cur_pos = start_pos;
  sparse_fs->start_pos = start_pos;
  sparse_fs->end_pos = end_pos;

  sparse_fs->cct_nodes_read    = 0;    //number of cct nodes that have been read
  sparse_fs->metric_bytes_read = 0;    //number of bytes for metric-tbl section that have been read
  sparse_fs->cur_metric_id     = 0;    //keep track of id for metrics
  sparse_fs->lm_bytes_read     = 0;    //number of bytes for loadmap section that have been read
  sparse_fs->sm_block_touched  = 0;    //number of sparse metrics blocks that have been touched (entries might not have been read yet)
                                       //(block = a chunk containing value and metric id pairs for one cct node)

  //initialize footer
  if(end_pos == 0) {
    fseek(fs, 0, SEEK_END);
    sparse_fs->end_pos = end_pos = ftell(fs);
  }
  size_t footer_position = end_pos - SF_footer_SIZE;
  fseek(fs, footer_position, SEEK_SET);
  int ret = hpcrun_fmt_footer_fread(&(sparse_fs->footer), fs);
  if(ret != HPCFMT_OK){
    hpcio_fclose(fs);
    free(sparse_fs);
    return NULL;
  }
  hpcrun_sparse_footer_update_w_start(&(sparse_fs->footer), start_pos); //temp
  fseek(fs, sparse_fs->footer.hdr_start, SEEK_SET); 

  return sparse_fs;
}

//TEMPARARY function: we concatenate hpcrun files into one giant file for experiments
// so we need to update the footer
// TODO in the future: if hpcrun output is one file at the beginning,
// the footer info should already be correct
void hpcrun_sparse_footer_update_w_start(hpcrun_fmt_footer_t *f, size_t start_pos)
{
  f->hdr_start     += start_pos;
  f->hdr_end       += start_pos;
  f->loadmap_start += start_pos;
  f->loadmap_end   += start_pos;
  f->cct_start     += start_pos;
  f->cct_end       += start_pos;
  f->met_tbl_start += start_pos;
  f->met_tbl_end   += start_pos;
  f->sm_start      += start_pos;
  f->sm_end        += start_pos;
  f->footer_start  += start_pos;
  
}

/* succeed: return 0; fail: return 1; */
int hpcrun_sparse_pause(hpcrun_sparse_file_t* sparse_fs)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  sparse_fs->cur_pos = ftell(sparse_fs->file);
  ret = hpcio_fclose(sparse_fs->file);
  if(!ret) sparse_fs->mode = PAUSED;
  return ret;
}

/* succeed: return 0; fail open: return 1; was open already or current position out of range: return -1 */
int hpcrun_sparse_resume(hpcrun_sparse_file_t* sparse_fs, const char* path)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, PAUSED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  FILE* fs = hpcio_fopen_r(path);
  if(!fs) return SF_FAIL;
  if((sparse_fs->cur_pos < sparse_fs->start_pos)
    ||(sparse_fs->cur_pos >= sparse_fs->end_pos))
    return SF_ERR;
  sparse_fs->file = fs;
  fseek(fs, sparse_fs->cur_pos, SEEK_SET);
  sparse_fs->mode = OPENED;
  return SF_SUCCEED;
}

void hpcrun_sparse_close(hpcrun_sparse_file_t* sparse_fs)
{
  if(sparse_fs->mode == OPENED) hpcio_fclose(sparse_fs->file);
  free(sparse_fs);
}

/* check if current mode is as expected, yes: 0; no: -1 */
int hpcrun_sparse_check_mode(hpcrun_sparse_file_t* sparse_fs, bool expected, const char* msg)
{
  if(sparse_fs->mode != expected){
    fprintf(stderr, "ERROR: %s: hpcrun_sparse_file object's current state is %s, not as expected %s\n", 
      msg, MODE(sparse_fs->mode), MODE(expected));
    return SF_ERR;
  }
  return SF_SUCCEED;
}


/* succeed: returns 0; error while reading: returns -1 */
int hpcrun_sparse_read_hdr(hpcrun_sparse_file_t* sparse_fs, hpcrun_fmt_hdr_t* hdr)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  fseek(sparse_fs->file, sparse_fs->footer.hdr_start, SEEK_SET);
  ret = hpcrun_fmt_hdr_fread(hdr, sparse_fs->file, malloc);
  if(ret != HPCFMT_OK) return SF_ERR;
  if(ftell(sparse_fs->file) != sparse_fs->footer.hdr_end) return SF_ERR;
  return SF_SUCCEED;
}

/* succeed: returns positive id; End of list: returns 0; Fail reading: returns -1; */
int hpcrun_sparse_next_lm(hpcrun_sparse_file_t* sparse_fs, loadmap_entry_t* lm)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  if(sparse_fs->lm_bytes_read == 0) sparse_fs->lm_bytes_read = SF_num_lm_SIZE; //the first lm should skip the info about number of lms

  size_t realoffset = sparse_fs->footer.loadmap_start + sparse_fs->lm_bytes_read;
  if(realoffset == sparse_fs->footer.loadmap_end) return SF_END; // no more next lm
  if(realoffset > sparse_fs->footer.loadmap_end)  return SF_ERR;
  fseek(sparse_fs->file, realoffset, SEEK_SET);
  HPCFMT_ThrowIfError(hpcrun_fmt_loadmapEntry_fread(lm, sparse_fs->file, malloc));
  sparse_fs->lm_bytes_read += ftell(sparse_fs->file) - realoffset;

  return (lm) ? lm->id : SF_ERR;
}

/* succeed: returns a metric ID; end of list: returns 0; error: returns -1 */
int hpcrun_sparse_next_metric(hpcrun_sparse_file_t* sparse_fs, metric_desc_t* m, metric_aux_info_t* perf_info,double fmtVersion)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  if(sparse_fs->metric_bytes_read == 0) sparse_fs->metric_bytes_read = SF_num_metric_SIZE; //the first metric should skip the info about number of metrics

  size_t realoffset = sparse_fs->footer.met_tbl_start + sparse_fs->metric_bytes_read;
  if(realoffset == sparse_fs->footer.met_tbl_end) return SF_END; // no more next metric
  if(realoffset > sparse_fs->footer.met_tbl_end)  return SF_ERR; 
  fseek(sparse_fs->file, realoffset, SEEK_SET);
  HPCFMT_ThrowIfError(hpcrun_fmt_metricDesc_fread(m, perf_info, sparse_fs->file, fmtVersion, malloc));
  sparse_fs->cur_metric_id += 1;
  sparse_fs->metric_bytes_read += (ftell(sparse_fs->file) - realoffset);

  return sparse_fs->cur_metric_id;
}

/* succeed: returns a cct ID; end of list: returns 0; error: returns -1 */
int hpcrun_sparse_next_context(hpcrun_sparse_file_t* sparse_fs, hpcrun_fmt_cct_node_t* node)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;
  
  //first time initialization
  if(sparse_fs->cct_nodes_read == 0){ 
    fseek(sparse_fs->file, sparse_fs->footer.cct_start, SEEK_SET);
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&sparse_fs->num_cct_nodes, sparse_fs->file));
  }
  if(sparse_fs->cct_nodes_read == sparse_fs->num_cct_nodes) return SF_END;

  size_t realoffset = sparse_fs->footer.cct_start + (SF_cct_node_SIZE * sparse_fs->cct_nodes_read) + SF_num_cct_SIZE; 
  if(realoffset > sparse_fs->footer.cct_end) return SF_ERR;
  fseek(sparse_fs->file, realoffset, SEEK_SET);
  epoch_flags_t fake = {0};//need to remove in the future
  HPCFMT_ThrowIfError(hpcrun_fmt_cct_node_fread(node, fake, sparse_fs->file));
  sparse_fs->cct_nodes_read ++;
  return node->id;
}

/* succeed: returns 0; error while reading, or length == 0: returns -1 */
int hpcrun_sparse_read_id_tuple(hpcrun_sparse_file_t* sparse_fs, id_tuple_t* id_tuple)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  fseek(sparse_fs->file, sparse_fs->footer.sm_start, SEEK_SET);
  ret = id_tuple_fread(id_tuple, sparse_fs->file);
  if(id_tuple->length == 0) return SF_ERR;
  if(ret != HPCFMT_OK) return SF_ERR;

  return SF_SUCCEED;
}


/* succeed: returns a cct ID that we can read next_entry for; end of list: returns 0; error: returns -1 */
int hpcrun_sparse_next_block(hpcrun_sparse_file_t* sparse_fs)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  //first time initialization 
  if(sparse_fs->sm_block_touched == 0){
    int id_tuple_size;
    uint16_t id_tuple_length;
    fseek(sparse_fs->file, sparse_fs->footer.sm_start, SEEK_SET);
    HPCFMT_ThrowIfError(hpcfmt_int2_fread(&id_tuple_length, sparse_fs->file));
    id_tuple_size = PMS_id_tuple_len_SIZE + PMS_id_SIZE * id_tuple_length;
    fseek(sparse_fs->file, (sparse_fs->footer.sm_start + id_tuple_size), SEEK_SET);
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(sparse_fs->num_nzval),sparse_fs->file));
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(sparse_fs->num_nz_cct_nodes),sparse_fs->file));
    sparse_fs->val_mid_offset     = sparse_fs->footer.sm_start + id_tuple_size + SF_num_val_SIZE + SF_num_nz_cct_node_SIZE;
    sparse_fs->cct_node_id_idx_offset = sparse_fs->val_mid_offset + (SF_mid_SIZE + SF_val_SIZE) * sparse_fs->num_nzval; 
  }
  if(sparse_fs->sm_block_touched == sparse_fs->num_nz_cct_nodes) return SF_END; //no more cct block

  //read the first val_metricID pair position related to this cct
  size_t realoffset = sparse_fs->cct_node_id_idx_offset + (SF_cct_node_id_SIZE + SF_cct_node_idx_SIZE) * sparse_fs->sm_block_touched;
  if(realoffset > sparse_fs->footer.sm_end) return SF_ERR;
  fseek(sparse_fs->file, realoffset, SEEK_SET);
  size_t val_mid_idx;
  uint32_t cct_node_id;
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&cct_node_id,sparse_fs->file));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&val_mid_idx,sparse_fs->file));

  //set up records for this current block
  sparse_fs->cur_block_start = sparse_fs->val_mid_offset + (SF_mid_SIZE + SF_val_SIZE) * val_mid_idx;
  fseek(sparse_fs->file, SF_cct_node_id_SIZE, SEEK_CUR);
  uint64_t next_block_start_idx;
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&next_block_start_idx,sparse_fs->file));
  sparse_fs->cur_block_end = sparse_fs->val_mid_offset + (SF_mid_SIZE + SF_val_SIZE) * next_block_start_idx;
  sparse_fs->sm_block_touched++;

  //seek to the first val_metricID place
  fseek(sparse_fs->file,(sparse_fs->val_mid_offset + (SF_mid_SIZE + SF_val_SIZE) * val_mid_idx), SEEK_SET); 

  return cct_node_id;
}

/* succeed: returns positive metricID (matching metricTbl, start from 1); end of this block: 0;error: return -1*/
/* ASSUMPTION: it is called continously for one block, i.e. no other fseek happen between calls */
int hpcrun_sparse_next_entry(hpcrun_sparse_file_t* sparse_fs, hpcrun_metricVal_t* val)
{
  int ret = hpcrun_sparse_check_mode(sparse_fs, OPENED, __func__);
  if(ret != SF_SUCCEED) return SF_ERR;

  if(sparse_fs->sm_block_touched == 0){
    fprintf(stderr, "ERROR: hpcrun_sparse_next_entry(...) has to be called after hpcrun_sparse_next_block(...) to set up entry point.\n");
    return SF_ERR;
  }
  size_t cur_pos = ftell(sparse_fs->file);
  size_t cur_block_start_pos = sparse_fs->cur_block_start;
  size_t cur_block_end_pos   = sparse_fs->cur_block_end;
  if(cur_pos > sparse_fs->footer.sm_end || cur_block_start_pos > sparse_fs->footer.sm_end || cur_block_end_pos > sparse_fs->footer.sm_end) return SF_ERR;
  if((cur_pos < cur_block_start_pos) || (cur_pos > cur_block_end_pos)){
    fprintf(stderr, "ERROR: cannot read next entry for current cct: current position of hpcrun_sparse_file object is not within curren cct block's range.\n");
    return SF_ERR;
  }
  if(cur_pos == cur_block_end_pos) return SF_END; 

  uint16_t mid;
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(val->bits),sparse_fs->file));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&mid,sparse_fs->file));
  mid ++; //match the metric id in metricTbl(starting as 1), it was recorded starting as 0

  return mid;
}



//***************************************************************************
// hpctrace (located here for now)
//***************************************************************************

//***************************************************************************
// [hpctrace] hdr
//***************************************************************************

const hpctrace_hdr_flags_t hpctrace_hdr_flags_NULL = 0;


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
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->flags), infs));
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

  uint64_t flag_bits = flags;
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
  int nw;

  nw = fwrite(HPCTRACE_FMT_Magic,   1, HPCTRACE_FMT_MagicLen, fs);
  if (nw != HPCTRACE_FMT_MagicLen) return HPCFMT_ERR;

  nw = fwrite(HPCTRACE_FMT_Version, 1, HPCTRACE_FMT_VersionLen, fs);
  if (nw != HPCTRACE_FMT_VersionLen) return HPCFMT_ERR;

  nw = fwrite(HPCTRACE_FMT_Endian,  1, HPCTRACE_FMT_EndianLen, fs);
  if (nw != HPCTRACE_FMT_EndianLen) return HPCFMT_ERR;

  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(flags, fs));

  return HPCFMT_OK;
}


int
hpctrace_fmt_hdr_fprint(hpctrace_fmt_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCTRACE_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %s)\n", hdr->versionStr);
  fprintf(fs, "  (endian: %c)\n", hdr->endian);
  fprintf(fs, "  (flags: 0x%"PRIx64")\n", hdr->flags);
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

  ret = hpcfmt_int8_fread(&(x->comp), fs);
  if (ret != HPCFMT_OK) {
    return ret; // can be HPCFMT_EOF
  }

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->cpId), fs));

  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->metricId), fs));
  }
  else {
    x->metricId = HPCTRACE_FMT_MetricId_NULL;
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

  uint64_t comp = x->comp;
  for (shift = 56; shift >= 0; shift -= 8) {
    buf[k] = (comp >> shift) & 0xff;
    k++;
  }

  uint32_t cpId = x->cpId;
  for (shift = 24; shift >= 0; shift -= 8) {
    buf[k] = (cpId >> shift) & 0xff;
    k++;
  }

  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
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
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x->comp, outfs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->cpId, outfs));
  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x->metricId, outfs));
  }

  return HPCFMT_OK;
}

char*
hpctrace_fmt_datum_swrite(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  char* buf)
{
  buf = hpcfmt_int8_swrite(x->comp, buf);
  buf = hpcfmt_int4_swrite(x->cpId, buf);
  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
    buf = hpcfmt_int4_swrite(x->metricId, buf);
  }
  return buf;
}


int
hpctrace_fmt_datum_fprint(hpctrace_fmt_datum_t* x, hpctrace_hdr_flags_t flags,
			  FILE* fs)
{
  fprintf(fs, "(%llu, %u", HPCTRACE_FMT_GET_TIME(x->comp), x->cpId);
  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS)) {
    fprintf(fs, ", %llu",  HPCTRACE_FMT_GET_DLCA(x->comp));
  }
  if (HPCTRACE_HDR_FLAGS_GET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
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
  int nw;

  nw = fwrite(HPCMETRICDB_FMT_Magic,   1, HPCMETRICDB_FMT_MagicLen, outfs);
  if (nw != HPCTRACE_FMT_MagicLen) return HPCFMT_ERR;

  nw = fwrite(HPCMETRICDB_FMT_Version, 1, HPCMETRICDB_FMT_VersionLen, outfs);
  if (nw != HPCMETRICDB_FMT_VersionLen) return HPCFMT_ERR;

  nw = fwrite(HPCMETRICDB_FMT_Endian,  1, HPCMETRICDB_FMT_EndianLen, outfs);
  if (nw != HPCMETRICDB_FMT_EndianLen) return HPCFMT_ERR;

  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(hdr->numNodes, outfs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(hdr->numMetrics, outfs));

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

