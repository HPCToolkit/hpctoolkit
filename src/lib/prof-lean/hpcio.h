// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

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
FILE* hpcio_fopen_w(const char* fnm, int overwrite);
FILE* hpcio_open_r(const char* fnm);
int   hpcio_close(FILE* fs);


//***************************************************************************

// hpcio_fread_leX: Reads 'X' number of little-endian bytes from the file
// stream 'fs', correctly orders them for the current architecture,
// and stores the result in 'val'. Returns the number of bytes read.

size_t hpcio_fread_le2(uint16_t* val, FILE* fs);
size_t hpcio_fread_le4(uint32_t* val, FILE* fs);
size_t hpcio_fread_le8(uint64_t* val, FILE* fs);
size_t hpcio_fwrite_le2(uint16_t* val, FILE* fs);
size_t hpcio_fwrite_le4(uint32_t* val, FILE* fs);
size_t hpcio_fwrite_le8(uint64_t* val, FILE* fs);


// hpcio_fwrite_beX: Write 'X' number of bytes from 'val' to the
// big-endian file stream 'fs', correctly ordering the bytes before
// writing.  Returns the number of bytes written.

size_t hpcio_fread_be2(uint16_t* val, FILE* fs);
size_t hpcio_fread_be4(uint32_t* val, FILE* fs);
size_t hpcio_fread_be8(uint64_t* val, FILE* fs);
size_t hpcio_fwrite_be2(uint16_t* val, FILE* fs);
size_t hpcio_fwrite_be4(uint32_t* val, FILE* fs);
size_t hpcio_fwrite_be8(uint64_t* val, FILE* fs);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcio_h */
