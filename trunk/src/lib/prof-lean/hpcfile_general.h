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

#ifndef HPCFILE_GENERAL_H
#define HPCFILE_GENERAL_H

//************************* System Include Files ****************************

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h> /* commonly available, unlike <stdint.h> */

//*************************** User Include Files ****************************

#include <include/general.h> /* special printf format strings */
#include "io.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

// Common function status values
#define HPCFILE_OK   1
#define HPCFILE_ERR -1


// The library should generally be very quiet; these are for
// catastrophic circumstances...

// HPCFILE_ERRMSG(format_string, args): prints an error message along
// with source file location.
#define HPCFILE_ERRMSG(...)                                            \
  { fprintf(stderr, "hpcfile error [%s:%d]: ", __FILE__, __LINE__);    \
    fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

// HPCFILE_DIE(format_str, format_str_args): prints an error message
// and exits.
#define HPCFILE_DIE(...) HPCFILE_ERRMSG(__VA_ARGS__); { exit(1); }


// Types with known sizes (for reading/writing)
typedef uint64_t hpcfile_vma_t;
typedef uint64_t hpcfile_uint_t;

//***************************************************************************

// Allocation callbacks for use with the library.  In order to conform
// to the requirments of the HPC profiler, this library may not assume
// that malloc/free are safe to use for managing dynamic memory.  These
// callbacks allow the user to specify how dynamic memory ought to
// allocated/freed.

typedef void* (*hpcfile_cb__alloc_fn_t)(size_t);
typedef void  (*hpcfile_cb__free_fn_t)(void*);

#if 0 /* describe callbacks */

// Allocates space for 'sz' bytes and returns a pointer to the
// beginning of the chuck.  'sz' should never be 0.  FIXME errors
void* hpcfile_cb__alloc(size_t sz);

// Deallocates the memory chunk (allocated with the above) beginning
// at 'mem'.  Note that 'mem' may be NULL.
void  hpcfile_cb__free(void* mem);

#endif /* describe callbacks */

//***************************************************************************

// Open and close a file stream for use with the library's read/write
// routines. 
//
// hpcfile_open_for_write, hpcfile_open_for_read: Opens the file
// pointed to by 'fnm' and returns a file stream for buffered I/O.
// For writing, if 'overwrite' is 0, it is an error for the file to
// already exist; if 'overwrite' is 1 any existing file will be
// overwritten.  For reading, it is an error if the file does not
// exist.  For any of these errors, or other open errors, NULL is
// returned; otherwise a non-null FILE pointer is returned.
//
// hpcfile_close: Close the file stream.  Returns HPCFILE_OK upon
// success; HPCFILE_ERR on error.
FILE* hpcfile_open_for_write(const char* fnm, int overwrite);
FILE* hpcfile_open_for_read(const char* fnm);
int   hpcfile_close(FILE* fs);

//***************************************************************************
//
// Data chunk descriptors for file data that is of uncertain existance
// or of variable length.  Each chunk begins with a tag to 1) identify
// the basic data format (or structure) and 2) specify the
// interpretation of the data.  Consequently, strint data order is not
// necessary and the file can be easily extended and easily parsed.
//
//***************************************************************************

// ---------------------------------------------------------
// Data chunk tags:
//
// Tag format:
// [ data tag: 28 bits | format tag: 4 bits ]
//
// ---------------------------------------------------------

// Get the format part of the tag
#define HPCFILE_TAG_GET_FORMAT(tag) ((tag) & 0xf)

// Creates a tag value (not intended for public use)
#define HPCTAG(dtag, ftag) (((dtag) << 4) | ((ftag) & 0xf))

// Format Tags
#define HPCFILE_STR    0 /* hpcfile_str_t */
#define HPCFILE_NUM8   1 /* hpcfile_num8_t */
#define HPCFILE_NUM8S  2 /* hpcfile_num8s_t */

// CSPROF target name 
#define HPCFILE_TAG__CSPROF_TARGET   HPCTAG(0, HPCFILE_STR)
// CSPROF DLL name
#define HPCFILE_TAG__CSPROF_DLL      HPCTAG(1, HPCFILE_STR)
// CSPROF DLL load address
#define HPCFILE_TAG__CSPROF_DLL_ADDR HPCTAG(2, HPCFILE_NUM8S)
// CSPROF sample event
#define HPCFILE_TAG__CSPROF_EVENT    HPCTAG(3, HPCFILE_STR)
// CSPROF sample period
#define HPCFILE_TAG__CSPROF_PERIOD   HPCTAG(4, HPCFILE_NUM8)
// CSPROF metric flags
#define HPCFILE_TAG__CSPROF_METRIC_FLAGS  HPCTAG(5, HPCFILE_NUM8) 


int hpcfile_tag__fread(uint32_t* tag, FILE* fs);
int hpcfile_tag__fwrite(uint32_t tag, FILE* fs);


// ---------------------------------------------------------
// hpcfile_str_t: An arbitrary length character string.
// ---------------------------------------------------------
typedef struct hpcfile_str_s {
  uint32_t tag;    // data/structure format tag

  uint32_t length; // length of string
  char*    str;    // string

} hpcfile_str_t;

int hpcfile_str__init(hpcfile_str_t* x);
int hpcfile_str__fini(hpcfile_str_t* x);

int hpcfile_str__fread(hpcfile_str_t* x, FILE* fs, 
		       hpcfile_cb__alloc_fn_t alloc_fn);
int hpcfile_str__fwrite(hpcfile_str_t* x, FILE* fs);
int hpcfile_str__fprint(hpcfile_str_t* x, FILE* fs);

// ---------------------------------------------------------
// hpcfile_num8_t: One 8 byte number.
// ---------------------------------------------------------
typedef struct hpcfile_num8_s {
  uint32_t tag; // data/structure format tag
  
  uint64_t num; // some sort of 8 byte numbers

} hpcfile_num8_t;

int hpcfile_num8__init(hpcfile_num8_t* x);
int hpcfile_num8__fini(hpcfile_num8_t* x);

int hpcfile_num8__fread(hpcfile_num8_t* x, FILE* fs);
int hpcfile_num8__fwrite(hpcfile_num8_t* x, FILE* fs);
int hpcfile_num8__fprint(hpcfile_num8_t* x, FILE* fs);

// ---------------------------------------------------------
// hpcfile_num8s_t: An arbitrary length string of 8 byte numbers.
// ---------------------------------------------------------
typedef struct hpcfile_num8s_s {
  uint32_t tag; // data/structure format tag
  
  uint32_t  length; // length of string
  uint64_t* nums;   // string of some sort of 8 byte numbers

} hpcfile_num8s_t;

int hpcfile_num8s__init(hpcfile_num8s_t* x);
int hpcfile_num8s__fini(hpcfile_num8s_t* x);

int hpcfile_num8s__fread(hpcfile_num8s_t* x, FILE* fs,
			 hpcfile_cb__alloc_fn_t alloc_fn);
int hpcfile_num8s__fwrite(hpcfile_num8s_t* x, FILE* fs);
int hpcfile_num8s__fprint(hpcfile_num8s_t* x, FILE* fs);

//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif
