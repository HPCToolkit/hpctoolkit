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
// Copyright ((c)) 2002-2022, Rice University
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
//   General and helper functions for reading/writing a HPC data
//   files from/to a binary file.
//
//   These routines *must not* allocate dynamic memory; if such memory
//   is needed, callbacks to the user's allocator should be used.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


//*************************** User Include Files ****************************

#include "hpcfmt.h"
#include "hpcfmt2.h"
#include "hpcio.h"
#include "hpcio2.h"
#include "hpctio_obj.h"

#include <include/uint.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// hpcfmt_str_t
//***************************************************************************

int
hpcfmt_str_fread2(char** str, FILE* infs, hpcfmt_alloc_fn alloc)
{
  uint32_t len;
  char* buf = NULL;

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&len, infs));
  if (alloc) {
    buf = (char*) alloc(len+1);
  }
  if (!buf) {
    return HPCFMT_ERR;
  }
  
  for (int i = 0; i < len; i++) {
    int c = fgetc(infs);
    if (c == EOF) {
      return HPCFMT_ERR;
    }
    buf[i] = (char) c;
  }
  buf[len] = '\0';

  *str = buf;
  return HPCFMT_OK;
}


int
hpcfmt_str_fwrite2(const char* str, hpctio_obj_t* out)
{
  uint32_t len = (str) ? strlen(str) : 0;

  hpcfmt_int4_fwrite2(len, out);
  
  for (int i = 0; i < len; i++) {
    int c = hpctio_obj_append(&(str[i]), 1, 1, out);
    if (c == -1) {
      return HPCFMT_ERR;
    }
  }
  return HPCFMT_OK;
}


void
hpcfmt_str_free2(const char* str, hpcfmt_free_fn dealloc)
{
  dealloc((void *)str);
}


//***************************************************************************
// generic read and write
//***************************************************************************


int
hpcfmt_fread2(void *data, size_t size, FILE *infs)
{
	size_t bytes = fread(data, sizeof(char), size, infs);
	if (bytes == size) {
			return HPCFMT_OK;
	}
	return HPCFMT_ERR;
}


int
hpcfmt_fwrite2(void *data, size_t size, hpctio_obj_t *out)
{
  size_t bytes = hpctio_obj_append(data, sizeof(char), size, out);
  if (bytes == size)
  	return HPCFMT_OK;
  return HPCFMT_ERR;
}


//***************************************************************************
//
//***************************************************************************

int
hpcfmt_nvpair_fwrite2(hpcfmt_nvpair_t* nvp, FILE* fs)
{
  int ret;

  ret = hpcfmt_str_fwrite(nvp->name, fs);
  if (ret != HPCFMT_OK) { 
    return HPCFMT_ERR;
  }
  
  ret = hpcfmt_str_fwrite(nvp->val, fs);
  if (ret != HPCFMT_OK) { 
    return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}


int
hpcfmt_nvpairs_vfwrite2(hpctio_obj_t* out, va_list args)
{
  va_list _tmp;
  va_copy(_tmp, args);

  uint32_t len = 0;

  for (char* arg = va_arg(_tmp, char*); arg != NULL; 
       arg = va_arg(_tmp, char*)) {
    arg = va_arg(_tmp, char*);
    len++;
  }
  va_end(_tmp);

  hpcfmt_int4_fwrite2(len, out);

  for (char* arg = va_arg(args, char*); arg != NULL; 
       arg = va_arg(args, char*)) {
    hpcfmt_str_fwrite2(arg, out); // write NAME
    arg = va_arg(args, char*);
    hpcfmt_str_fwrite2(arg, out); // write VALUE
  }
  
  return HPCFMT_OK;
}


int
hpcfmt_nvpair_fread2(hpcfmt_nvpair_t* inp, FILE* infs, hpcfmt_alloc_fn alloc)
{
  hpcfmt_str_fread(&(inp->name), infs, alloc);
  hpcfmt_str_fread(&(inp->val), infs, alloc);

  return HPCFMT_OK;
}


int
hpcfmt_nvpair_fprint2(hpcfmt_nvpair_t* nvp, FILE* fs, const char* pre)
{
  fprintf(fs, "%s[nv-pair: '%s', '%s']\n", pre, nvp->name, nvp->val);
  return HPCFMT_OK;
}


//***************************************************************************
// 
//***************************************************************************

int
hpcfmt_nvpairList_fread2(HPCFMT_List(hpcfmt_nvpair_t)* nvps,
			FILE* infs, hpcfmt_alloc_fn alloc)
{
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(nvps->len), infs));
  if (alloc != NULL) {
    nvps->lst = (hpcfmt_nvpair_t*) alloc(nvps->len * sizeof(hpcfmt_nvpair_t));
  }
  for (uint32_t i = 0; i < nvps->len; i++) {
    hpcfmt_nvpair_fread(&nvps->lst[i], infs, alloc);
  }
  return HPCFMT_OK;
}


int
hpcfmt_nvpairList_fprint2(const HPCFMT_List(hpcfmt_nvpair_t)* nvps,
			 FILE* fs, const char* pre)
{
  for (uint32_t i = 0; i < nvps->len; ++i) {
    hpcfmt_nvpair_fprint(&nvps->lst[i], fs, pre);
  }
  return HPCFMT_OK;
}


const char*
hpcfmt_nvpairList_search2(const HPCFMT_List(hpcfmt_nvpair_t)* nvps,
			 const char* name)
{
  for (uint32_t i = 0; i < nvps->len; ++i) {
    if (strcmp(nvps->lst[i].name, name) == 0) {
      return nvps->lst[i].val;
    }
  }
  return NULL;
}


void
hpcfmt_nvpairList_free2(HPCFMT_List(hpcfmt_nvpair_t)* nvps,
		       hpcfmt_free_fn dealloc)
{
  for (uint32_t i = 0; i < nvps->len; ++i) {
    dealloc(nvps->lst[i].name);
    dealloc(nvps->lst[i].val);
  }
  dealloc(nvps->lst);
}
