// -*-Mode: C;-*-
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
//    hpcfile_csprof.c
//
// Purpose:
//    Low-level types and functions for reading/writing a call stack
//    profile from/to a binary file.
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
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

#include "hpcfile_general.h"
#include "hpcfile_csproflib.h"
#include "hpcfile_csprof.h"

//*************************** Forward Declarations **************************

int hpcfile_csprof_read_hdr(FILE* fs, hpcfile_csprof_hdr_t* hdr);

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
  hpcfile_csprof_hdr_t fhdr;
  hpcfile_str_t str;
  hpcfile_num8_t num8;
  int i;

  if (!fs) { return HPCFILE_ERR; }

  // Write header 
  hpcfile_csprof_hdr__init(&fhdr);
  fhdr.num_data = 3;
  if (hpcfile_csprof_hdr__fwrite(&fhdr, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Write HPCFILE_TAG__CSPROF_TARGET
  hpcfile_str__init(&str);
  str.tag = HPCFILE_TAG__CSPROF_TARGET;
  if (data->target) {
    str.length = strlen(data->target);
    str.str = data->target;
  }
  if (hpcfile_str__fwrite(&str, fs) != HPCFILE_OK) { return HPCFILE_ERR; }

  hpc_fwrite_le4(&data->num_metrics, fs);

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
                    epoch_table_t* epochtbl,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn)
{
  hpcfile_csprof_hdr_t fhdr;
  hpcfile_str_t str;
  hpcfile_num8_t num8;
  uint32_t tag;
  size_t sz;
  int ret = HPCFILE_ERR;

  if (!fs) { return HPCFILE_ERR; }

  hpcfile_csprof_data__init(data);
  
  // Read and sanity check header
  if (hpcfile_csprof_read_hdr(fs, &fhdr) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  
  // FIXME: TEMPORARY
  
  // ----------------------------------------------------------
  // 1. Target
  // ----------------------------------------------------------
  sz = hpc_fread_le4(&tag, fs); // HPCFILE_STR
  if (sz != sizeof(tag)) { return HPCFILE_ERR; }
  
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
  sz = hpc_fread_le4(&tag, fs); 
  if (sz != sizeof(tag)) { return HPCFILE_ERR; }

  data->num_metrics = tag; // FIXME: YUCK

  // 2b. metric descriptions
  data->metrics = alloc_fn(data->num_metrics * sizeof(hpcfile_csprof_metric_t));
  
  for (uint32_t i = 0; i < data->num_metrics; ++i) {
    // Read metrics data tag
    sz = hpc_fread_le4(&tag, fs);
    if (sz != sizeof(tag)) { return HPCFILE_ERR; }

    // read in the name of the  metric
    str.str = NULL;
    if (hpcfile_str__fread(&str, fs, alloc_fn) != HPCFILE_OK) {
      free_fn(str.str);
      return HPCFILE_ERR;
    }
    data->metrics[i].metric_name = str.str;
	  
    // read in the flags of the metric
    sz = hpc_fread_le4(&tag, fs);
    if (sz != sizeof(tag)) { return HPCFILE_ERR; }
    if (hpcfile_num8__fread(&num8, fs) != HPCFILE_OK) {
      return HPCFILE_ERR;
    }
    data->metrics[i].flags = num8.num;
	  
    // read in the sample period
    sz = hpc_fread_le4(&tag, fs);
    if (sz != sizeof(tag)) { return HPCFILE_ERR; }
    if (hpcfile_num8__fread(&num8, fs) != HPCFILE_OK) {
      return HPCFILE_ERR;
    }
	  
    data->metrics[i].sample_period = num8.num;
  }


#if 0  
  // Read data chunks (except epoch)
  for (i = 0; i < fhdr.num_data-1; ++i) {  
    // Read data tag
    sz = hpc_fread_le4(&tag, fs);
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
#endif
    
    // Interpret the data: FIXME sanity check
#if 0 
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
    

  // processing the epoch part here to reach the trees part--FMZ
  {
    uint32_t num_epoch;
    uint32_t num_modules;
    uint32_t num_trees;
    uint64_t tsamps;
    //size_t namelen;
    //char module_name[256];
    int i,jj;
    uint64_t vaddr,mapaddr;
    //read number of epochs
    hpc_fread_le4(&num_epoch, fs);
    //read in all epochs 
    epochtbl->num_epoch = num_epoch; 
    epochtbl->epoch_modlist=alloc_fn(num_epoch*sizeof(epoch_entry_t));
    for (i=0;i<num_epoch;i++) {
      hpc_fread_le4(&num_modules, fs); 
      epochtbl->epoch_modlist[i].num_loadmodule=num_modules; 
      epochtbl->epoch_modlist[i].loadmodule=alloc_fn(num_modules*sizeof(ldmodule_t));
      for (jj=0; jj<num_modules; jj++) { 
	str.str=NULL; 
	if (hpcfile_str__fread(&str, fs, alloc_fn) != HPCFILE_OK) {
	  free_fn(str.str);
	  return HPCFILE_ERR;
	} 
	epochtbl->epoch_modlist[i].loadmodule[jj].name=str.str;
	hpc_fread_le8(&vaddr, fs);
	hpc_fread_le8(&mapaddr, fs); 
	epochtbl->epoch_modlist[i].loadmodule[jj].vaddr=vaddr;
	epochtbl->epoch_modlist[i].loadmodule[jj].mapaddr=mapaddr;
      } //read in modules
    }//read in epoch
    
    //skip some info before into the trees
    hpc_fread_le4(&num_trees,fs);
    hpc_fread_le8(&tsamps,fs); 
    
  } // processing epoch table
  
  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  
  // Cleanup after error
  // cleanup_after_error:

  return ret;
}

// See header file for documentation of public interface.
int
hpcfile_csprof_fprint(FILE* infs, FILE* outfs, hpcfile_csprof_data_t* data)
{
#if 0
  hpcfile_csprof_hdr_t fhdr;
  // Read header...
  
  // Print header
  hpcfile_csprof_hdr__fprint(&fhdr, outfs);
  fputs("\n", outfs);

  // Read and print each data chunk...
#endif

  epoch_table_t epochtbl;

  hpcfile_csprof_read(infs, data, &epochtbl, 
		      (hpcfile_cb__alloc_fn_t)malloc,
		      (hpcfile_cb__free_fn_t)free);

  for (int i = 0; i < epochtbl.num_epoch; ++i) {
    // print an epoch
    epoch_entry_t* epoch = & epochtbl.epoch_modlist[i];
    fprintf(outfs, "{epoch %d:\n", i);

    for (int j = 0; j < epoch->num_loadmodule; ++j) {
      ldmodule_t* lm = & epoch->loadmodule[j];

      fprintf(outfs, "  lm %d: %s %"PRIx64" -> %"PRIx64"\n",
	      j, lm->name, lm->vaddr, lm->mapaddr);
      
      free(lm->name);
    }
    fprintf(outfs, "}\n");

    free(epoch->loadmodule);
  }
  free(epochtbl.epoch_modlist);


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
  
  sz = hpc_fread_le8(&x->num_data, fs);
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
  
  sz = hpc_fwrite_le8(&x->num_data, fs);
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
  int i;

  fputs("# csprof_data:\n", fs);
  
  if (x->target) { fprintf(fs, "# target: %s\n", x->target); }
  fprintf(fs, "# metrics: %d\n", x->num_metrics);
  
  for(i = 0; i < x->num_metrics; ++i) {
    hpcfile_csprof_metric_t metric = x->metrics[i];
    
    if (metric.metric_name) { 
      fprintf(fs, "# event: %s\n", metric.metric_name); 
    }
    fprintf(fs, "# sample period: %"PRIu64"\n", metric.sample_period); 
  }
  
  return HPCFILE_OK;
}

//***************************************************************************

