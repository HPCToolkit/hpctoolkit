// -*-Mode: C++;-*- // technically C99
// $Id$

// * BeginRiceCopyright *****************************************************
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   $Source$
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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


//*************************** User Include Files ****************************

#include "hpcio.h"

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
//
//***************************************************************************

// See header for interface information.
FILE* 
hpcio_fopen_w(const char* fnm, int overwrite)
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
hpcio_open_r(const char* fnm)
{
  FILE* fs = fopen(fnm, "r");
  return fs;
}


// See header for interface information.
int   
hpcio_close(FILE* fs)
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
hpcio_fread_le2(uint16_t* val, FILE* fs)
{
  uint16_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 0; shift < 16; shift += 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint16_t)(c & 0xff) << shift); // 0, 8
  }

  *val = v;
  return num_read;
}


size_t
hpcio_fread_le4(uint32_t* val, FILE* fs)
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
hpcio_fread_le8(uint64_t* val, FILE* fs)
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
hpcio_fwrite_le2(uint16_t* val, FILE* fs)
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
hpcio_fwrite_le4(uint32_t* val, FILE* fs)
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
hpcio_fwrite_le8(uint64_t* val, FILE* fs)
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


size_t
hpcio_fread_be2(uint16_t* val, FILE* fs)
{
  uint16_t v = 0; // local copy of val
  int shift = 0, num_read = 0, c;
  
  for (shift = 8; shift >= 0; shift -= 8) {
    if ( (c = fgetc(fs)) == EOF ) { break; }
    num_read++;
    v |= ((uint16_t)(c & 0xff) << shift); // 8, 0
  }

  *val = v;
  return num_read;
}


size_t
hpcio_fread_be4(uint32_t* val, FILE* fs)
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
hpcio_fread_be8(uint64_t* val, FILE* fs)
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
hpcio_fwrite_be2(uint16_t* val, FILE* fs)
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
hpcio_fwrite_be4(uint32_t* val, FILE* fs)
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
hpcio_fwrite_be8(uint64_t* val, FILE* fs)
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


#if 0
size_t 
hpcio_fread_be4(uint32_t* val, FILE* fs)
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
