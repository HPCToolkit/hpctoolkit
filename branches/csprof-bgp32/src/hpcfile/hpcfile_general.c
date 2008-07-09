// -*-Mode: C-*-
// $Id: hpcfile_general.c 1168 2008-03-14 01:53:59Z eraxxon $

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
//    $Source$
//
// Purpose:
//    General and helper functions for reading/writing a HPC data
//    files from/to a binary file.
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

//*************************** Forward Declarations **************************

// See header for interface information.
FILE* 
hpcfile_open_for_write(const char* fnm, int overwrite)
{
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd;
  FILE* fs = NULL;

  if (overwrite == 0) {
    // Open file for writing; fail if the file already exists.  
    fd = open(fnm, O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd < 0) { return NULL; }  
  } else if (overwrite == 1) {
    // Open file for writing; truncate file already exists.
    fd = open(fnm, O_WRONLY | O_CREAT | O_TRUNC, mode); 
  } else {
    return NULL; // blech
  }
  
  // Get a buffered stream since we will be performing many small writes.
  fs = fdopen(fd, "w");
  return fs;
}

// See header for interface information.
FILE* 
hpcfile_open_for_read(const char* fnm)
{
  FILE* fs = fopen(fnm, "r");
  return fs;
}

// See header for interface information.
int   
hpcfile_close(FILE* fs)
{
  if (fs) {
    if (fclose(fs) == EOF) { 
      return HPCFILE_ERR; 
    } 
  }
  return HPCFILE_OK; 
}

//***************************************************************************

int 
hpcfile_str__init(hpcfile_str_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}

