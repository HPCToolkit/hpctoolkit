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

#ifndef prof_lean_hpcfmt_h
#define prof_lean_hpcfmt_h

//************************* System Include Files ****************************

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>


//*************************** User Include Files ****************************

#include "hpcio.h"


//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************

enum {
  HPCFMT_OK  =  1,
  HPCFMT_ERR = -1,
  HPCFMT_EOF = -2,
};

#define HPCFMT_ThrowIfError(v) if ( (v) != HPCFMT_OK ) { return HPCFMT_ERR; }

//***************************************************************************

// Types with known sizes (for reading/writing)
typedef uint64_t hpcfmt_vma_t;
typedef uint64_t hpcfmt_uint_t;

typedef union {
  double   r8;
  uint64_t i8;
} hpcfmt_byte8_union_t;


//***************************************************************************
// Macros for defining "list of" things
//***************************************************************************

#define HPCFMT_List(t) list_of_ ## t

#define HPCFMT_ListRep(t)			\
  struct HPCFMT_List(t) {			\
    uint32_t len;				\
    t* lst;					\
  } 

#define HPCFMT_List_declare(t)			\
  typedef HPCFMT_ListRep(t) HPCFMT_List(t)


//***************************************************************************
// Memory allocator/deallocator function types
//***************************************************************************

// Allocates space for 'nbytes' bytes and returns a pointer to the
// beginning of the chuck.  'sz' should never be 0.
typedef void* hpcfmt_alloc_fn(size_t nbytes);

// Deallocates the memory chunk (allocated with the above) beginning
// at 'mem'.  Note that 'mem' may be NULL.
typedef void  hpcfmt_free_fn(void* mem);


//***************************************************************************
// Generic reader/writer primitives
//***************************************************************************

int hpcfmt_fread(void *data, size_t size, FILE *infs);

int hpcfmt_fwrite(void *data, size_t size, FILE *outfs);


