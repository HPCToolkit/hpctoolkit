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
// Copyright ((c)) 2002-2020, Rice University
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
// Purpose:
//   Low-level types and functions for reading/writing id_tuples (each represent a unique profile)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCTOOLKIT_PROF_LEAN_ID_TUPLE_H
#define HPCTOOLKIT_PROF_LEAN_ID_TUPLE_H

//***************************************************************************
// system includes
//***************************************************************************

#include <stdbool.h>
#include <limits.h>



//***************************************************************************
// local includes
//***************************************************************************

#include <include/uint.h>

#include "../prof-lean/hpcio.h"
#include "../prof-lean/hpcio-buffer.h"
#include "../prof-lean/hpcfmt.h"
#include "../prof-lean/hpcrun-fmt.h"



//***************************************************************************
// macros
//***************************************************************************

#define IDTUPLE_INVALID        UINT16_MAX

#define IDTUPLE_SUMMARY        0
#define IDTUPLE_NODE           1
#define IDTUPLE_RANK           2
#define IDTUPLE_THREAD         3
#define IDTUPLE_GPUDEVICE      4
#define IDTUPLE_GPUCONTEXT     5
#define IDTUPLE_GPUSTREAM      6
#define IDTUPLE_CORE           7

#define IDTUPLE_MAXTYPES       8

#define PMS_id_tuple_len_SIZE  2
#define PMS_id_SIZE            10



//***************************************************************************
// types
//***************************************************************************

typedef struct pms_id_t {
  uint16_t kind;
  uint64_t index;
} pms_id_t;


typedef struct id_tuple_t {
  uint16_t length; // number of valid ids

  uint16_t ids_length; // number of id slots allocated
  pms_id_t* ids;
} id_tuple_t;


typedef void* (id_tuple_allocator_fn_t)(size_t bytes);



//***************************************************************************
// interface operations
//***************************************************************************

#if defined(__cplusplus)
extern "C" {
#endif



const char *kindStr(const uint16_t kind);


//---------------------------------------------------------------------------
// tuple initialization
//---------------------------------------------------------------------------

void
id_tuple_constructor
(
 id_tuple_t *tuple, 
 pms_id_t *ids, 
 int ids_length
);


void 
id_tuple_push_back
(
 id_tuple_t *tuple, 
 uint16_t kind, 
 uint64_t index
);


void
id_tuple_copy
(
 id_tuple_t *dest, 
 id_tuple_t *src, 
 id_tuple_allocator_fn_t alloc
);


//---------------------------------------------------------------------------
// Single id_tuple
//---------------------------------------------------------------------------

int 
id_tuple_fwrite(id_tuple_t* x, FILE* fs);

int 
id_tuple_fread(id_tuple_t* x, FILE* fs);

int 
id_tuple_fprint(id_tuple_t* x, FILE* fs);

void 
id_tuple_dump(id_tuple_t* x);

void 
id_tuple_free(id_tuple_t* x);


//---------------------------------------------------------------------------
// for thread.db (thread major sparse)
//---------------------------------------------------------------------------

int 
id_tuples_pms_fwrite(uint32_t num_tuples, id_tuple_t* x, FILE* fs);

int 
id_tuples_pms_fread(id_tuple_t** x, uint32_t num_tuples,FILE* fs);

int 
id_tuples_pms_fprint(uint32_t num_tuples,uint64_t id_tuples_size, id_tuple_t* x, FILE* fs);

void 
id_tuples_pms_free(id_tuple_t** x, uint32_t num_tuples);

//---------------------------------------------------------------------------

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //HPCTOOLKIT_PROF_LEAN_ID_TUPLE_H