int 
hpcfile_str__fini(hpcfile_str_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_str__fread(hpcfile_str_t* x, FILE* fs, 
		   hpcfile_cb__alloc_fn_t alloc_fn)
{
  size_t sz;
  
  // [tag has already been read]

  sz = hpc_fread_le4(&x->length, fs);
  if (sz != sizeof(x->length)) { return HPCFILE_ERR; }

  if (x->length > 0) { 
    x->str = alloc_fn((x->length + 1) * 1); // add space for '\0'
    sz = fread(x->str, 1, x->length, fs);  
    x->str[x->length] = '\0'; 
    if (sz != x->length) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_str__fwrite(hpcfile_str_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(x->tag, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le4(&x->length, fs);
  if (sz != sizeof(x->length)) { return HPCFILE_ERR; }
  
  sz = fwrite(x->str, 1, x->length, fs);
  if (sz != x->length) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_str__fprint(hpcfile_str_t* x, FILE* fs)
{
  fputs("{hpcfile_str: ", fs);
  
  fprintf(fs, "(tag: %u)\n", x->tag);

  fprintf(fs, "(length: %u)", x->length);
  if (x->str) { fprintf(fs, " (str: %s)", x->str); }
  
  fputs("\n", fs);

  return HPCFILE_OK;
}

//***************************************************************************

int
hpcfile_tag__fread(uint32_t* tag, FILE* fs)
{
  size_t sz;
  
  sz = hpc_fread_le4(tag, fs);
  if (sz != sizeof(*tag)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_tag__fwrite(uint32_t tag, FILE* fs)
{
  size_t sz;

  sz = hpc_fwrite_le4(&tag, fs);
  if (sz != sizeof(tag)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_num8__init(hpcfile_num8_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}

int 
hpcfile_num8__fini(hpcfile_num8_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_num8__fread(hpcfile_num8_t* x, FILE* fs)
{
  size_t sz;
  
  // [tag has already been read]

  sz = hpc_fread_le8(&x->num, fs);
  if (sz != sizeof(x->num)) { return HPCFILE_ERR; }
    
  return HPCFILE_OK;
}

int 
hpcfile_num8__fwrite(hpcfile_num8_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(x->tag, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->num, fs);
  if (sz != sizeof(x->num)) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}

int 
hpcfile_num8__fprint(hpcfile_num8_t* x, FILE* fs)
{
  fputs("{hpcfile_num8: ", fs);

  fprintf(fs, "(tag: %u)\n", x->tag); 

  fprintf(fs, "(num: %"PRIu64")\n", x->num);

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_num8s__init(hpcfile_num8s_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}

int 
hpcfile_num8s__fini(hpcfile_num8s_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_num8s__fread(hpcfile_num8s_t* x, FILE* fs, 
		     hpcfile_cb__alloc_fn_t alloc_fn)
{
  size_t sz;
  
  // [tag has already been read]

  sz = hpc_fread_le4(&x->length, fs);
  if (sz != sizeof(x->length)) { return HPCFILE_ERR; }
  
  if (x->length > 0) {
    uint32_t i;
    x->nums = alloc_fn(x->length * sizeof(uint64_t));
    for (i = 0; i < x->length; ++i) {
      sz = hpc_fread_le8(&x->nums[i], fs);
      if (sz != sizeof(uint64_t)) { return HPCFILE_ERR; }
    }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_num8s__fwrite(hpcfile_num8s_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(x->tag, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le4(&x->length, fs);
  if (sz != sizeof(x->length)) { return HPCFILE_ERR; }
  
  if (x->length > 0) {
    uint32_t i;
    for (i = 0; i < x->length; ++i) {
      sz = hpc_fwrite_le8(&x->nums[i], fs);
      if (sz != sizeof(uint64_t)) { return HPCFILE_ERR; }
    }
  }

  return HPCFILE_OK;
}

int 
hpcfile_num8s__fprint(hpcfile_num8s_t* x, FILE* fs)
{
  fputs("{hpcfile_num8s: ", fs);

  fprintf(fs, "(tag: %u)\n", x->tag);  

  fprintf(fs, "(length: %u)", x->length);
  if (x->nums) { 
    uint32_t i;
    for (i = 0; i < x->length; ++i) {
      fprintf(fs, " (num: %"PRIu64")", x->nums[i]); 
    }
  }
  
  fputs("\n", fs);

  return HPCFILE_OK;
}

//***************************************************************************
// Read and write x bytes in little/big endian format.  See header for
// interface information.
//
// Example: Read/Write a 4 byte value into/from a program value:
//   program value:       B3 B2 B1 B0 (actual memory order depends on system)
//
//   little-endian file:  B0 B1 B2 B3
//   big-endian file:     B3 B2 B1 B0
//***************************************************************************

size_t
hpc_fread_le4(uint32_t* val, FILE* fs)
{
  uint32_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 0; shift < 32; shift += 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint32_t)(c & 0xff) << shift); // 0, 8, 16, 24
  }

  *val = v;
  return num_read;
}

size_t 
hpc_fread_le8(uint64_t* val, FILE* fs)
{
  uint64_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 0; shift < 64; shift += 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint64_t)(c & 0xff) << shift); // 0, 8, 16, 24... 56
  }

  *val = v;
  return num_read;
}

size_t 
hpc_fwrite_le4(uint32_t* val, FILE* fs)
{
  uint32_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 0; shift < 32; shift += 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}

size_t 
hpc_fwrite_le8(uint64_t* val, FILE* fs)
{
  uint64_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 0; shift < 64; shift += 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}


#if 0

size_t 
hpc_fread_be4(uint32_t* val, FILE* fs)
{
  uint32_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;

  for (shift = 24; shift >= 0; shift -= 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint32_t)(c & 0xff) << shift); // 24, 16, 8, 0
  }
 
  *val = v;
  return num_read;
}


#define BIG_ENDIAN      0
#define LITTLE_ENDIAN   1

// hpc_get_endianness: Return endianness of current architecture. 
int
hpc_get_endianness()
{
  uint16_t val = 0x0001;
  char* bite = (char*) &val;
  if (bite[0] == 0x00) {
    return BIG_ENDIAN;    // 'bite' points to most significant byte
  } else { // bite[0] == 0x01
    return LITTLE_ENDIAN; // 'bite' points to least significant byte
  }   
}

#endif
