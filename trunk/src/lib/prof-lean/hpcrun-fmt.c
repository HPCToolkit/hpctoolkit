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
//   hpcfile_csprof.c
//
// Purpose:
//   Low-level types and functions for reading/writing a call path
//   profile as formatted data.
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

#include "hpcrun-fmt.h"
#include "hpcfmt.h"

//*************************** Forward Declarations **************************
int
nvpairs_vfwrite(FILE *out, va_list args)
{
  va_list _tmp;
  va_copy(_tmp, args);

  uint32_t len = 0;

  for (char *arg = va_arg(_tmp, char *); arg != END_NVPAIRS; arg = va_arg(_tmp, char *)) {
    arg = va_arg(_tmp, char *);
    len++;
  }
  va_end(_tmp);

  hpcio_fwrite_le4(&len, out);

  for (char *arg = va_arg(args, char *); arg != END_NVPAIRS; arg = va_arg(args, char *)) {
    hpcrun_fstr_fwrite(arg, out); // write NAME
    arg = va_arg(args, char *);
    hpcrun_fstr_fwrite(arg, out); // write VALUE
  }
  return HPCFILE_OK;
}

//*************************** Main Code **************************

#define DBG_READ_METRICS 0

int hpcfile_csprof_read_hdr(FILE* fs, hpcfile_csprof_hdr_t* hdr);

hpcfile_metric_data_t hpcfile_metric_data_ZERO = { .bits = 0 };

int
hpcrun_fmt_nvpair_t_fwrite(nvpair_t *nvp, FILE *outfs)
{
  THROW_ERR(hpcrun_fstr_fwrite(nvp->name, outfs));
  THROW_ERR(hpcrun_fstr_fwrite(nvp->val, outfs));

  return HPCFILE_OK;
}

int
hpcrun_fmt_nvpair_t_fread(nvpair_t *inp, FILE *infs, alloc_fn alloc)
{
  hpcrun_fstr_fread(&(inp->name), infs, alloc);
  hpcrun_fstr_fread(&(inp->val), infs, alloc);

  return HPCFILE_OK;
}

int
hpcrun_fmt_nvpair_t_fprint(nvpair_t *nvp, FILE *outfs)
{
  fprintf(outfs,"NV(%s,%s)\n", nvp->name, nvp->val);
  return HPCFILE_OK;
}

// static eventually ?? 
int
hpcrun_fmt_list_of_nvpair_t_fread(LIST_OF(nvpair_t) *nvps, FILE *in, alloc_fn alloc)
{
  hpcio_fread_le4(&(nvps->len), in);
  if (alloc != NULL) {
    nvps->lst = (nvpair_t *) alloc(nvps->len * sizeof(nvpair_t));
  }
  for (uint32_t i=0; i<nvps->len; i++){
    hpcrun_fmt_nvpair_t_fread(&(nvps->lst[i]), in, alloc);
  }
  return HPCFILE_OK;
}

// static eventually ??
int
hpcrun_fmt_list_of_nvpair_t_fprint(LIST_OF(nvpair_t) *nvps, FILE *out)
{
  nvpair_t *p = nvps->lst;
  for (uint32_t i=0; i<nvps->len; i++,p++){
    hpcrun_fmt_nvpair_t_fprint(p, out);
  }
  return HPCFILE_OK;
}

int
hpcrun_fmt_hdr_fwrite(FILE *outfs, ...)
{
  va_list args;
  va_start(args, outfs);

  fwrite(MAGIC, 1, strlen(MAGIC), outfs);

  nvpairs_vfwrite(outfs, args);

  va_end(args);

  return HPCFILE_OK;
}

int
hpcrun_fmt_hdr_fread(hpcrun_fmt_hdr_t *hdr, FILE* infs, alloc_fn alloc)
{

  fread(&(hdr->tag), 1, strlen(MAGIC), infs);
  hdr->tag[strlen(MAGIC)] = '\0';
  hpcrun_fmt_list_of_nvpair_t_fread(&(hdr->nvps), infs, alloc);

  return HPCFILE_OK;
}

