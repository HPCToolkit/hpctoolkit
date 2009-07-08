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

#include "hpcio.h"
#include "hpcrun-fmt.h"
#include "hpcfmt.h"

//*************************** Forward Declarations **************************
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

//*************************** Main Code **************************

#define DBG_READ_METRICS 0

int hpcfile_csprof_read_hdr(FILE* fs, hpcfile_csprof_hdr_t* hdr);

hpcrun_metric_data_t hpcrun_metric_data_ZERO = { .bits = 0 };

int
hpcrun_fmt_nvpair_t_fwrite(nvpair_t *nvp, FILE *outfs)
{
  THROW_HPC_ERR(hpcfmt_fstr_fwrite(nvp->name, outfs));
  THROW_HPC_ERR(hpcfmt_fstr_fwrite(nvp->val, outfs));

  return HPCFILE_OK;
}

int
hpcrun_fmt_nvpair_t_fread(nvpair_t *inp, FILE *infs, alloc_fn alloc)
{
  hpcfmt_fstr_fread(&(inp->name), infs, alloc);
  hpcfmt_fstr_fread(&(inp->val), infs, alloc);

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
  hpcfmt_byte4_fread(&(nvps->len), in);
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

//
//  ==== hdr ====
//

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

//
// === epoch hdr ====
//

int
hpcrun_fmt_epoch_hdr_fwrite(FILE *out, uint64_t flags, uint32_t ra_distance, uint64_t granularity, ...)
{
  va_list args;
  va_start(args, granularity);

  fwrite(EPOCH_TAG, 1, strlen(EPOCH_TAG), out);
  hpcfmt_byte8_fwrite(flags, out);
  hpcfmt_byte4_fwrite(ra_distance, out);
  hpcfmt_byte8_fwrite(granularity, out);
  nvpairs_vfwrite(out, args);
  va_end(args);

  return HPCFILE_OK;
}

int
hpcrun_fmt_epoch_hdr_fread(hpcrun_fmt_epoch_hdr_t *ehdr, FILE *fs, alloc_fn alloc)
{
  fread(&(ehdr->tag), 1, sizeof(EPOCH_TAG)-1, fs);
  ehdr->tag[sizeof(EPOCH_TAG)-1] = '\0';

  if (strcmp(ehdr->tag, EPOCH_TAG) != 0){
    fprintf(stderr,"invalid epoch header tag: %s\n", ehdr->tag);
    return HPCFILE_ERR;
  }

  hpcfmt_byte8_fread(&(ehdr->flags), fs);
  hpcfmt_byte4_fread(&(ehdr->ra_distance), fs);
  hpcfmt_byte8_fread(&(ehdr->granularity), fs);
  hpcrun_fmt_list_of_nvpair_t_fread(&(ehdr->nvps), fs, alloc);

  return HPCFILE_OK;
}

int
hpcrun_fmt_epoch_hdr_fprint(hpcrun_fmt_epoch_hdr_t *ehdr, FILE *out)
{
  fprintf(out,"{epoch tag: %s}\n", ehdr->tag);
  fprintf(out,"{epoch flags: %lx}\n",ehdr->flags);
  fprintf(out,"{epoch characteristic return address distance: %d}\n",ehdr->ra_distance);
  fprintf(out,"{epoch address granularity (bucket size): %ld}\n",ehdr->granularity);

  hpcrun_fmt_list_of_nvpair_t_fprint(&(ehdr->nvps), out);

  return HPCFILE_OK;
}

//
//   === metric table ===
//
int
hpcrun_fmt_metric_tbl_fwrite(metric_tbl_t *metric_tbl, FILE *out)
{
  hpcfmt_byte4_fwrite(metric_tbl->len, out);
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    hpcfmt_fstr_fwrite(p->name, out);
    hpcfmt_byte8_fwrite(p->flags, out);
    hpcfmt_byte8_fwrite(p->period, out);
  }
  return HPCFILE_OK;
}