static inline int
hpcfmt_int2_fread(uint16_t* val, FILE* infs)
{
  size_t sz = hpcio_be2_fread(val, infs);
  if ( sz != sizeof(uint16_t) ) {
    return (sz == 0 && feof(infs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


static inline int
hpcfmt_int4_fread(uint32_t* val, FILE* infs)
{
  size_t sz = hpcio_be4_fread(val, infs);
  if ( sz != sizeof(uint32_t) ) {
    return (sz == 0 && feof(infs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


static inline int
hpcfmt_int8_fread(uint64_t* val, FILE* infs)
{
  size_t sz = hpcio_be8_fread(val, infs);
  if ( sz != sizeof(uint64_t) ) {
    return (sz == 0 && feof(infs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


static inline int
hpcfmt_intX_fread(uint8_t* val, size_t size, FILE* infs)
{
  size_t sz = hpcio_beX_fread(val, size, infs);
  if (sz != size) {
    return (sz == 0 && feof(infs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


static inline int
hpcfmt_real8_fread(double* val, FILE* infs)
{
  size_t sz = hpcio_be8_fread((uint64_t*)val, infs);
  if ( sz != sizeof(double) ) {
    return (sz == 0 && feof(infs)) ? HPCFMT_EOF : HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


//***************************************************************************

static inline int
hpcfmt_int2_fwrite(uint16_t val, FILE* outfs)
{
  if ( sizeof(uint16_t) != hpcio_be2_fwrite(&val, outfs) ) {
    return HPCFMT_ERR;
  }
  return HPCFMT_OK;
}

static inline char*
hpcfmt_int2_swrite(uint16_t val, char* buf)
{
  return hpcio_be2_swrite(val, buf);
}


static inline int
hpcfmt_int4_fwrite(uint32_t val, FILE* outfs)
{
  if ( sizeof(uint32_t) != hpcio_be4_fwrite(&val, outfs) ) {
    return HPCFMT_ERR;
  }
  return HPCFMT_OK;
}

static inline char*
hpcfmt_int4_swrite(uint32_t val, char* buf)
{
  return hpcio_be4_swrite(val, buf);
}


static inline int
hpcfmt_int8_fwrite(uint64_t val, FILE* outfs)
{
  if ( sizeof(uint64_t) != hpcio_be8_fwrite(&val, outfs) ) {
    return HPCFMT_ERR;
  }
  return HPCFMT_OK;
}

static inline char*
hpcfmt_int8_swrite(uint64_t val, char* buf)
{
  return hpcio_be8_swrite(val, buf);
}


static inline int
hpcfmt_intX_fwrite(uint8_t* val, size_t size, FILE* outfs)
{
  if (size != hpcio_beX_fwrite(val, size, outfs)) {
    return HPCFMT_ERR;
  }
  return HPCFMT_OK;
}

static inline char*
hpcfmt_intX_swrite(uint8_t* val, size_t size, char* buf)
{
  return hpcio_beX_swrite(val, size, buf);
}


static inline int
hpcfmt_real8_fwrite(double val, FILE* outfs)
{
  // N.B.: This apparently breaks C99's "strict" anti-aliasing rules
  //uint64_t* v = (uint64_t*)(&val);
  
  hpcfmt_byte8_union_t* v = (hpcfmt_byte8_union_t*)(&val);
  if ( sizeof(double) != hpcio_be8_fwrite(&(v->i8), outfs) ) {
    return HPCFMT_ERR;
  }
  return HPCFMT_OK;
}


//***************************************************************************
// hpcfmt_str_t
//***************************************************************************

typedef struct hpcfmt_str_t {

  uint32_t len; // length of string
  char str[];   // string data. [Variable length data]

} hpcfmt_str_t;


int 
hpcfmt_str_fread(char** str, FILE* infs, hpcfmt_alloc_fn alloc);

int 
hpcfmt_str_fwrite(const char* str, FILE* outfs);

void 
hpcfmt_str_free(const char* str, hpcfmt_free_fn dealloc);


static inline const char*
hpcfmt_str_ensure(const char* x)
{
  return (x) ? x : "";
}


//***************************************************************************
// hpcfmt_nvpair_t
//***************************************************************************

typedef struct hpcfmt_nvpair_t {

  char* name;
  char* val;

} hpcfmt_nvpair_t;

int
hpcfmt_nvpair_fwrite(hpcfmt_nvpair_t* nvp, FILE* fs);

int
hpcfmt_nvpairs_vfwrite(FILE* out, va_list args);

int
hpcfmt_nvpair_fread(hpcfmt_nvpair_t* inp, FILE* infs, hpcfmt_alloc_fn alloc);

int
hpcfmt_nvpair_fprint(hpcfmt_nvpair_t* nvp, FILE* fs, const char* pre);


//***************************************************************************
// List of hpcfmt_nvpair_t 
//***************************************************************************

HPCFMT_List_declare(hpcfmt_nvpair_t);

int
hpcfmt_nvpairList_fread(HPCFMT_List(hpcfmt_nvpair_t)* nvps,
			FILE* infs, hpcfmt_alloc_fn alloc);

int
hpcfmt_nvpairList_fprint(const HPCFMT_List(hpcfmt_nvpair_t)* nvps,
			 FILE* fs, const char* pre);

const char*
hpcfmt_nvpairList_search(const HPCFMT_List(hpcfmt_nvpair_t)* lst,
			 const char* name);

void
hpcfmt_nvpairList_free(HPCFMT_List(hpcfmt_nvpair_t)* nvps,
		       hpcfmt_free_fn dealloc);


//***************************************************************************


// --------------------------------------------------------------
// additional metric information
// this data is optional in metric description.
// at the moment, only perf event sample source needs this info
// --------------------------------------------------------------
typedef struct metric_aux_info_s {

	bool   is_multiplexed;  // flag if the event is multiplexed
	double threshold_mean;  // average threshold (if multiplexed)

	uint64_t num_samples;   // number of samples

} metric_aux_info_t;

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcfmt_h */
