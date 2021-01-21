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

#ifndef _POSIX_SOURCE
#  define _POSIX_SOURCE // fdopen()
#endif

// quiets the warning about _SVID_SOURCE being deprecated
// why are any of these flags necessary ?
#ifndef _DEFAULT_SOURCE
#  define _DEFAULT_SOURCE
#endif

#ifndef _SVID_SOURCE
#  define _SVID_SOURCE  // fputc_unlocked()
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // fdopen(), fputc_unlocked()

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


//*************************** User Include Files ****************************

#include "hpcio.h"



//*************************** Forward Declarations **************************



//***************************************************************************
// interface operations
//***************************************************************************

// See header for interface information.
FILE*
hpcio_fopen_w(const char* fnm, int overwrite)
{
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd;
  FILE* fs = NULL; // default return value

  if (overwrite == 0) {
    // Open file for writing; fail if the file already exists.  
    fd = open(fnm, O_WRONLY | O_CREAT | O_EXCL, mode);
  }
  else if (overwrite == 1) {
    // Open file for writing; truncate if the file already exists.
    fd = open(fnm, O_WRONLY | O_CREAT | O_TRUNC, mode);
  }
  else if (overwrite == 2) {
    // Options specific to /dev/null.
    fd = open(fnm, O_WRONLY);
  }
  else {
    return NULL; // blech
  }

  if (fd != -1 ) {
    // open succeeded. create a buffered stream since we 
    // will perform many small writes.
    fs = fdopen(fd, "w");
  }

  return fs;
}


// See header for interface information.
FILE*
hpcio_fopen_r(const char* fnm)
{
  FILE* fs = fopen(fnm, "r");
  return fs;
}


// See header for interface information.
FILE*
hpcio_fopen_rw(const char* fnm)
{
  FILE* fs = fopen(fnm, "r+");
  return fs;
}


// See header for interface information.
int
hpcio_fclose(FILE* fs)
{
  if (fs) {
    if (fclose(fs) == EOF) { 
      return 1;
    }
  }
  return 0;
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
hpcio_le2_fread(uint16_t* val, FILE* fs)
{
  uint16_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 0; shift < 16; shift += 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v = (uint16_t)(v | ((c & 0xff) << shift)); // 0, 8
  }

  *val = v;
  return num_read;
}


size_t
hpcio_le4_fread(uint32_t* val, FILE* fs)
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
hpcio_le8_fread(uint64_t* val, FILE* fs)
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


//***************************************************************************

size_t
hpcio_le2_fwrite(uint16_t* val, FILE* fs)
{
  uint16_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 0; shift < 16; shift += 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}


size_t
hpcio_le4_fwrite(uint32_t* val, FILE* fs)
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
hpcio_le8_fwrite(uint64_t* val, FILE* fs)
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


//***************************************************************************
// Big endian
//***************************************************************************

size_t
hpcio_be2_fread(uint16_t* val, FILE* fs)
{
  uint16_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 8; shift >= 0; shift -= 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v = (uint16_t)(v | ((c & 0xff) << shift)); // 8, 0
  }

  *val = v;
  return num_read;
}


size_t
hpcio_be4_fread(uint32_t* val, FILE* fs)
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


size_t
hpcio_be8_fread(uint64_t* val, FILE* fs)
{
  uint64_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 56; shift >= 0; shift -= 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint64_t)(c & 0xff) << shift); // 56, 48, 40, ... 0
  }

  *val = v;
  return num_read;
}


size_t
hpcio_beX_fread(uint8_t* val, size_t size, FILE* fs)
{
  size_t num_read = 0;

  for (uint i = 0; i < size; ++i) {
    int c = fgetc(fs);
    if (c == EOF) {
      break;
    }
    val[i] = (uint8_t) c;
    num_read++;
  }

  return num_read;
}


//***************************************************************************

size_t
hpcio_be2_fwrite(uint16_t* val, FILE* fs)
{
  uint16_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 8; shift >= 0; shift -= 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}


size_t
hpcio_be4_fwrite(uint32_t* val, FILE* fs)
{
  uint32_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 24; shift >= 0; shift -= 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}


size_t
hpcio_be8_fwrite(uint64_t* val, FILE* fs)
{
  uint64_t v = *val; // local copy of val
  int shift = 0, num_write = 0, c;
  
  for (shift = 56; shift >= 0; shift -= 8) {
    c = fputc( ((v >> shift) & 0xff) , fs);
    if (c == EOF) { break; }
    num_write++;
  }
  return num_write;
}


size_t
hpcio_beX_fwrite(uint8_t* val, size_t size, FILE* fs)
{
  size_t num_write = 0;
  
  for (uint i = 0; i < size; ++i) {
    int c = fputc(val[i], fs);
    if (c == EOF) {
      break;
    }
    num_write++;
  }

  return num_write;
}


char*
hpcio_be2_swrite(uint16_t val, char* buf)
{
  for (int shift = 8; shift >= 0; shift -= 8) {
    *(buf++) = (val >> shift) & 0xff;
  }
  return buf;
}


char*
hpcio_be4_swrite(uint32_t val, char* buf)
{
  for (int shift = 24; shift >= 0; shift -= 8) {
    *(buf++) = (val >> shift) & 0xff;
  }
  return buf;
}


char*
hpcio_be8_swrite(uint64_t val, char* buf)
{
  for (int shift = 56; shift >= 0; shift -= 8) {
    *(buf++) = (val >> shift) & 0xff;
  }
  return buf;
}


//***************************************************************************
//
//***************************************************************************

#if 0
#define BIG_ENDIAN      0
#define LITTLE_ENDIAN   1

// hpcio_get_endianness: Return endianness of current architecture. 
int
hpcio_get_endianness()
{
  uint16_t val = 0x0001;
  char* bite = (char*) &val;
  if (bite[0] == 0x00) {
    return BIG_ENDIAN;    // 'bite' points to most significant byte
  }
  else { // bite[0] == 0x01
    return LITTLE_ENDIAN; // 'bite' points to least significant byte
  }   
}
#endif
