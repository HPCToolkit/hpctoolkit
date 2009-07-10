// -*-Mode: C++;-*- // technically C99
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

#ifndef prof_lean_hpcfmt_h
#define prof_lean_hpcfmt_h

//************************* System Include Files ****************************

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>


//*************************** User Include Files ****************************

#include "hpcio.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

  // FIXME: rename to HPCFMT_OK/ERR or just delete
enum {
  HPCFILE_OK  =  1,
  HPCFILE_ERR = -1,
  HPCFILE_EOF = -2,
};


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
typedef uint64_t hpcfmt_vma_t;
typedef uint64_t hpcfmt_uint_t;

//***************************************************************************

// Allocation callbacks for use with the library.  In order to conform
// to the requirments of the HPC profiler, this library may not assume
// that malloc/free are safe to use for managing dynamic memory.  These
// callbacks allow the user to specify how dynamic memory ought to
// allocated/freed.

typedef void* (*hpcfile_cb__alloc_fn_t)(size_t);
typedef void  (*hpcfile_cb__free_fn_t)(void*);

typedef void* alloc_fn(size_t nbytes);
typedef void  free_fn(void *mem);

#if 0 /* describe callbacks */

// Allocates space for 'sz' bytes and returns a pointer to the
// beginning of the chuck.  'sz' should never be 0.  FIXME errors
void* hpcfile_cb__alloc(size_t sz);

// Deallocates the memory chunk (allocated with the above) beginning
// at 'mem'.  Note that 'mem' may be NULL.
void  hpcfile_cb__free(void* mem);

#endif /* describe callbacks */

//***************************************************************************

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


// --------------------------------------------
// 32 & 64 bit integer quantities
// --------------------------------------------

extern int hpcfmt_byte4_fwrite(uint32_t val, FILE *out);
extern int hpcfmt_byte4_fread(uint32_t *val, FILE *in);
extern int hpcfmt_byte8_fwrite(uint64_t val, FILE *out);
extern int hpcfmt_byte8_fread(uint64_t *val, FILE *in);


// ---------------------------------------------------------
// hpcfile_str_t: An arbitrary length character string.
// ---------------------------------------------------------

typedef struct hpcfile_str_s {
  uint32_t tag;    // data/structure format tag

  uint32_t length; // length of string
  char*    str;    // string

} hpcfile_str_t;

typedef struct hpcfmt_fstr_t {

  uint32_t len; // length of string
  char str[];   // string data. [Variable length data]

} hpcfmt_fstr_t;

int hpcfile_str__init(hpcfile_str_t* x);
int hpcfile_str__fini(hpcfile_str_t* x);

int hpcfile_str__fread(hpcfile_str_t* x, FILE* fs, 
		       hpcfile_cb__alloc_fn_t alloc_fn);
int hpcfile_str__fwrite(hpcfile_str_t* x, FILE* fs);
int hpcfile_str__fprint(hpcfile_str_t* x, FILE* fs);

int hpcfmt_fstr_fread(char **str, FILE *infs, alloc_fn alloc);
int hpcfmt_fstr_fwrite(char *str, FILE *outfs);
void hpcfmt_fstr_free(char *str, free_fn dealloc);

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

//***************************************************************************
//   DBG macro -- convenience only
//

#define DD(...) if (getenv("HPC_VERB")) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");} while (0)

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcfmt_h */
