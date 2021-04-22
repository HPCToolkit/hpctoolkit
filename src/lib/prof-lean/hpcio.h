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

#ifndef prof_lean_hpcio_h
#define prof_lean_hpcio_h

//************************* System Include Files ****************************

#include <stdio.h>
#include <inttypes.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************

// Note: use #defines to (portably) avoid warnings about unused
// "static const char*" variables.
#define HPCIO_RWBufferSz (4 * 1024 * 1024)

//***************************************************************************

// Open and close a file stream for use with the library's read/write
// routines. 
//
// hpcio_fopen_w, hpcio_open_r: Opens the file pointed to by 'fnm' and
// returns a file stream for buffered I/O.  For writing, if
// 'overwrite' is 0, it is an error for the file to already exist; if
// 'overwrite' is 1 any existing file will be overwritten.  For
// reading, it is an error if the file does not exist.  For any of
// these errors, or other open errors, NULL is returned; otherwise a
// non-null FILE pointer is returned.
//
// hpcio_close: Close the file stream.  Returns 0 upon success; 
// non-zero on error.
FILE*
hpcio_fopen_w(const char* fnm, int overwrite);

FILE*
hpcio_fopen_r(const char* fnm);

FILE*
hpcio_fopen_rw(const char* fnm);

int
hpcio_fclose(FILE* fs);


//***************************************************************************

// hpcio_leX_fread: Reads 'X' number of little-endian bytes from the file
// stream 'fs', correctly orders them for the current architecture,
// and stores the result in 'val'. Returns the number of bytes read.

size_t
hpcio_le2_fread(uint16_t* val, FILE* fs);

size_t
hpcio_le4_fread(uint32_t* val, FILE* fs);

size_t
hpcio_le8_fread(uint64_t* val, FILE* fs);


size_t
hpcio_le2_fwrite(uint16_t* val, FILE* fs);

size_t
hpcio_le4_fwrite(uint32_t* val, FILE* fs);

size_t
hpcio_le8_fwrite(uint64_t* val, FILE* fs);


//***************************************************************************

// hpcio_beX_fwrite: Write 'X' number of bytes from 'val' to the
// big-endian file stream 'fs', correctly ordering the bytes before
// writing.  Returns the number of bytes written.

size_t
hpcio_be2_fread(uint16_t* val, FILE* fs);

size_t
hpcio_be4_fread(uint32_t* val, FILE* fs);

size_t
hpcio_be8_fread(uint64_t* val, FILE* fs);

size_t
hpcio_beX_fread(uint8_t* val, size_t size, FILE* fs);


size_t
hpcio_be2_fwrite(uint16_t* val, FILE* fs);

size_t
hpcio_be4_fwrite(uint32_t* val, FILE* fs);

size_t
hpcio_be8_fwrite(uint64_t* val, FILE* fs);

size_t
hpcio_beX_fwrite(uint8_t* val, size_t size, FILE* fs);


//***************************************************************************

// hpcio_beX_swrite: Write 'X' number of bytes from 'val' to the
// big-endian buffer 'buf', correctly ordering the bytes before
// writing.  Returns an offset to the next byte after those written.

char*
hpcio_be2_sread(uint16_t val, char* buf);

char*
hpcio_be4_sread(uint32_t val, char* buf);

char*
hpcio_be8_sread(uint64_t val, char* buf);

char*
hpcio_beX_sread(uint8_t* val, size_t size, char* buf);


char*
hpcio_be2_swrite(uint16_t val, char* buf);

char*
hpcio_be4_swrite(uint32_t val, char* buf);

char*
hpcio_be8_swrite(uint64_t val, char* buf);

char*
hpcio_beX_swrite(uint8_t* val, size_t size, char* buf);


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcio_h */