int
hpcrun_fmt_hdr_fprint(hpcrun_fmt_hdr_t *hdr, FILE* out)
{
  fwrite(&(hdr->tag), 1, sizeof(hdr->tag), out);
  fwrite("\n", 1, 1, out);
  hpcrun_fmt_list_of_nvpair_t_fprint(&(hdr->nvps), out);
  return HPCFILE_OK;
}


// HPC_CSPROF format details: 
//   List of tagged data chunks that represent data in 'hpcfile_csprof_data_t'
//   hpcfile_str_t, hpcfile_num8_str_t, etc.
//

//***************************************************************************
// hpcfile_csprof_write()
//***************************************************************************

// See header file for documentation of public interface.
// Cf. 'HPC_CSPROF format details' above.
int
hpcfile_csprof_write(FILE* fs, hpcfile_csprof_data_t* data)
{
#if defined(INCLUDE_OLD_H)
  hpcfile_csprof_hdr_t fhdr;
#endif

  hpcfile_str_t str;
  hpcfile_num8_t num8;
  int i;

  if (!fs) { return HPCFILE_ERR; }

#if defined(INCLUDE_OLD_H) // old header

  // Write header 
  hpcfile_csprof_hdr__init(&fhdr);
  fhdr.num_data = 3;
  if (hpcfile_csprof_hdr__fwrite(&fhdr, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
#endif // old header

  // ----------------------------------------------------------
  // 1. Target: HPCFILE_TAG__CSPROF_TARGET
  // ----------------------------------------------------------
  hpcfile_str__init(&str);
  str.tag = HPCFILE_TAG__CSPROF_TARGET;
  if (data->target) {
    str.length = strlen(data->target);
    str.str = data->target;
  }
  if (hpcfile_str__fwrite(&str, fs) != HPCFILE_OK) { return HPCFILE_ERR; }


  // ----------------------------------------------------------
  // 2. Metrics
  // ----------------------------------------------------------
  hpcio_fwrite_le4(&data->num_metrics, fs);

  for (i = 0; i < data->num_metrics; ++i) {
    hpcfile_csprof_metric_t metric = data->metrics[i];
    
    hpcfile_str__init(&str);
    str.tag = HPCFILE_TAG__CSPROF_EVENT;
    str.length = strlen(metric.metric_name);
    str.str = metric.metric_name;
    if(hpcfile_str__fwrite(&str, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
    
    hpcfile_num8__init(&num8);
    num8.tag = HPCFILE_TAG__CSPROF_METRIC_FLAGS;
    num8.num = metric.flags;
    if (hpcfile_num8__fwrite(&num8, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
    
    num8.tag = HPCFILE_TAG__CSPROF_PERIOD;
    num8.num = metric.sample_period;
    if (hpcfile_num8__fwrite(&num8, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

//***************************************************************************
// hpcfile_csprof_read()
//***************************************************************************

// See header file for documentation of public interface.
// Cf. 'HPC_CSPROF format details' above.
int
hpcfile_csprof_read(FILE* fs, hpcfile_csprof_data_t* data,  
                    epoch_table_t* loadmap_tbl,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn)
{

#if defined(INCLUDE_OLD_H)
  hpcfile_csprof_hdr_t fhdr;
#endif

  hpcfile_str_t str;
  hpcfile_num8_t num8;
  uint32_t tag;
  size_t sz;
  int ret;

  if (!fs) { return HPCFILE_ERR; }

  hpcfile_csprof_data__init(data);
  
#if defined(INCLUDE_OLD_H) // old header
  // Read and sanity check header
  ret = hpcfile_csprof_read_hdr(fs, &fhdr);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }
#endif // old header
  
  // FIXME: TEMPORARY

  // ----------------------------------------------------------
  // 1. Target
  // ----------------------------------------------------------
  ret = hpcfile_tag__fread(&tag, fs); // HPCFILE_STR
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }
  
  str.str = NULL;
  ret = hpcfile_str__fread(&str, fs, alloc_fn);
  if (ret != HPCFILE_OK) { 
    free_fn(str.str);
    return HPCFILE_ERR;
  }
  
  data->target = str.str;

  
  // ----------------------------------------------------------
  // 2. Metrics
  // ----------------------------------------------------------

  // 2a. number of metrics
  ret = hpcfile_tag__fread(&tag, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  data->num_metrics = tag; // FIXME: YUCK

  // 2b. metric descriptions
  data->metrics = alloc_fn(data->num_metrics * sizeof(hpcfile_csprof_metric_t));
  
  for (uint32_t i = 0; i < data->num_metrics; ++i) {
    // Read metrics data tag
    ret = hpcfile_tag__fread(&tag, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    // read in the name of the  metric
    str.str = NULL;
    if (hpcfile_str__fread(&str, fs, alloc_fn) != HPCFILE_OK) {
      free_fn(str.str);
      return HPCFILE_ERR;
    }
    data->metrics[i].metric_name = str.str;

    // read in the flags of the metric
    ret = hpcfile_tag__fread(&tag, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    ret = hpcfile_num8__fread(&num8, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    data->metrics[i].flags = num8.num;
	  
    // read in the sample period
    ret = hpcfile_tag__fread(&tag, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    ret = hpcfile_num8__fread(&num8, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }
	  
    data->metrics[i].sample_period = num8.num;
  }


#if 0
  size_t sz;
  // Read data chunks
  for (i = 0; i < fhdr.num_data-1; ++i) {
    // Read data tag
    sz = hpcio_fread_le4(&tag, fs);
    if (sz != sizeof(tag)) { return HPCFILE_ERR; }

    // Read data into appropriate structure
    switch (HPCFILE_TAG_GET_FORMAT(tag)) {
      case HPCFILE_STR:
	str.str = NULL;
	if (hpcfile_str__fread(&str, fs, alloc_fn) != HPCFILE_OK) { 
	  free_fn(str.str);
	  return HPCFILE_ERR;
	}

	data->target = str.str; 

	break;
      case HPCFILE_NUM8:
	ret = hpcfile_num8__fread(&num8, fs);
	if (ret != HPCFILE_OK) { 

	}
	break;

      case HPCFILE_NUM8S:
	num8s.nums = NULL;
	if (hpcfile_num8s__fread(&num8s, fs, alloc_fn) != HPCFILE_OK) { 
	  free_fn(num8s.nums);
	  return HPCFILE_ERR;
	}
	break;

      default:
	return HPCFILE_ERR; 
	break;
    }
    
    // Interpret the data: FIXME sanity check
    switch (tag) {
    case HPCFILE_TAG__CSPROF_TARGET: 
      data->target = str.str; 
      break;
    case HPCFILE_TAG__CSPROF_EVENT:
      data->event = str.str; 
      break;
    case HPCFILE_TAG__CSPROF_PERIOD:
      data->sample_period = num8.num;
      break;
    default: 
      break; // skip 
    }
  }
#endif
    
  // ----------------------------------------------------------
  // 3. LoadMaps
  // ----------------------------------------------------------

  // read number of loadmaps
  uint32_t num_loadmap;
  sz = hpcio_fread_le4(&num_loadmap, fs);
  if (sz != sizeof(num_loadmap)) { 
    return HPCFILE_ERR; 
  }

  // read loadmaps 
  loadmap_tbl->num_epoch = num_loadmap; 
  loadmap_tbl->epoch_modlist = alloc_fn(num_loadmap * sizeof(epoch_entry_t));

  for (int i = 0; i < num_loadmap; ++i) {
    uint32_t num_modules;
    hpcio_fread_le4(&num_modules, fs); 

    loadmap_tbl->epoch_modlist[i].num_loadmodule = num_modules; 
    loadmap_tbl->epoch_modlist[i].loadmodule = 
      alloc_fn(num_modules * sizeof(ldmodule_t));
    
    for (int j = 0; j < num_modules; ++j) { 
      uint64_t vaddr, mapaddr;
      str.str = NULL; 
      if (hpcfile_str__fread(&str, fs, alloc_fn) != HPCFILE_OK) {
	free_fn(str.str);
	return HPCFILE_ERR;
      }
      loadmap_tbl->epoch_modlist[i].loadmodule[j].name = str.str;
      hpcio_fread_le8(&vaddr, fs);
      hpcio_fread_le8(&mapaddr, fs); 
      loadmap_tbl->epoch_modlist[i].loadmodule[j].vaddr = vaddr;
      loadmap_tbl->epoch_modlist[i].loadmodule[j].mapaddr = mapaddr;
    }
  }
  
  // ----------------------------------------------------------
  // 4. 
  // ----------------------------------------------------------
  uint64_t num_tramp_samples;
  
  sz = hpcio_fread_le4(&data->num_ccts, fs);
  if (sz != sizeof(data->num_ccts)) { 
    return HPCFILE_ERR; 
  }

  sz = hpcio_fread_le8(&num_tramp_samples, fs);
  if (sz != sizeof(num_tramp_samples)) { 
    return HPCFILE_ERR; 
  }

  // ----------------------------------------------------------
  // 
  // ----------------------------------------------------------
  
  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  return ret;
}

// See header file for documentation of public interface.
int
hpcfile_csprof_fprint(FILE* infs, FILE* outfs, hpcfile_csprof_data_t* data)
{
  int ret;

  // Read header and basic data
  epoch_table_t loadmap_tbl;

  ret = hpcfile_csprof_read(infs, data, &loadmap_tbl, 
			    (hpcfile_cb__alloc_fn_t)malloc,
			    (hpcfile_cb__free_fn_t)free);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  // Print header
  //-- hpcfile_csprof_id__fprint();
  //-- hpcfile_csprof_hdr__fprint(&fhdr, outfs);
  //-- fputs("\n", outfs);

  // Print data
  hpcfile_csprof_data__fprint(data, outfs);

  // Print loadmaps
  for (int i = 0; i < loadmap_tbl.num_epoch; ++i) {
    // print a loadmap
    epoch_entry_t* loadmap = &loadmap_tbl.epoch_modlist[i];
    fprintf(outfs, "{loadmap %d:\n", i);

    for (int j = 0; j < loadmap->num_loadmodule; ++j) {
      ldmodule_t* lm = & loadmap->loadmodule[j];

      fprintf(outfs, "  lm %d: %s %"PRIx64" -> %"PRIx64"\n",
	      j, lm->name, lm->vaddr, lm->mapaddr);
    }
    fprintf(outfs, "}\n");
  }

  epoch_table__free_data(&loadmap_tbl, (hpcfile_cb__free_fn_t)free);

  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  return HPCFILE_OK;
}

//***************************************************************************

// hpcfile_csprof_read_hdr: Read the cstree header from the file
// stream 'fs' into 'hdr' and sanity check header info.  Returns
// HPCFILE_OK upon success; HPCFILE_ERR on error.
int
hpcfile_csprof_read_hdr(FILE* fs, hpcfile_csprof_hdr_t* hdr)
{
  // Read header
  if (hpcfile_csprof_hdr__fread(hdr, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Sanity check file id
  if (strncmp(hdr->fid.magic_str, HPCFILE_CSPROF_MAGIC_STR, 
	      HPCFILE_CSPROF_MAGIC_STR_LEN) != 0) { 
    return HPCFILE_ERR; 
  }
  if (strncmp(hdr->fid.version, HPCFILE_CSPROF_VERSION, 
	      HPCFILE_CSPROF_VERSION_LEN) != 0) { 
    return HPCFILE_ERR; 
  }
  if (hdr->fid.endian != HPCFILE_CSPROF_ENDIAN) { return HPCFILE_ERR; }
  
  // Sanity check header [nothing needed]
  
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_csprof_id__init(hpcfile_csprof_id_t* x)
{
  memset(x, 0, sizeof(*x));

  strncpy(x->magic_str, HPCFILE_CSPROF_MAGIC_STR,
	  HPCFILE_CSPROF_MAGIC_STR_LEN);
  strncpy(x->version, HPCFILE_CSPROF_VERSION, HPCFILE_CSPROF_VERSION_LEN);
  x->endian = HPCFILE_CSPROF_ENDIAN;

  return HPCFILE_OK;
}

int 
hpcfile_csprof_id__fini(hpcfile_csprof_id_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_csprof_id__fread(hpcfile_csprof_id_t* x, FILE* fs)
{
  size_t sz;
  int c;

  sz = fread((char*)x->magic_str, 1, HPCFILE_CSPROF_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSPROF_MAGIC_STR_LEN) { return HPCFILE_ERR; }
  
  sz = fread((char*)x->version, 1, HPCFILE_CSPROF_VERSION_LEN, fs);
  if (sz != HPCFILE_CSPROF_VERSION_LEN) { return HPCFILE_ERR; }
  
  c = fgetc(fs);
  if (c == EOF) { return HPCFILE_ERR; }
  x->endian = (char)c;

  return HPCFILE_OK;
}

int 
hpcfile_csprof_id__fwrite(hpcfile_csprof_id_t* x, FILE* fs)
{
  size_t sz;

  sz = fwrite((char*)x->magic_str, 1, HPCFILE_CSPROF_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSPROF_MAGIC_STR_LEN) { return HPCFILE_ERR; }

  sz = fwrite((char*)x->version, 1, HPCFILE_CSPROF_VERSION_LEN, fs);
  if (sz != HPCFILE_CSPROF_VERSION_LEN) { return HPCFILE_ERR; }
  
  if (fputc(x->endian, fs) == EOF) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_csprof_id__fprint(hpcfile_csprof_id_t* x, FILE* fs)
{
  fprintf(fs, "{fileid: (magic: %s) (ver: %s) (end: %c)}\n", 
	  x->magic_str, x->version, x->endian);

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_csprof_hdr__init(hpcfile_csprof_hdr_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_csprof_id__init(&x->fid);
  
  return HPCFILE_OK;
}

int 
hpcfile_csprof_hdr__fini(hpcfile_csprof_hdr_t* x)
{
  hpcfile_csprof_id__fini(&x->fid);
  return HPCFILE_OK;
}

int 
hpcfile_csprof_hdr__fread(hpcfile_csprof_hdr_t* x, FILE* fs)
{
  size_t sz;
    
  if (hpcfile_csprof_id__fread(&x->fid, fs) != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fread_le8(&x->num_data, fs);
  if (sz != sizeof(x->num_data)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_csprof_hdr__fwrite(hpcfile_csprof_hdr_t* x, FILE* fs)
{
  size_t sz;
  
  if (hpcfile_csprof_id__fwrite(&x->fid, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fwrite_le8(&x->num_data, fs);
  if (sz != sizeof(x->num_data)) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}

int 
hpcfile_csprof_hdr__fprint(hpcfile_csprof_hdr_t* x, FILE* fs)
{
  fputs("{csprof_hdr:\n", fs);
  
  hpcfile_csprof_id__fprint(&x->fid, fs);

  fprintf(fs, "(num_data: %"PRIu64")}\n", x->num_data);
  
  return HPCFILE_OK;
}

//***************************************************************************


void 
epoch_table__free_data(epoch_table_t* x, hpcfile_cb__free_fn_t free_fn)
{
  for (int i = 0; i < x->num_epoch; ++i) {
    epoch_entry_t* loadmap = & x->epoch_modlist[i];
    for (int j = 0; j < loadmap->num_loadmodule; ++j) {
      ldmodule_t* lm = & loadmap->loadmodule[j];
      free_fn(lm->name);
    }
    free_fn(loadmap->loadmodule);
  }
  free_fn(x->epoch_modlist);
}


//***************************************************************************

int 
hpcfile_csprof_data__init(hpcfile_csprof_data_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}

int 
hpcfile_csprof_data__fini(hpcfile_csprof_data_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_csprof_data__fprint(hpcfile_csprof_data_t* x, FILE* fs)
{
  fputs("{csprof_data:\n", fs);

  char* tgt = (x->target) ? x->target : "";
  fprintf(fs, "  target: %s\n", tgt);

  fprintf(fs, "  metrics: %d\n", x->num_metrics);
  for (int i = 0; i < x->num_metrics; ++i) {
    hpcfile_csprof_metric_t metric = x->metrics[i];
    
    // (metric.metric_name)
    fprintf(fs, "  { event: %s\t", metric.metric_name); 
    fprintf(fs, "  sample period: %"PRIu64" }\n", metric.sample_period); 
  }
  fputs("  }\n", fs);
  
  fprintf(fs, "  num CCTs: %d\n", x->num_ccts);

  return HPCFILE_OK;
}

//***************************************************************************

// HPC_CSTREE format details: 
//
// As the nodes are written, each will be given a persistent id and
// each node will know its parent's persistent id.  The id numbers are
// between CSTREE_ID_ROOT and (CSTREE_ID_ROOT + n - 1) where n is the
// number of nodes in the tree.  Furthermore, parents are always
// written before any of their children.  This scheme allows
// hpcfile_cstree_read to easily and efficiently construct the tree
// since the persistent ids correspond to simple array indices and
// parent nodes will be read before children.
//
// The id's will be will be assigned so that given a node x and its
// children, the id of x is less than the id of any child and less
// than the id of any child's descendent.

// ---------------------------------------------------------
// Three cases for lips:
//   1) a-to-0:         trivial
//   2) M-to-1, 1-to-1: associate lip with root note
//   3) 1-to-M:         associate lip with each note
//
// 1. If a lip is necessary, it comes *after* the note (since note
//    association indicates whether a lip is needed) and receives the
//    id of that node.
//
// 2. We can pass chord root ids as an extra parameter in PREORDER walk.


//***************************************************************************

// hpcfile_cstree_read_hdr: Read the cstree header from the file
// stream 'fs' into 'hdr' and sanity check header info.  Returns
// HPCFILE_OK upon success; HPCFILE_ERR on error.
int
hpcfile_cstree_read_hdr(FILE* fs, hpcfile_cstree_hdr_t* hdr)
{
  int ret;
  
  // Read header
  ret = hpcfile_cstree_hdr__fread(hdr, fs);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Sanity check file id
  ret = strncmp(hdr->fid.magic_str, HPCFILE_CSTREE_MAGIC_STR, 
		HPCFILE_CSTREE_MAGIC_STR_LEN);
  if (ret != 0) {
    return HPCFILE_ERR; 
  }
  ret = strncmp(hdr->fid.version, HPCFILE_CSTREE_VERSION, 
		HPCFILE_CSTREE_VERSION_LEN);
  if (ret != 0) { 
    return HPCFILE_ERR; 
  }
  if (hdr->fid.endian != HPCFILE_CSTREE_ENDIAN) { 
    return HPCFILE_ERR; 
  }
  
  // Sanity check header
  if (hdr->vma_sz != 8) { 
    return HPCFILE_ERR; 
  }
  if (hdr->uint_sz != 8) { 
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_id__init(hpcfile_cstree_id_t* x)
{
  memset(x, 0, sizeof(*x));

  strncpy(x->magic_str, HPCFILE_CSTREE_MAGIC_STR, HPCFILE_CSTREE_MAGIC_STR_LEN);
  strncpy(x->version, HPCFILE_CSTREE_VERSION, HPCFILE_CSTREE_VERSION_LEN);
  x->endian = HPCFILE_CSTREE_ENDIAN;

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fini(hpcfile_cstree_id_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fread(hpcfile_cstree_id_t* x, FILE* fs)
{
  size_t sz;
  int c;

  sz = fread((char*)x->magic_str, 1, HPCFILE_CSTREE_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSTREE_MAGIC_STR_LEN) { return HPCFILE_ERR; }
  
  sz = fread((char*)x->version, 1, HPCFILE_CSTREE_VERSION_LEN, fs);
  if (sz != HPCFILE_CSTREE_VERSION_LEN) { return HPCFILE_ERR; }

  c = fgetc(fs);
  if (c == EOF) { return HPCFILE_ERR; }
  x->endian = (char)c;

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fwrite(hpcfile_cstree_id_t* x, FILE* fs)
{
  size_t sz;

  sz = fwrite((char*)x->magic_str, 1, HPCFILE_CSTREE_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSTREE_MAGIC_STR_LEN) { return HPCFILE_ERR; }

  sz = fwrite((char*)x->version, 1, HPCFILE_CSTREE_VERSION_LEN, fs);
  if (sz != HPCFILE_CSTREE_VERSION_LEN) { return HPCFILE_ERR; }
  
  if (fputc(x->endian, fs) == EOF) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fprint(hpcfile_cstree_id_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s{fileid: (magic: %s) (ver: %s) (end: %c)}\n", 
	  pre, x->magic_str, x->version, x->endian);

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_hdr__init(hpcfile_cstree_hdr_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_cstree_id__init(&x->fid);
  
  x->vma_sz  = sizeof(hpcfile_vma_t);
  x->uint_sz = sizeof(hpcfile_uint_t);

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fini(hpcfile_cstree_hdr_t* x)
{
  hpcfile_cstree_id__fini(&x->fid);
  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fread(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_cstree_id__fread(&x->fid, fs);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fread_le4(&x->vma_sz, fs);
  if (sz != sizeof(x->vma_sz)) { 
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fread_le4(&x->uint_sz, fs);
  if (sz != sizeof(x->uint_sz)) { 
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fread_le8(&x->num_nodes, fs);
  if (sz != sizeof(x->num_nodes)) { 
    return HPCFILE_ERR; 
  }

  sz = hpcio_fread_le4(&x->epoch, fs);
  if (sz != sizeof(x->epoch)) { 
    return HPCFILE_ERR; 
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fwrite(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_cstree_id__fwrite(&x->fid, fs);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  sz = hpcio_fwrite_le4(&x->vma_sz, fs);
  if (sz != sizeof(x->vma_sz)) { 
    return HPCFILE_ERR; 
  }
 
  sz = hpcio_fwrite_le4(&x->uint_sz, fs);
  if (sz != sizeof(x->uint_sz)) { 
    return HPCFILE_ERR; 
  }

  sz = hpcio_fwrite_le8(&x->num_nodes, fs);
  if (sz != sizeof(x->num_nodes)) { 
    return HPCFILE_ERR; 
  }

  sz = hpcio_fwrite_le4(&x->epoch, fs);
  if (sz != sizeof(x->epoch)) { 
    return HPCFILE_ERR; 
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fprint(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  const char* pre = "  ";
  fputs("{cstree_hdr:\n", fs);

  hpcfile_cstree_id__fprint(&x->fid, fs, pre);

  fprintf(fs, "%s(vma_sz: %u) (uint_sz: %u) (num_nodes: %"PRIu64")}\n", 
	  pre, x->vma_sz, x->uint_sz, x->num_nodes);
  
  return HPCFILE_OK;
}

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
  
  size_t sz;
  int i, ret;

  ret = hpcfile_cstree_as_info__fread(&x->as_info, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpcio_fread_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpcio_fread_le8(&x->lip.id, fs);
  if (sz != sizeof(x->lip.id)) { return HPCFILE_ERR; }

  sz = hpcio_fread_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }

  sz = hpcio_fread_le4(&x->cpid, fs);
  if (sz != sizeof(x->cpid)) { return HPCFILE_ERR; }

  //DIAG_MsgIf(DBG_READ_METRICS, "reading node ip=%"PRIx64, x->ip);
  for (i = 0; i < x->num_metrics; ++i) {
    sz = hpcio_fread_le8(&x->metrics[i].bits, fs);
    //DIAG_MsgIf(DBG_READ_METRICS, "metrics[%d]=%"PRIu64, i, x->metrics[i]);
    if (sz != sizeof(x->metrics[i])) {
      return HPCFILE_ERR;
    }
  }

  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs) 
{
  size_t sz;
  int i, ret;

  ret = hpcfile_cstree_as_info__fwrite(&x->as_info, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpcio_fwrite_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpcio_fwrite_le8(&x->lip.id, fs);
  if (sz != sizeof(x->lip.id)) { return HPCFILE_ERR; }

  sz = hpcio_fwrite_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }

  sz = hpcio_fwrite_le4(&x->cpid, fs);
  if (sz != sizeof(x->cpid)) { return HPCFILE_ERR; }

  for (i = 0; i < x->num_metrics; ++i) {
    sz = hpcio_fwrite_le8(&x->metrics[i].bits, fs);
    if (sz != sizeof(x->metrics[i])) {
      return HPCFILE_ERR;
    }
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs, 
				const char* pre)
{
  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(as_str, x->as_info);

  fprintf(fs, "%s{nodedata: (as: %s) (ip: 0x%"PRIx64") (lip: [%"PRIu64"][%p]) (sp: %"PRIx64") (cpid: %"PRIu32")\n", pre, as_str, x->ip, x->lip.id, x->lip.ptr, 
	  x->sp, x->cpid);

  fprintf(fs, "%s  (metrics:", pre);
  for (int i = 0; i < x->num_metrics; ++i) {
    fprintf(fs, " %"PRIu64" ", x->metrics[i].bits);
  }
  fprintf(fs, ") }\n");

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcio_fread_le4(&x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpcio_fwrite_le4(&x->bits, fs);
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
  size_t sz;

  //  HPCFILE_TAG__CSTREE_LIP has already been read

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    sz = hpcio_fread_le8(&x->data8[i], fs);
    if (sz != sizeof(x->data8[i])) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(HPCFILE_TAG__CSTREE_LIP, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    sz = hpcio_fwrite_le8(&x->data8[i], fs);
    if (sz != sizeof(x->data8[i])) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_lip__fprint(lush_lip_t* x, hpcfile_uint_t id, 
			   FILE* fs, const char* pre)
{
  char lip_str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(lip_str, x);

  fprintf(fs, "%s{lip:  (id: %"PRIu64") %s}\n", pre, id, lip_str);

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
  size_t sz;
  int ret;

  // HPCFILE_TAG__CSTREE_NODE has already been read

  sz = hpcio_fread_le8(&x->id, fs);
  if (sz != sizeof(x->id)) { 
    return HPCFILE_ERR; 
  }
  
  sz = hpcio_fread_le8(&x->id_parent, fs);
  if (sz != sizeof(x->id_parent)) { 
    return HPCFILE_ERR; 
  }
  
  ret = hpcfile_cstree_nodedata__fread(&x->data, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(HPCFILE_TAG__CSTREE_NODE, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }

  sz = hpcio_fwrite_le8(&x->id, fs);
  if (sz != sizeof(x->id)) { return HPCFILE_ERR; }
  
  sz = hpcio_fwrite_le8(&x->id_parent, fs);
  if (sz != sizeof(x->id_parent)) { return HPCFILE_ERR; }
  
  if (hpcfile_cstree_nodedata__fwrite(&x->data, fs) != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "{node: (id: %"PRIu64") (id_parent: %"PRIu64")}\n", 
	  x->id, x->id_parent);

  hpcfile_cstree_nodedata__fprint(&x->data, fs, pre);
  
  return HPCFILE_OK;
}

//***************************************************************************
