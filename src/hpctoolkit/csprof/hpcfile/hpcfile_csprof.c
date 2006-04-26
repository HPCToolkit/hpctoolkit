// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
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

  for(i = 0; i < data->num_metrics; ++i) {
      hpcfile_csprof_metric_t metric = data->metrics[i];

      hpcfile_str__init(&str);
      str.tag = HPCFILE_TAG__CSPROF_EVENT;
      str.length = strlen(metric.metric_name);
      str.str = metric.metric_name;
      if(hpcfile_str__fwrite(&str, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
  
      hpcfile_num8__init(&num8);
      num8.tag = HPCFILE_TAG__CSPROF_METRIC_FLAGS;
      num8.num = (uint64_t) metric.metric_flags;
      if (hpcfile_num8__fwrite(&num8, fs) != HPCFILE_OK) { return HPCFILE_ERR; }

      printf("wrote metric flags\n");
      num8.tag = HPCFILE_TAG__CSPROF_PERIOD;
      num8.num = metric.sample_period;
      if (hpcfile_num8__fwrite(&num8, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
  }

  return HPCFILE_OK;
}

//***************************************************************************
// hpcfile_csprof_read()
//***************************************************************************

#if 0
// See header file for documentation of public interface.
// Cf. 'HPC_CSPROF format details' above.
int
hpcfile_csprof_read(FILE* fs, hpcfile_csprof_data_t* data, 
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn)
{
  hpcfile_csprof_hdr_t fhdr;
  hpcfile_str_t str;
  hpcfile_num8_t num8;
  uint64_t i;
  uint32_t tag;
  size_t sz;
  int ret = HPCFILE_ERR;

  if (!fs) { return HPCFILE_ERR; }

  hpcfile_csprof_data__init(data);
  
  // Read and sanity check header
  if (hpcfile_csprof_read_hdr(fs, &fhdr) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Read data chunks
  for (i = 0; i < fhdr.num_data; ++i) {

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
	break;
      case HPCFILE_NUM8:
	if (hpcfile_num8__fread(&num8, fs) != HPCFILE_OK) { 
	  return HPCFILE_ERR;
	}
#if 0
      case HPCFILE_NUM8S:
	num8s.nums = NULL;
	if (hpcfile_num8s__fread(&num8s, fs, alloc_fn) != HPCFILE_OK) { 
	  free_fn(num8s.nums);
	  return HPCFILE_ERR;
	}
#endif
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

  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  
  // Cleanup after error
  // cleanup_after_error:

  return ret;
}
#endif

// See header file for documentation of public interface.
int
hpcfile_csprof_convert_to_txt(FILE* infs, FILE* outfs)
{
  hpcfile_csprof_hdr_t fhdr;
  
  // Open file for reading; read and sanity check header
  if (hpcfile_csprof_read_hdr(infs, &fhdr) != HPCFILE_OK) {
    fputs("** Error reading header **\n", outfs);
    return HPCFILE_ERR;
  }
  
  // Print header
  hpcfile_csprof_hdr__fprint(&fhdr, outfs);
  fputs("\n", outfs);

  // Read and print each data chunk
  // --FIXME--

  
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
  
  if ((c = fgetc(fs)) == EOF) { return HPCFILE_ERR; }
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

  fprintf(fs, "(num_data: %lu)}\n", x->num_data);
  
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

        if (metric.metric_name) { fprintf(fs, "# event: %s\n", metric.metric_name); }
        fprintf(fs, "# sample period: %lu\n", metric.sample_period); 
    }

    return HPCFILE_OK;
}

//***************************************************************************