int
hpcrun_fmt_metric_tbl_fread(metric_tbl_t *metric_tbl, FILE *in, alloc_fn alloc)
{
  hpcfmt_byte4_fread(&(metric_tbl->len), in);
  if (alloc != NULL) {
    metric_tbl->lst = (metric_desc_t *) alloc(metric_tbl->len * sizeof(metric_desc_t));
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
hpcrun_fmt_metric_tbl_fprint(metric_tbl_t *metric_tbl, FILE *out)
{
  fputs("{csprof_data:\n", out);

  fprintf(out, "  metrics: %d\n", metric_tbl->len);
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    fprintf(out, "  { event: %s\t", p->name); 
    fprintf(out, "  sample period: %"PRIu64" }\n", p->period); 
  }
  fputs("  }\n", out);
  
  return HPCFILE_OK;
}


void
hpcrun_fmt_metric_tbl_free(metric_tbl_t *metric_tbl, free_fn dealloc)
{
  metric_desc_t *p = metric_tbl->lst;
  for (uint32_t i = 0; i < metric_tbl->len; p++, i++) {
    hpcfmt_fstr_free(p->name, dealloc);
  }
  dealloc((void*)metric_tbl->lst);
  metric_tbl->lst = NULL;
}

//
//   === load map ===
//
int
hpcrun_fmt_loadmap_fwrite(uint32_t num_modules, loadmap_src_t *src, FILE *out)
{
  hpcfmt_byte4_fwrite(num_modules, out);

  for(int i = 0; i < num_modules; i++) {
    hpcfmt_fstr_fwrite(src->name, out);
    hpcfmt_byte8_fwrite((uint64_t)src->vaddr, out);
    hpcfmt_byte8_fwrite((uint64_t)src->mapaddr, out);
    src = src->next;
  }
  
  return HPCFILE_OK;
}

int
hpcrun_fmt_loadmap_fread(loadmap_t *loadmap, FILE *in, alloc_fn alloc)
{
  hpcfmt_byte4_fread(&(loadmap->len), in);
  if (alloc) {
    loadmap->lst = alloc(loadmap->len * sizeof(loadmap_entry_t));
  }

  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {
    hpcfmt_fstr_fread(&(e->name), in, alloc);
    hpcfmt_byte8_fread(&(e->vaddr), in);
    hpcfmt_byte8_fread(&(e->mapaddr), in);
    // FIXME ?? (don't know what these are for yet ...
    e->flags = 0;
  }

  return HPCFILE_OK;
}

int
hpcrun_fmt_loadmap_fprint(loadmap_t *loadmap, FILE *out)
{
  loadmap_entry_t* e = loadmap->lst;
  fprintf(out, "{loadmap:\n");

  for (int i = 0; i < loadmap->len; e++,i++) {
    fprintf(out, "  lm %d: %s %"PRIx64" -> %"PRIx64"\n",
            i, e->name, e->vaddr, e->mapaddr);
  }
  fprintf(out, "}\n");
  
  return HPCFILE_OK;
}

void
hpcrun_fmt_loadmap_free(loadmap_t *loadmap, free_fn dealloc)
{
  loadmap_entry_t *e = loadmap->lst;
  for(int i = 0; i < loadmap->len; e++, i++) {
    hpcfmt_fstr_free(e->name, dealloc);
    e->name = NULL;
  }
  dealloc(loadmap->lst);
  loadmap->lst = NULL;
}




#if defined(OLD_READ)
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

  hpcfile_str_t str;
  size_t sz;
  int ret;

  if (!fs) { return HPCFILE_ERR; }

  // ----------------------------------------------------------
  // 3. LoadMaps
  // ----------------------------------------------------------

  // read number of loadmaps
  uint32_t num_loadmap;
  sz = hpcfmt_byte4_fread(&num_loadmap, fs);
  if (sz != sizeof(num_loadmap)) { 
    return HPCFILE_ERR; 
  }

  // read loadmaps 
  loadmap_tbl->num_epoch = num_loadmap; 
  loadmap_tbl->epoch_modlist = alloc_fn(num_loadmap * sizeof(epoch_entry_t));

  for (int i = 0; i < num_loadmap; ++i) {
    uint32_t num_modules;
    hpcfmt_byte4_fread(&num_modules, fs); 

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
      hpcfmt_byte8_fread(&vaddr, fs);
      hpcfmt_byte8_fread(&mapaddr, fs); 
      loadmap_tbl->epoch_modlist[i].loadmodule[j].vaddr = vaddr;
      loadmap_tbl->epoch_modlist[i].loadmodule[j].mapaddr = mapaddr;
    }
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
hpcfile_csprof_fprint(FILE* infs, FILE* outfs, char *target, hpcfile_csprof_data_t* data)
{
  int ret;

  // read loadmap
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
  hpcfile_csprof_data__fprint(data, target, outfs);

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
hpcfile_csprof_data__fprint(hpcfile_csprof_data_t* x, char *target, FILE* fs)
{
  return HPCFILE_OK;
}

#endif // defined(OLD_READ)

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
  
  x->vma_sz  = sizeof(hpcfmt_vma_t);
  x->uint_sz = sizeof(hpcfmt_uint_t);

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
  
  hpcfmt_byte4_fread(&x->as_info.bits, fs);
  hpcfmt_byte8_fread(&x->ip, fs);
  hpcfile_cstree_lip__fread(&x->real_lip, fs);
#if 0
  hpcfmt_byte8_fread(&x->lip.id, fs);
#endif
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

#if defined(OLD_LIP)
  if (! x->lip.ptr ) {
    lush_lip_init(&x->real_lip);
    x->lip.ptr = &x->real_lip;
  }
#endif
  hpcfile_cstree_lip__fwrite(&x->real_lip, fs);

#if 0
  hpcfmt_byte8_fwrite(x->lip.id, fs);
  DD("*** DD: Past lush lip write (lip id = %lu", x->lip.id);
#endif
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

  fprintf(fs, "%s{nodedata: (as: %s) (ip: 0x%"PRIx64") (lip: [%"PRIu64"][%p])\n", pre, as_str, x->ip, x->lip.id, 
	  x->lip.ptr); 
	  
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
hpcfile_cstree_lip__fprint(lush_lip_t* x, hpcfmt_uint_t id, 
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
  fprintf(fs, "{node: (id: %"PRIu32") (id_parent: %"PRIu32")}\n", 
	  x->id, x->id_parent);

  hpcfile_cstree_nodedata__fprint(&x->data, fs, pre);
  
  return HPCFILE_OK;
}

//***************************************************************************
